"""Minimal FastAPI application exposing a POST endpoint at /oms."""

import logging
from typing import Any, Dict, Optional

from fastapi import FastAPI, Request, Response
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)
logger = logging.getLogger("oms-hub")

app = FastAPI(title="OMS Hub", version="0.1.1", description="A minimal OMS Hub API")


class OMSPayload(BaseModel):
    """Data structure describing an OMS payload; payload length is flexible."""

    gateway: str = Field(..., description="Gateway identifier that received the frame")
    status: int = Field(..., description="Status code reported by the gateway/device")
    rssi: float = Field(..., description="Received signal strength indicator")
    lqi: int = Field(..., description="Link quality indicator")
    manuf: int = Field(..., description="Manufacturer identifier")
    id: str = Field(
        ...,
        description='4-byte device identifier in hex, MSB->LSB (e.g. "01119360")',
        pattern=r"^[0-9A-Fa-f]{8}$",
    )
    dev_type: int = Field(..., description="Device type")
    version: int = Field(..., description="Firmware/protocol version")
    ci: int = Field(..., description="Cluster identifier")
    payload_len: Optional[int] = Field(
        default=None, description="Length of payload bytes (optional, inferred if missing)"
    )
    logical_hex: str = Field(
        ...,
        description="Hex string of the CRC-free frame (arbitrary length)",
        min_length=1,
    )

    @property
    def inferred_payload_len(self) -> int:
        """Return provided payload_len or infer from logical_hex length."""
        return self.payload_len if self.payload_len is not None else len(self.logical_hex) // 2


@app.api_route("/", methods=["GET", "HEAD"], response_model=None)
async def health(request: Request):
    """Health endpoint to confirm the service is reachable."""
    client = request.client.host if request.client else "-"
    logger.info("Health %s from %s", request.method, client)
    if request.method == "HEAD":
        return Response(status_code=200, headers={"X-OMS-Status": "alive"})
    return JSONResponse(
        content={"status": "ok", "message": "OMS Hub is running"},
        headers={"X-OMS-Status": "alive"},
    )


@app.head("/oms", include_in_schema=False)
async def oms_head():
    """Allow HEAD on /oms for simple liveness checks."""
    return Response(status_code=200, headers={"X-OMS-Status": "alive"})


@app.post("/oms")
async def handle_oms(payload: OMSPayload) -> Dict[str, Any]:
    """Accept a validated OMS payload, log it, and return an acknowledgement."""
    logger.info("Received OMS payload:\n%s", payload.model_dump_json(indent=2))

    return {
        "status": "accepted",
        "received": payload.model_dump(),
        "payload_len": payload.inferred_payload_len,
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=8080, reload=True)
