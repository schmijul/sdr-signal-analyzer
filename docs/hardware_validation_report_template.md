# Hardware Validation Report Template

This docs-visible page is the canonical lab report template. Copy from this page when you prepare an attached-device validation report.

Maintainer note:
- `docs/templates/hardware_validation_report_template.md` is only a pointer back to this canonical page so the template text does not drift in two places.

- Date:
- Operator:
- Repository commit:
- Machine:
- OS:
- Python version:
- Compiler:
- Backend:
- Hardware model:
- Firmware / FPGA / driver / SDK version:
- Antenna / cabling notes:
- Center frequency:
- Sample rate:
- Gain:
- Additional bandwidth / clock / time settings:
- Exact command:
- GUI used:
- Diagnostics log path:
- JSONL export path:
- Recording path:
- Replay comparison command:
- Observed behavior:
- Screenshots:
- Pass / fail:
- Open issues:

## Required Evidence

- attach the diagnostics log
- attach the JSONL export if analysis output is relevant
- record whether this was startup-only, short-stream, or longer stability validation
- record the exact command in `commands.txt` or inline in this report
- state explicitly whether this report is enough to move the backend status forward
