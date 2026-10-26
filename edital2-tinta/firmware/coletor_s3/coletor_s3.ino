// ESP32 chão de fábrica - Passos 2 e 3: sensores, LEDs e envio via ESPNOW
// LED azul = operação normal | LED amarelo = alerta

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <DHT.h>

// ===== PINOS =====
#define DHT_PIN      4
#define DHT_TIPO     DHT22
#define LDR_PIN      1
#define TRIG_PIN     5
#define ECHO_PIN     18
#define PIR_PIN      6
#define LED_AZUL     38
#define LED_AMARELO  39

// ===== MAC DO ESP32 DE EXIBIÇÃO (WROOM-32) =====
uint8_t MAC_DISPLAY[] = {0xE8, 0x31, 0xCD, 0xAC, 0xF4, 0xD0};
#define CANAL_WIFI 11   // troque pelo "Canal Wi-Fi" mostrado no Serial da WROOM

// ===== CALIBRAÇÃO DO TANQUE =====
#define DIST_VAZIO_CM  20.0   // distância do sensor até o fundo (tanque vazio)
#define DIST_CHEIO_CM   4.0   // distância do sensor até a superfície (tanque cheio)
#define NIVEL_MINIMO    20    // abaixo disso: alerta
#define INTERVALO_MS    2000

// ===== PACOTE ENVIADO =====
typedef struct __attribute__((packed)) {
  uint8_t  nivel;          // %
  float    temperatura;    // °C
  float    umidade;        // %
  int16_t  luminosidade;   // valor analógico
  uint8_t  presenca;       // 0 = ausente, 1 = detectada
  uint32_t timestamp;      // ms desde que a placa ligou
} Pacote;

DHT dht(DHT_PIN, DHT_TIPO);
unsigned long ultimoCiclo = 0;
volatile int resultadoEnvio = -1;   // -1 aguardando, 0 falha, 1 sucesso
Pacote pendente;
bool temPendente = false;

// Callback do ESPNOW (a assinatura muda entre as versões do core ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void aoEnviar(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
void aoEnviar(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  resultadoEnvio = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 0;
}

float medirDistanciaCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracao == 0) return -1;
  return duracao * 0.0343 / 2.0;
}

// Mediana de 5 leituras para reduzir ruído
float distanciaMediana() {
  float v[5];
  for (int i = 0; i < 5; i++) { v[i] = medirDistanciaCm(); delay(30); }
  for (int i = 0; i < 4; i++)
    for (int j = i + 1; j < 5; j++)
      if (v[j] < v[i]) { float t = v[i]; v[i] = v[j]; v[j] = t; }
  return v[2];
}

int lerLuz() {
  long soma = 0;
  for (int i = 0; i < 10; i++) { soma += analogRead(LDR_PIN); delay(5); }
  return 4095 - (soma / 10);
}

// Envia um pacote e espera o resultado (até 300 ms)
bool enviar(const Pacote &p) {
  resultadoEnvio = -1;
  esp_err_t erro = esp_now_send(MAC_DISPLAY, (const uint8_t *)&p, sizeof(p));
  if (erro != ESP_OK) return false;
  unsigned long inicio = millis();
  while (resultadoEnvio == -1 && millis() - inicio < 300) delay(1);
  return resultadoEnvio == 1;
}

// Pisca o LED duas vezes e devolve ao estado em que estava
void piscar(int pino) {
  bool antes = digitalRead(pino);
  for (int i = 0; i < 2; i++) {
    digitalWrite(pino, !antes); delay(100);
    digitalWrite(pino, antes);  delay(100);
  }
}

void ciclo() {
  float dist = distanciaMediana();
  float temp = dht.readTemperature();
  float umid = dht.readHumidity();
  int luz = lerLuz();
  bool presenca = digitalRead(PIR_PIN);

  // Nível do tanque (%)
  bool falhaNivel = (dist < 0);
  int nivel = 0;
  if (!falhaNivel) {
    float n = (DIST_VAZIO_CM - dist) / (DIST_VAZIO_CM - DIST_CHEIO_CM) * 100.0;
    nivel = (int)round(constrain(n, 0.0, 100.0));
  }

  // Alerta: nível baixo ou falha na leitura do nível
  bool alerta = falhaNivel || (nivel < NIVEL_MINIMO);
  digitalWrite(LED_AZUL, alerta ? LOW : HIGH);
  digitalWrite(LED_AMARELO, alerta ? HIGH : LOW);

  Serial.println("----------------------------");
  if (falhaNivel) {
    Serial.println("Ultrassônico: falha na leitura");
  } else {
    Serial.print("Distância: "); Serial.print(dist); Serial.println(" cm");
    Serial.print("Nível do tanque: "); Serial.print(nivel); Serial.println("%");
  }
  if (!falhaNivel && nivel < NIVEL_MINIMO) Serial.println("Alerta! Nível de tinta baixo.");

  if (isnan(temp) || isnan(umid)) {
    Serial.println("DHT22: falha na leitura");
  } else {
    Serial.print("Temperatura: "); Serial.print(temp, 0);
    Serial.print(" ºC | Umidade: "); Serial.print(umid, 0); Serial.println("%");
  }

  Serial.print("Luminosidade: "); Serial.println(luz);
  Serial.println(presenca ? "Presença detectada" : "Sem presença");
  Serial.println(alerta ? "Alerta – verificar tanque de tinta" : "Operação normal");

  // ===== ENVIO ESPNOW =====
  Pacote atual;
  atual.nivel = (uint8_t)nivel;
  atual.temperatura = temp;
  atual.umidade = umid;
  atual.luminosidade = (int16_t)luz;
  atual.presenca = presenca ? 1 : 0;
  atual.timestamp = millis();

  // Reenvio: primeiro tenta o pacote que falhou no ciclo anterior
  if (temPendente) {
    if (enviar(pendente)) {
      temPendente = false;
      Serial.println("Pacote pendente reenviado com sucesso.");
    }
  }

  if (enviar(atual)) {
    Serial.printf("Pacote enviado com sucesso: {nível=%d%%, temp=%.0f°C, umidade=%.0f%%, luz=%d, presença=%d}\n",
                  atual.nivel, atual.temperatura, atual.umidade, atual.luminosidade, atual.presenca);
    if (!alerta) piscar(LED_AZUL);   // em alerta, o azul continua apagado
  } else {
    Serial.println("Falha no envio ESPNOW");
    pendente = atual;       // guarda para reenviar no próximo ciclo
    temPendente = true;
    piscar(LED_AMARELO);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  dht.begin();

  // ESPNOW exige o modo Wi-Fi Station
  WiFi.mode(WIFI_STA);
  delay(200);

  // Mesmo canal de rádio da WROOM (que está conectada ao roteador)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(CANAL_WIFI, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  Serial.print("MAC desta placa: "); Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar o ESPNOW");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(aoEnviar);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, MAC_DISPLAY, 6);
  peer.channel = CANAL_WIFI;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Erro ao adicionar o ESP32 de exibição");
    while (true) delay(1000);
  }

  Serial.println("Aquecendo o PIR (40 s)...");
  delay(40000);
  Serial.println("Sistema pronto.");
}

void loop() {
  if (millis() - ultimoCiclo >= INTERVALO_MS) {
    ultimoCiclo = millis();
    ciclo();
  }
}