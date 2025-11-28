# OMS Hub (FastAPI)

Kleine FastAPI-Anwendung, die einen `POST`-Endpoint unter `/oms` bereitstellt.

## Setup

```bash
cd Hub
python -m venv .venv
source .venv/bin/activate  # Windows: .venv\\Scripts\\activate
pip install -r requirements.txt
```

## Starten

```bash
uvicorn main:app --reload
```

Der Endpoint ist anschließend unter `http://localhost:8000/oms` erreichbar. Wenn du einen anderen Port nutzen möchtest, gib ihn Uvicorn mit, z. B.:

```bash
uvicorn main:app --reload --host 0.0.0.0 --port 9000
```

## Health Check

Auf `GET /` erhältst du eine kurze Health-Message (`{"status": "ok", ...}`), damit bei direktem Aufruf der Basis-URL etwas zurückkommt.
`HEAD /` liefert `200 OK` mit Header `X-OMS-Status: alive`.

## Beispiel-Request (erwartetes Format)

```bash
curl -X POST http://localhost:8000/oms \
  -H "Content-Type: application/json" \
  -d '{
        "gateway": "gw-1",
        "status": 0,
        "rssi": -58,
        "lqi": 200,
        "manuf": 1234,
        "id": "01119360",          # 4-Byte-ID als Hex-String, genau 8 Zeichen (MSB->LSB)
        "dev_type": 1,
        "version": 2,
        "ci": 42,
        "payload_len": 8,          # optional; wird falls fehlend aus logical_hex Länge/2 abgeleitet
        "logical_hex": "0a0b0c0d0e0f"  # beliebige Länge
      }'
```

Das Backend erwartet dieses Schema; ungültige Requests führen zu 422. Die Antwort enthält das empfangene Objekt unter `received` und den abgeleiteten `payload_len`. Im Log wird die Payload formatiert als JSON ausgegeben.
