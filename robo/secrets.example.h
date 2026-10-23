// Copie este arquivo para "secrets.h" (na mesma pasta do robo.ino) e preencha.
// O secrets.h está no .gitignore e não deve ser enviado ao GitHub.

#define WIFI_SSID_VALOR   "SEU_SSID"
#define WIFI_SENHA_VALOR  "SUA_SENHA"

// Telefone sem "+", no formato DDI + DDD + número, exatamente como o CallMeBot cadastrou.
#define CB_TELEFONE_VALOR "5571XXXXXXXXX"
#define CB_APIKEY_VALOR   "SUA_APIKEY"

// IP do computador que roda o backend, na mesma rede do ESP32 (use "hostname -I").
#define URL_API_VALOR     "http://192.168.0.X:8000/leituras"
