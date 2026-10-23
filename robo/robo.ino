// Robo explorador: probabilidade de vida (ESP32-S3) com envio ao backend
// Bibliotecas: DHT sensor library, Adafruit Unified Sensor, ESP32Servo

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <time.h>
#include "secrets.h"

// WI-FI, CALLMEBOT E BACKEND (valores vêm do secrets.h)
const char* WIFI_SSID   = WIFI_SSID_VALOR;
const char* WIFI_SENHA  = WIFI_SENHA_VALOR;
const char* CB_TELEFONE = CB_TELEFONE_VALOR;
const char* CB_APIKEY   = CB_APIKEY_VALOR;
const char* URL_API     = URL_API_VALOR;

// PINOS 
#define DHT_PIN      4
#define DHT_TIPO     DHT22
#define LDR_PIN      1
#define TRIG_PIN     5
#define ECHO_PIN     18
#define LED_AZUL     38   // operando normalmente
#define LED_AMARELO  39   // desligado ou alerta
#define SERVO_PIN    16
#define USAR_SERVO   1    // 0 = ignora o servo

// LIMITES 
#define TEMP_MIN          15.0
#define TEMP_MAX          30.0   // para forcar o alerta em teste, use 35.0
#define UMID_MIN          40.0
#define UMID_MAX          70.0   // para forcar o alerta em teste, use 90.0
#define LIMITE_LUZ        2300   // LDR invertido: luz adequada se leitura < limite
#define DIST_PRESENCA_CM  30.0
#define LIMITE_ALERTA     75
#define INTERVALO_MS      2000
#define INTERVALO_ALERTA_MS 60000

// BUFFER DE LEITURAS 
#define BUFFER_MAX 100
String buffer[BUFFER_MAX];
int bufInicio = 0;
int bufQtd = 0;

enum Estado { LIGADO, DESLIGADO, ALERTA };

DHT dht(DHT_PIN, DHT_TIPO);
Servo servo;

bool desligadoRemoto = false;
unsigned long ultimoCiclo = 0;
unsigned long ultimoAlerta = 0;
bool alertaJaEnviado = false;

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
  return soma / 10;
}

void enviarWhatsApp() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem Wi-Fi: mensagem nao enviada.");
    return;
  }
  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;
  String url = String("https://api.callmebot.com/whatsapp.php?phone=") + CB_TELEFONE +
               "&text=Alerta%21+Alta+probabilidade+de+vida+detectada+no+planeta."
               "&apikey=" + CB_APIKEY;
  http.begin(cliente, url);
  int codigo = http.GET();
  Serial.print("CallMeBot, codigo HTTP: ");
  Serial.println(codigo);
  if (codigo == 200 || codigo == 210) Serial.println("CallMeBot: mensagem aceita.");
  else Serial.println("CallMeBot: falha no envio.");
  http.end();
}

void moverServo(int angulo) {
#if USAR_SERVO
  servo.write(angulo);
#endif
}

// Comandos pelo Monitor Serial (provisorio, no lugar do joystick):
// f = frente, t = tras, p = parar, d = desligar, l = ligar
void tratarComando() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') { desligadoRemoto = true; moverServo(90); Serial.println("Comando: DESLIGAR"); }
    else if (c == 'l') { desligadoRemoto = false; Serial.println("Comando: LIGAR"); }
    else if (!desligadoRemoto) {
      if (c == 'f') moverServo(180);
      else if (c == 't') moverServo(0);
      else if (c == 'p') moverServo(90);
    }
  }
}

// BACKEND
String timestampISO() {
  time_t agora = time(nullptr);
  if (agora < 1700000000) return "";   // relogio ainda nao sincronizou
  struct tm t;
  gmtime_r(&agora, &t);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
  return String(buf);
}

String montarJSON(float temp, float umid, int luz, bool presenca, int prob) {
  String ts = timestampISO();
  String j = "{";
  j += "\"timestamp\":" + (ts.length() ? "\"" + ts + "\"" : String("null")) + ",";
  j += "\"temperatura_c\":" + (isnan(temp) ? String("null") : String(temp, 1)) + ",";
  j += "\"umidade_pct\":" + (isnan(umid) ? String("null") : String(umid, 1)) + ",";
  j += "\"luminosidade\":" + String(luz) + ",";
  j += "\"presenca\":" + String(presenca ? 1 : 0) + ",";
  j += "\"probabilidade_vida\":" + String((float)prob, 1) + "}";
  return j;
}

