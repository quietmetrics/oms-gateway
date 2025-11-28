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

Der Endpoint ist anschließend unter `http://localhost:8000/oms` erreichbar.

## Beispiel-Request

```bash
curl -X POST http://localhost:8000/oms \
  -H "Content-Type: application/json" \
  -d '{"order_id": "12345", "data": {"item": "abc"}, "source": "cli"}'
```
