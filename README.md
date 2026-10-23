# iot-esp32-robo-espacial

Projeto de IoT com ESP32: um robô explorador que mede o ambiente, calcula a probabilidade de existir vida, avisa por WhatsApp quando ela passa de 75% e guarda todas as leituras em um banco de dados.

## Objetivo

- **Robô físico:** ler temperatura, umidade, luminosidade e presença, calcular a probabilidade de vida e sinalizar o estado do robô com LEDs.
- **Armazenamento:** enviar as leituras a cada 2 segundos para uma API em Python, que grava em SQLite.
- **Versionamento:** código do robô, backend e script SQL neste repositório.

## Estrutura

```
robo/        firmware do ESP32-S3 (robo.ino e secrets.example.h)
backend/     API FastAPI (main.py), requirements.txt e schema.sql
```

## Como montar o robô

### Componentes

| Componente | Quantidade | Função |
|---|---|---|
| ESP32-S3 DevKit | 1 | Placa principal |
| Sensor DHT22 | 1 | Temperatura e umidade |
| Módulo sensor de luminosidade com LDR (AO, DO, GND, VCC) | 1 | Luminosidade (usa só o AO) |
| Sensor ultrassônico HC-SR04 | 1 | Presença (objeto a menos de 30 cm) |
| LED azul + resistor de 100 ohms | 1 | Robô operando normalmente |
| LED amarelo + resistor de 220 ohms | 1 | Robô desligado ou em alerta |
| Resistores de 1 kohm e 2 kohms | 1 de cada | Divisor de tensão no pino ECHO |
| Microservo 9g | 1 | Locomoção |
| Protoboard, jumpers e cabo USB | 1 kit | Montagem e alimentação |

### Ligações

| Componente | Pino | GPIO do ESP32-S3 |
|---|---|---|
| DHT22 | DATA | 4 |
| Módulo LDR | AO | 1 |
| Ultrassônico | TRIG | 5 |
| Ultrassônico | ECHO (via divisor 1 kohm / 2 kohms) | 18 |
| LED azul (via 100 ohms) | ânodo | 38 |
| LED amarelo (via 220 ohms) | ânodo | 39 |
| Servo | sinal | 16 |

Alimentação:
- DHT22 e módulo LDR em 3V3. O HC-SR04 e o servo em 5V.
- O divisor do ECHO reduz os 5 V do sensor para cerca de 3,3 V: 1 kohm entre o ECHO e o GPIO 18, e 2 kohms entre o GPIO 18 e o GND.
- Cátodo dos LEDs no GND. Todos os GNDs em comum.

### Lógica do firmware

A cada 2 segundos o robô lê os sensores e soma a probabilidade de vida:

| Critério | Pontos |
|---|---|
| Temperatura entre 15 e 30 °C | 25 |
| Umidade entre 40 e 70 % | 25 |
| Luz adequada (leitura do LDR abaixo de 2300) | 20 |
| Presença detectada (distância abaixo de 30 cm) | 30 |

O módulo de LDR usado é invertido: mais luz resulta em leitura menor. Com probabilidade de 75% ou menos, o LED azul fica aceso. Acima de 75%, o LED amarelo acende, o robô exibe o alerta no Monitor Serial e envia uma mensagem de WhatsApp pelo CallMeBot, no máximo uma vez por minuto. Os comandos `f`, `t`, `p`, `d` e `l` digitados no Monitor Serial substituem o joystick (frente, trás, parar, desligar e ligar).

### Gravação

1. Na Arduino IDE, instale as bibliotecas **DHT sensor library**, **Adafruit Unified Sensor** e **ESP32Servo**.
2. Copie `robo/secrets.example.h` para `robo/secrets.h` e preencha o Wi-Fi, o telefone e a APIKEY do CallMeBot e o IP do computador que roda o backend.
3. Selecione a placa **ESP32S3 Dev Module** e grave o `robo/robo.ino`. Monitor Serial em 115200 baud.

### CallMeBot

1. Adicione o número do CallMeBot (consulte o número atual em callmebot.com) aos contatos do WhatsApp.
2. Envie `I allow callmebot to send me messages` e guarde a APIKEY recebida.
3. Use o telefone no formato `DDI + DDD + número`, exatamente como o CallMeBot cadastrou.

## Como rodar o backend Python

```bash
cd backend
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8000
```

O `--host 0.0.0.0` é necessário para o ESP32 alcançar o servidor. O computador e o ESP32 precisam estar na mesma rede. Se necessário, libere a porta: `sudo ufw allow 8000`.

O banco `leituras.db` é criado automaticamente na primeira execução. Para criá-lo manualmente a partir do script SQL:

```bash
sqlite3 leituras.db < backend/schema.sql
```

### Comportamento em caso de falha

Se o servidor não responder com 200 ou 201, o ESP32 guarda a leitura em um buffer de até 100 posições e reenvia as pendentes nos ciclos seguintes, na ordem original e com a hora em que foram medidas.

## Como consultar os dados salvos

Pela API:

```bash
curl http://localhost:8000/leituras
```

A rota `GET /leituras` devolve as últimas 100 leituras. Também é possível testar pela documentação interativa em `http://localhost:8000/docs`.

Direto no banco:

```bash
sqlite3 leituras.db "SELECT * FROM leituras ORDER BY id DESC LIMIT 10;"
```

Se o `sqlite3` não estiver instalado, use Python:

```bash
python3 -c "import sqlite3; c=sqlite3.connect('leituras.db'); print(c.execute('SELECT * FROM leituras ORDER BY id DESC LIMIT 10').fetchall())"
```

Exemplo de leitura enviada pelo robô:

```json
{
  "timestamp": "2026-10-21T02:31:18Z",
  "temperatura_c": 28.7,
  "umidade_pct": 80.3,
  "luminosidade": 3145,
  "presenca": 1,
  "probabilidade_vida": 80.0
}
```

