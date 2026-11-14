# Pen Pen TriIcelon for the browser

A hand-written C decompilation of Pen Pen TriIcelon (Dreamcast, US). It runs as a
[retrowser](https://retrowser.app) module.

**Play:** https://retrowser.app/g/Sorikairox/penpen-web?ref=rtw-dist. You need your own copy of the game: pick
your disc archive (a `.7z` of the cue and bins) when asked. No game data is in this repository or in the module.

- `src/`: the game and the libraries it uses, as readable C.
- `port/`: the runtime (SH-4 interpreter used as the test oracle, HLE of the SDK, WebGL2 renderer, sound), the
  recompiler used only for differential testing, and the retrowser glue.
- `tools/`, `docker/`: the build and test tools. Everything that reads the disc runs in Docker without network.
- `docs/port/`: the plan, the decomp method (`DECOMP.md`) and session notes (`HANDOFF.md`).

Licence: MIT (our code). The game itself always comes from the player's disc.
