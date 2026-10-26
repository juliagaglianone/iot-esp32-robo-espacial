# Grafana

O Grafana lê o bucket `praticas-iot` do InfluxDB e exibe o dashboard **Tanque de tinta**, com atualização automática a cada 5 segundos.

## Data source

Em *Connections > Data sources > Add data source > InfluxDB*:

| Campo | Valor |
|---|---|
| Name | `influxdb` |
| Query language | **Flux** |
| URL | `http://localhost:8086` |
| Basic auth | desligado |
| Organization | `Senai Cimatec` |
| Token | token de **leitura** do bucket |
| Default Bucket | `praticas-iot` |

Ao clicar em *Save & test*, deve aparecer: "datasource is working. 1 buckets found".

## Dashboard

O dashboard está versionado em [`grafana/dashboard_tanque.json`](../grafana/dashboard_tanque.json).

Para importar: abra `http://localhost:3000/dashboard/import`, envie o arquivo JSON, selecione o data source `influxdb` e clique em *Import*.

Configuração geral: `uid` `tanque-tinta`, atualização a cada 5 s, janela inicial de 15 minutos.

## Painéis

| Painel | Tipo | Dado | Consulta (resumo) |
|---|---|---|---|
| Nível do tanque (%) | Gauge | `nivel` | `last()`; vermelho abaixo de 20, amarelo de 20 a 50, verde acima |
| Nível do tanque ao longo do tempo | Série temporal | `nivel` | `aggregateWindow(fn: mean)` |
| Temperatura e umidade | Série temporal | `temperatura`, `umidade` | `aggregateWindow(fn: mean)` |
| Luminosidade (0 a 4095) | Série temporal | `luminosidade` | `aggregateWindow(fn: mean)` |
| Presença | Stat | `presenca` | `last()`; 0 = OFF, 1 = ON |
| Presença ao longo do tempo | Barras | `presenca` | `aggregateWindow(fn: max)` |

Exemplo de consulta (painel do gauge):

```flux
from(bucket: "praticas-iot")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "tanque" and r._field == "nivel")
  |> last()
```

## Acesso

Interface em `http://localhost:3000`.
