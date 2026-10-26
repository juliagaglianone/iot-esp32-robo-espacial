// ESP32 de monitoramento (WROOM-32): recebe via ESPNOW, envia à API e controla LEDs/telas
// LED azul = recebendo dados | LED amarelo = timeout de comunicação

#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>
#include <time.h>

// ===== CONFIGURAÇÃO =====
// Wi-Fi, URL da API e MAC da S3 ficam em secrets.h (copie de secrets.example.h)
#include "secrets.h"

// ===== PINOS E TEMPOS =====
#define LED_AZUL            25
#define LED_AMARELO         26
#define TIMEOUT_MS          5000
#define INTERVALO_TELA_MS   2000

typedef struct __attribute__((packed)) {
  uint8_t  nivel;
  float    temperatura;
  float    umidade;
  int16_t  luminosidade;
  uint8_t  presenca;
  uint32_t timestamp;   // ms do coletor (o timestamp ISO é gerado aqui, via NTP)
} Pacote;

Pacote recebido;                  // escrito pelo callback
Pacote exibido;                   // usado pelas telas
volatile bool novoPacote = false;
volatile unsigned long ultimoRx = 0;
bool temDados = false;
int estadoLed = -1;               // 1 = ok, 0 = timeout
int telaAtual = 0;
unsigned long ultimaTela = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Callback de recepção (a assinatura muda entre as versões do core ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void aoReceber(const esp_now_recv_info_t *info, const uint8_t *dados, int tam) {
  const uint8_t *mac = info->src_addr;
#else
void aoReceber(const uint8_t *mac, const uint8_t *dados, int tam) {
#endif
  if (memcmp(mac, MAC_COLETOR, 6) != 0) return;     // só aceita o coletor autorizado
  if (tam != sizeof(Pacote)) return;
  Pacote p;
  memcpy(&p, dados, sizeof(p));
  if (p.nivel > 100 || p.presenca > 1 || p.luminosidade < 0 || p.luminosidade > 4095) return;  // valida
  portENTER_CRITICAL(&mux);
  recebido = p;
  novoPacote = true;
  ultimoRx = millis();
  portEXIT_CRITICAL(&mux);
}

String timestampISO() {
  time_t agora = time(nullptr);
  if (agora < 1700000000) return "";               // NTP ainda não sincronizou
  struct tm t;
  gmtime_r(&agora, &t);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
  return String(buf);
}

void enviarParaAPI(const Pacote &p, const String &ts) {
  if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); Serial.println("API: sem Wi-Fi"); return; }
  String j = "{";
  j += "\"nivel\":" + String(p.nivel) + ",";
  j += "\"temperatura\":" + (isnan(p.temperatura) ? String("null") : String(p.temperatura, 1)) + ",";
  j += "\"umidade\":" + (isnan(p.umidade) ? String("null") : String(p.umidade, 1)) + ",";
  j += "\"luminosidade\":" + String(p.luminosidade) + ",";
  j += "\"presenca\":" + String(p.presenca);
  if (ts.length()) j += ",\"timestamp\":\"" + ts + "\"";
  j += "}";

  WiFiClient cliente;
  HTTPClient http;
  http.setConnectTimeout(1500);
  http.setTimeout(2000);
  http.begin(cliente, URL_API);
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST(j);
  Serial.print("API: POST /leituras, codigo ");
  Serial.println(codigo);
  http.end();
}

void atualizarLeds(bool ok) {
  if ((int)ok == estadoLed) return;
  estadoLed = ok;
  digitalWrite(LED_AZUL, ok ? HIGH : LOW);
  digitalWrite(LED_AMARELO, ok ? LOW : HIGH);
  Serial.println(ok ? "LED AZUL ON - dados recebidos" : "LED AMARELO ON - timeout de comunicação");
}

// Por enquanto as telas vão para o Serial. Ao ligar a matriz, imprima "texto" nela.
void mostrarTela(bool ok) {
  if (!ok) { Serial.println("Tela -> SEM DADOS"); return; }
  char texto[16];
  const char *nome;
  switch (telaAtual) {
    case 0: nome = "NVL"; snprintf(texto, sizeof(texto), "NVL %d%%", exibido.nivel); break;
    case 1: nome = "TMP"; snprintf(texto, sizeof(texto), isnan(exibido.temperatura) ? "TMP --" : "TMP %.0fC", exibido.temperatura); break;
    case 2: nome = "UMD"; snprintf(texto, sizeof(texto), isnan(exibido.umidade) ? "UMD --" : "UMD %.0f%%", exibido.umidade); break;
    case 3: nome = "LUX"; snprintf(texto, sizeof(texto), "LUX %d", exibido.luminosidade); break;
    default: nome = "PRS"; snprintf(texto, sizeof(texto), "PRS %s", exibido.presenca ? "ON" : "OFF"); break;
  }
  Serial.printf("Tela -> %s   [%s]\n", nome, texto);
  telaAtual = (telaAtual + 1) % 5;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);

  WiFi.mode(WIFI_STA);                       // ESPNOW exige o modo Station
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  Serial.print("Conectando ao Wi-Fi");
  unsigned long ini = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - ini < 15000) { delay(500); Serial.print("."); }
  Serial.println(WiFi.status() == WL_CONNECTED ? " conectado" : " falhou (segue sem Wi-Fi)");
  Serial.print("MAC desta placa: "); Serial.println(WiFi.macAddress());
  Serial.print("Canal Wi-Fi: "); Serial.println(WiFi.channel());   // use este número na placa S3

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");    // hora UTC para o timestamp ISO

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar o ESPNOW");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(aoReceber);
  Serial.println("Receptor pronto.");
}

void loop() {
  if (novoPacote) {
    Pacote p;
    portENTER_CRITICAL(&mux);
    p = recebido;
    novoPacote = false;
    portEXIT_CRITICAL(&mux);
    exibido = p;
    temDados = true;
    String ts = timestampISO();
    Serial.printf("RX: nivel=%d%% temp=%.0fC umd=%.0f%% lux=%d prs=%d ts=%s\n",
                  p.nivel, p.temperatura, p.umidade, p.luminosidade, p.presenca,
                  ts.length() ? ts.c_str() : "sem-hora");
    enviarParaAPI(p, ts);
  }

  bool ok = temDados && (millis() - ultimoRx <= TIMEOUT_MS);
  atualizarLeds(ok);

  if (millis() - ultimaTela >= INTERVALO_TELA_MS) {
    ultimaTela = millis();
    mostrarTela(ok);
  }
}