void bufferAdd(const String& s) {
  if (bufQtd == BUFFER_MAX) {          // cheio: descarta a mais antiga
    bufInicio = (bufInicio + 1) % BUFFER_MAX;
    bufQtd--;
  }
  buffer[(bufInicio + bufQtd) % BUFFER_MAX] = s;
  bufQtd++;
}

bool postar(const String& json) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return false;
  }
  WiFiClient cliente;
  HTTPClient http;
  http.setConnectTimeout(1500);
  http.setTimeout(2000);
  http.begin(cliente, URL_API);
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST(json);
  Serial.print("POST /leituras, codigo: ");
  Serial.print(codigo);
  if (codigo > 0) {
    Serial.print(" | resposta: ");
    Serial.print(http.getString());
  }
  Serial.println();
  http.end();
  return (codigo == 200 || codigo == 201);
}

void enviarBuffer() {
  int enviados = 0;
  while (bufQtd > 0 && enviados < 5) {   // no maximo 5 por ciclo
    if (!postar(buffer[bufInicio])) break;
    bufInicio = (bufInicio + 1) % BUFFER_MAX;
    bufQtd--;
    enviados++;
  }
  if (bufQtd > 0) {
    Serial.print("Leituras pendentes no buffer: ");
    Serial.println(bufQtd);
  }
}

void ciclo() {
  float temp = dht.readTemperature();
  float umid = dht.readHumidity();
  int luz = lerLuz();
  float dist = distanciaMediana();
  bool presenca = (dist > 0 && dist < DIST_PRESENCA_CM);

  int prob = 0;
  if (!isnan(temp) && temp >= TEMP_MIN && temp <= TEMP_MAX) prob += 25;
  if (!isnan(umid) && umid >= UMID_MIN && umid <= UMID_MAX) prob += 25;
  if (luz < LIMITE_LUZ) prob += 20;
  if (presenca) prob += 30;

  Estado estado;
  if (desligadoRemoto) estado = DESLIGADO;
  else if (prob > LIMITE_ALERTA) estado = ALERTA;
  else estado = LIGADO;

  digitalWrite(LED_AZUL, estado == LIGADO ? HIGH : LOW);
  digitalWrite(LED_AMARELO, estado == LIGADO ? LOW : HIGH);

  Serial.println("----------------------------");
  if (isnan(temp) || isnan(umid)) Serial.println("DHT22: falha na leitura");
  else {
    Serial.print("Temperatura: "); Serial.print(temp); Serial.println(" C");
    Serial.print("Umidade: "); Serial.print(umid); Serial.println(" %");
  }
  Serial.print("Luz (AO): "); Serial.println(luz);
  Serial.print("Distancia: "); Serial.print(dist); Serial.println(" cm");
  Serial.println(presenca ? "Presenca detectada" : "Sem presenca");
  Serial.print("Estado do robo: ");
  Serial.println(estado == LIGADO ? "ligado" : estado == DESLIGADO ? "desligado" : "alerta");
  Serial.print("Probabilidade de vida: "); Serial.print(prob); Serial.println("%");

  if (estado == ALERTA) {
    Serial.println("ALERTA! Alta probabilidade de vida detectada!");
    if (!alertaJaEnviado || millis() - ultimoAlerta >= INTERVALO_ALERTA_MS) {
      enviarWhatsApp();
      alertaJaEnviado = true;
      ultimoAlerta = millis();
    }
  } else {
    alertaJaEnviado = false;
    if (estado == LIGADO)
      Serial.println("Exploracao normal. Nenhum indicio relevante detectado.");
  }

  bufferAdd(montarJSON(temp, umid, luz, presenca, prob));
  enviarBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  dht.begin();

#if USAR_SERVO
  ESP32PWM::allocateTimer(0);
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(90);
#endif

  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  Serial.print("Conectando ao Wi-Fi");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " conectado" : " falhou (segue sem Wi-Fi)");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // hora UTC para o timestamp
  unsigned long t0 = millis();
  while (time(nullptr) < 1700000000 && millis() - t0 < 10000) delay(200);
}

void loop() {
  tratarComando();
  if (millis() - ultimoCiclo >= INTERVALO_MS) {
    ultimoCiclo = millis();
    ciclo();
  }
}
