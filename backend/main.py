import sqlite3
from datetime import datetime, timezone
from typing import Optional

from fastapi import FastAPI
from pydantic import BaseModel

DB = "leituras.db"
app = FastAPI(title="Robo explorador")


def conectar():
    con = sqlite3.connect(DB)
    con.row_factory = sqlite3.Row
    return con


with conectar() as con:
    con.execute("""
        CREATE TABLE IF NOT EXISTS leituras (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            temperatura_c REAL,
            umidade_pct REAL,
            luminosidade INTEGER,
            presenca INTEGER,
            probabilidade_vida REAL
        )
    """)


class Leitura(BaseModel):
    timestamp: Optional[str] = None      # se vier vazio, o servidor usa a hora de chegada
    temperatura_c: Optional[float] = None  # null se o DHT22 falhar
    umidade_pct: Optional[float] = None
    luminosidade: int
    presenca: int
    probabilidade_vida: float


@app.post("/leituras", status_code=201)
def criar_leitura(l: Leitura):
    ts = l.timestamp or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    con = conectar()
    with con:
        cur = con.execute(
            "INSERT INTO leituras (timestamp, temperatura_c, umidade_pct, "
            "luminosidade, presenca, probabilidade_vida) VALUES (?, ?, ?, ?, ?, ?)",
            (ts, l.temperatura_c, l.umidade_pct, l.luminosidade,
             l.presenca, l.probabilidade_vida),
        )
    con.close()
    return {"status": "ok", "id": cur.lastrowid}


@app.get("/leituras")
def listar_leituras():
    con = conectar()
    linhas = con.execute(
        "SELECT * FROM leituras ORDER BY id DESC LIMIT 100"
    ).fetchall()
    con.close()
    return [dict(x) for x in linhas]
