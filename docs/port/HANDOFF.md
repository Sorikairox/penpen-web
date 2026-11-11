# HANDOFF

Newest session first. Read `docs/port/PLAN.md` (disc facts, survey, approach, phases, rules) before working.

## S0 — 2026-11-11: survey of the US disc, port plan

Static survey only, all in sandboxed containers (`--network none --read-only --cap-drop ALL`, disc volume
`penpen-disc` read-only, scratch volume `penpen-work`). Nothing from the disc was executed or copied to the host.

- Disc: Redump GD-ROM, 48 tracks; track 03 is IP.BIN + ISO descriptors + padding, track 48 holds all 194 files
  (150.5 MB), tracks 04–47 are 44 CDDA music tracks (~77 min). Hashes in PLAN 0.1.
- IP.BIN: `T15105N` `V1.020` 1999-08-19, area U, peripherals `0799A10` (VGA, Jump Pack, VMU, standard pad with
  analog triggers), boot `1ST_READ.BIN`, Katana (not WinCE).
- Files: 100 PVM + 10 PVR (VQ-mipmapped mostly), Ninja NJ/NJM/NJS, one custom uncompressed typed-record container
  format (`.BIN`/`.DAT`), 5 Manatee MLT banks + `MANATEE.DRV`, a Sofdec intro `PEN_OP.SFD`. No compression found.
- 1ST_READ.BIN: 2 287 804 bytes, unscrambled, 0x8C010000; ~7 000–7 500 functions (~3 400 game, ~3 800 SDK, split
  to confirm); compiler consistent with Hitachi SHC; no symbols.
- SDK: Shinobi 1.42, Ninja 01000086, Kamui 1.06, Manatee sd 1.00.18, gdFs 1.07, pd 1.46 / pdVib / pdLcd, bu 1.44,
  CRI Sofdec/ADX (mwSfd 1.28, ADXF 5.52, SFD 1.12).
- retrowser needs a `dreamcast` platform: disc random-access reads (multi-file pick, cue/gdi/chd), a disc
  fingerprint (IP.BIN or 1ST_READ SHA-1), `save.kind: vmu` 131072, DC pad bits + analog triggers (PLAN 6).
- Recommended: hand decomp (non-matching, differential tests) with SDK HLE, using a local-only static recompile as
  a scaffold/oracle (PLAN 7). Estimate ≈ 115–230 sessions.
- Survey script sources: `docs/port/survey/` (outputs stayed in the `penpen-work` volume).

Next: the user answers PLAN 11 (licence, retrowser platform work, Ghidra/rizin + emsdk downloads, scaffold); then
S1 tooling.
