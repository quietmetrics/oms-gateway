"""Minimal FastAPI application exposing a POST endpoint at /oms."""
import logging
from typing import Any, Dict, Optional

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field


class OMSPayload(BaseModel):
    order_id: str = Field(..., description="Unique identifier for the order or request")
    data: Dict[str, Any] = Field(default_factory=dict, description="Arbitrary payload data")
    source: Optional[str] = Field(
        default=None, description="Optional source system identifier for the payload"
    )


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)
logger = logging.getLogger("oms-hub")

app = FastAPI(title="OMS Hub", version="0.1.1")


@app.post("/oms")
async def handle_oms(payload: OMSPayload) -> Dict[str, Any]:
    """
    Accepts incoming OMS payloads and returns an acknowledgement.

    In a real implementation this is where you would trigger downstream processing,
    persist the payload, etc.
    """
    if not payload.order_id:
        raise HTTPException(status_code=400, detail="order_id is required")

    # Log the full incoming payload to the console for visibility.
    logger.info("Received OMS payload: %s", payload.json())

    return {
        "status": "accepted",
        "order_id": payload.order_id,
        "received": payload.data,
        "source": payload.source,
    }


if __name__ == "__main__":
    import uvicorn

    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
