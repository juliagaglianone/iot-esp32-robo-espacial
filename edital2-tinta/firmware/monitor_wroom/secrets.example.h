// Copie este arquivo para "secrets.h" (na mesma pasta do .ino) e preencha os valores.
// O secrets.h NÃO é versionado (está no .gitignore).

#pragma once

const char* WIFI_SSID  = "NOME_DA_SUA_REDE";
const char* WIFI_SENHA = "SENHA_DA_SUA_REDE";

// IP do computador que roda a API (descubra com: hostname -I)
const char* URL_API    = "http://192.168.X.X:8000/leituras";

// MAC da ESP32-S3 (coletor autorizado), impresso no Serial da S3
const uint8_t MAC_COLETOR[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
