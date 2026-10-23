-- Criação da tabela de leituras do robô explorador (SQLite)
-- Uso: sqlite3 leituras.db < schema.sql

CREATE TABLE IF NOT EXISTS leituras (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,        -- data e hora em UTC, formato ISO 8601
    temperatura_c REAL,             -- NULL se o DHT22 falhar
    umidade_pct REAL,               -- NULL se o DHT22 falhar
    luminosidade INTEGER,           -- valor bruto do LDR (módulo invertido: mais luz, valor menor)
    presenca INTEGER,               -- 1 = objeto a menos de 30 cm, 0 = sem presença
    probabilidade_vida REAL         -- 0 a 100
);

-- Consulta das últimas 100 leituras (mesma da rota GET /leituras)
-- SELECT * FROM leituras ORDER BY id DESC LIMIT 100;
