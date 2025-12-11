# Slide 1 – Title & Hook
- **OMS Gateway: Turning Wireless Meter Data into Usable Streams**
- *ESP32-C3 + CC1101 + OMS/W-MBus*
- Presenter: _[Your Name]_ • Course: _[Class/Term]_
- Visuals: hardware photo + Web UI screenshot to set context quickly.

# Slide 2 – Problem & Motivation
- Utilities deploy OMS/W-MBus meters → need a secure RF→IP bridge.
- Benefits: energy dashboards, automation, research datasets, interoperability.
- Fits IoT landscape: complements classmates’ projects (different sensors, protocols).

# Slide 3 – High-Level Architecture
- Mermaid (optional) or block diagram:
  - Meter → Gateway → Backend → Dashboard.
- Key stages:
  1. RF capture on CC1101 (mode C/LPWAN).
  2. Layered decoding on ESP32-C3 (DLL/ELL/AFL/TPL).
  3. Forwarding & visualization (FastAPI stub + SPA).
- Status note: RF + parser done; backend parsing next.

# Slide 4 – Hardware Stack
- **ESP32-C3**: Wi-Fi + FreeRTOS runtime, USB-C power.
- **CC1101**: Sub-GHz radio for OMS mode C, SPI + GDO interrupts.
- **Enclosure/CAD**: 3D-printed housing, stable lab setup.
- Wiring highlights: MOSI/MISO/SCLK/CS, GDO0/GDO2, LED on GPIO8 (active-low HAL).

# Slide 5 – OMS Protocol Stack Primer
- Layers (Volume 2):
  1. Physical (mode C timings, duty cycle).
  2. Data Link + Extended Link Layer (addresses, C/L fields, hop bits).
  3. Authentication & Fragmentation Layer (MAC, fragments, KI flag).
  4. Transport Layer (CI, Access Number, Status, CF).
  5. Application Protocols (M-Bus/DLMS/SML).
  6. Security Profiles (A–D → AES/TLS/CCM).
- Firmware maps each layer via `wmbus/pipeline`, `frame_parse`, etc.

# Slide 6 – Firmware Pipeline
- Flow: RF bytes → `wmbus_pipeline_receive` → parsed frame → `packet_router`.
- Sinks:
  - Web UI (live packet monitor, config).
  - FastAPI backend (JSON payloads).
- Diagnostics: logging, status LED patterns (fade/disconnect, double blink/connected, pulse/packet).

# Slide 7 – Web UI & UX
- Screenshots: overview, config panels, packet details.
- Features:
  - Wi-Fi STA/AP onboarding.
  - Radio presets (CS threshold, sync mode).
  - Whitelist CRUD, backend probe.
  - Live packet table w/ RSSI, IDs, security hints.
- Notes: SPA served from ESP32 via embedded assets.

# Slide 8 – Backend / Data Layer
- FastAPI endpoint `/oms`: validates gateway JSON payloads.
- Planned pipeline: MQTT + InfluxDB for long-term storage, dashboards.
- Reasoning: backend handles keys, advanced parsing (Annex B data points).
- Future work: decrypt AES-CBC/CCM/TLS payloads, map OBIS codes.

# Slide 9 – Challenges & Lessons
- RF noise in city → need strict sync detection, credit management.
- Layered parsing complexity → documentation + Mermaid diagrams for clarity.
- Hardware quirks: active-low LED, SPI timing, enclosure wiring.
- Transferable lessons for other IoT topics: layered design, UI diagnostics, backend offloading.

# Slide 10 – Roadmap & Collaboration
- Next steps:
  - Backend security parsing + key management.
  - MQTT/Influx integration.
  - PCB/RF layout rev.
  - Automated tests & contributor docs.
- Invite classmates:
  - Share tips on data pipelines, visualization, RF design.
  - Potential cross-project integrations (shared dashboards, MQTT topics).
- Provide repo link + documentation references (`doc/readme.md`, `doc/OMS_PROTOCOL_STACK.md`).

# Slide 11 – Q&A / Appendix
- Summary bullets:
  - Gateway bridges OMS RF to IP with layered decoding + UI.
  - Current build stable for RF + UI; backend parsing upcoming.
- Contact info (email, GitHub).
- Optional backup slides: protocol tables, references, BOM.
