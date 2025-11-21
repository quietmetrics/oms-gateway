# CC1101 Status Diagnostics

## Date
2025-??-?? (keep updated as new tests run)

## Observed Behaviour
- `MARCSTATE` frequently returns `0x1F` (WOR sleep) when `receive_packet()` polls for data.
- Driver logs repeatedly show:
  - `W (...) CC1101_DRIVER: CC1101 not in RX mode, current state: 0x1F`
  - `I (...) CC1101_DRIVER: Set CC1101 to RX mode`
  - `W (...) CC1101_DRIVER: Packet size (128) exceeds CC1101 FIFO, truncating to 64 bytes`
- FIFO dump is always `0x80` bytes even when RSSI reports ~ -60 dBm.

## Root Causes (probable)
1. **Radio never enters steady RX mode**  
   - CC1101 stays in WOR/idle (state `0x1F`) between polls. Reading the FIFO immediately after forcing RX yields stale/noisy data.
2. **Incomplete RF configuration**  
   - `configure_rf_parameters()` still uses placeholder register values; sync word/filter settings haven’t been tuned for wM-Bus yet.
3. **Possible GDO wiring mismatch**  
   - If GDO0/GDO2 are swapped or incorrectly defined, RX-ready indications never reach the firmware, so the loop reads on timeouts.

## Mitigations Implemented
1. **Driver-level safeguards**
   - Increased SPI transfer limit and truncated reported packet length to the FIFO size so we no longer crash with `ESP_ERR_INVALID_SIZE`.
   - Added logging around truncation so we can spot unrealistic packet lengths.
2. **System behaviour**
   - Gateway no longer aborts when CC1101 fails; radio status is exposed via `/api/dashboard_stats` and the web UI shows “Radio unavailable”.

## Outstanding Work
1. **Harden RX state management**
   - When `MARCSTATE != 0x0D`, flush RX FIFO (`SFRX`), re-enter RX (`SRX`), and re-check the state. If it still isn’t RX after retry, set the system radio status to `error` so users see the outage.
2. **Apply real wM-Bus register set**
   - Export the correct configuration from TI SmartRF Studio (868.95 MHz, 38.4 kbps GFSK) and replace the placeholder register writes in `configure_rf_parameters()`.
   - Ensure sync word, packet automation, FIFO thresholds, and filters match the target meters.
3. **Verify hardware wiring**
   - Confirm `CC1101_GDO0_GPIO` and `CC1101_GDO2_GPIO` match the actual wiring on the board. Adjust `cc1101.h` if necessary.
   - Check power/clock integrity; `RSSI -59 dBm` with pure `0x80` data suggests the demodulator is not locked.
4. **Monitoring**
   - Keep the existing radio-status panel in the UI up to date (dashboard and `/radio` page) so any regression is immediately visible to users.

## Next Steps
1. Implement the RX state retry/flush logic in `receive_packet()`.
2. Replace the CC1101 register block with the SmartRF export and test against known-good wM-Bus transmissions.
3. Validate GDO wiring and document the verified pin mapping in `Progress/Hardware_and_Technical_Details.md`.
4. Once real frames appear, update this document with the confirmed register set and expected log output (RSSI/LQI ranges, typical packet lengths).
