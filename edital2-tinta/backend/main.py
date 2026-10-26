import os
from datetime import datetime, timezone
from typing import Optional

from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

load_dotenv()
URL = os.environ["INFLUX_URL"]
TOKEN = os.environ["INFLUX_TOKEN"]
ORG = os.environ["INFLUX_ORG"]
BUCKET = os.environ["INFLUX_BUCKET"]

app = FastAPI(title="API do tanque de tinta")
client = InfluxDBClient(url=URL, token=TOKEN, org=ORG)
write_api = client.write_api(write_options=SYNCHRONOUS)


class Leitura(BaseModel):
    nivel: float
    temperatura: Optional[float] = None
    umidade: Optional[float] = None
    luminosidade: int
    presenca: int
    timestamp: Optional[str] = None   # ISO 8601 em UTC, ex.: 2026-10-26T00:10:00Z


@app.get("/health")
def health():
    return {"status": "ok"}


@app.post("/leituras", status_code=201)
def receber(l: Leitura):
    ts = datetime.now(timezone.utc)
    if l.timestamp:
        try:
            ts = datetime.fromisoformat(l.timestamp.replace("Z", "+00:00"))
        except ValueError:
            raise HTTPException(status_code=422, detail="timestamp inválido")

    ponto = (
        Point("tanque")
        .field("nivel", l.nivel)
        .field("luminosidade", l.luminosidade)
        .field("presenca", l.presenca)
        .time(ts, WritePrecision.S)
    )
    if l.temperatura is not None:
        ponto = ponto.field("temperatura", l.temperatura)
    if l.umidade is not None:
        ponto = ponto.field("umidade", l.umidade)

    try:
        write_api.write(bucket=BUCKET, org=ORG, record=ponto)
    except Exception as e:
        raise HTTPException(status_code=502, detail=f"erro ao gravar no InfluxDB: {e}")
    return {"status": "ok"}
