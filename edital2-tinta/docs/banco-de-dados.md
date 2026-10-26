# Banco de dados (InfluxDB)

O sistema usa o **InfluxDB 2.x**, um banco de séries temporais: cada leitura é gravada junto com o instante em que foi medida, e as consultas por período são rápidas.

## Organização

| Item | Valor |
|---|---|
| URL | `http://localhost:8086` |
| Organização | `Senai Cimatec` |
| Bucket | `praticas-iot` |
| Retenção | 30 dias (leituras mais antigas são apagadas automaticamente) |
| Linguagem de consulta | Flux |

## Modelo de dados

Cada leitura enviada pela API vira um ponto no *measurement* `tanque`. Não há *tags*; cada grandeza é um *field*.

| Field | Tipo | Unidade | Descrição |
|---|---|---|---|
| `nivel` | float | % | Nível do tanque (0 a 100), calculado pela distância do ultrassônico |
| `temperatura` | float | °C | Temperatura do DHT22 (omitido se a leitura falhar) |
| `umidade` | float | % | Umidade relativa do DHT22 (omitido se a leitura falhar) |
| `luminosidade` | int | 0 a 4095 | Leitura analógica do LDR (maior valor = mais luz) |
| `presenca` | int | 0 ou 1 | Sensor PIR (1 = presença detectada) |

O *timestamp* é gerado na ESP32 de monitoramento via NTP (UTC, ISO 8601). Se o NTP ainda não sincronizou, a API usa a hora de recebimento.

## Tokens

| Token | Permissão | Uso |
|---|---|---|
| Token de escrita | write no bucket `praticas-iot` | API (`backend/.env`) |
| Token de leitura | read no bucket `praticas-iot` | Data source do Grafana |

Os tokens nunca são versionados. A API os lê do arquivo `.env`, que está no `.gitignore`.

## Configuração (resumo)

1. Instalar o InfluxDB 2 e abrir `http://localhost:8086`.
2. Criar a organização `Senai Cimatec` e o bucket `praticas-iot`.
3. Em *Load Data > Buckets > praticas-iot > Settings*, definir a retenção em 30 dias.
4. Em *Load Data > API Tokens*, criar o token de escrita e o de leitura com acesso ao bucket.

## Consultar os dados salvos

### Pela interface (Data Explorer)

Em `http://localhost:8086`, abra *Data Explorer*, escolha o bucket `praticas-iot` e o measurement `tanque`, ou use o *Script Editor* com Flux:

```flux
from(bucket: "praticas-iot")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "tanque")
```

### Pela linha de comando (API HTTP)

```bash
curl --request POST "http://localhost:8086/api/v2/query?org=Senai%20Cimatec" \
  --header "Authorization: Token $TOKEN_LEITURA" \
  --header "Content-Type: application/vnd.flux" \
  --header "Accept: application/csv" \
  --data 'from(bucket: "praticas-iot")
    |> range(start: -10m)
    |> filter(fn: (r) => r._measurement == "tanque" and r._field == "nivel")
    |> last()'
```

### Consultas úteis

Último valor de cada grandeza:

```flux
from(bucket: "praticas-iot")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "tanque")
  |> last()
```

Média do nível por minuto na última hora:

```flux
from(bucket: "praticas-iot")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "tanque" and r._field == "nivel")
  |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)
```

Instantes em que havia presença:

```flux
from(bucket: "praticas-iot")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "tanque" and r._field == "presenca" and r._value == 1)
```
