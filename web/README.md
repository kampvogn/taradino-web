# Rise of the Triad in the browser

This branch (`browser`) of [taradino](https://github.com/fabiangreffrath/taradino) builds the game to
WebAssembly with Emscripten and adds network play in the browser. It runs at
<https://kampvogn.dk/rott/>.

- **Shareware episode** ("The HUNT Begins"): Apogee's package `1rott13.zip`, hosted unchanged as its
  licence requires (see `VENDOR.DOC` inside it). The page unpacks it in the browser.
- **Full version** (Dark War): players add their own `DARKWAR.WAD`/`.RTL` (and optionally `.RTC`,
  `REMOTE1.RTS`, `ROTTCD.RTC`). The files stay in the browser (Cache Storage).
- **Multiplayer (COMM-BAT)**: up to 5 players (shareware) or 11 (full version) through a small
  WebSocket relay. Everyone opens the same invite link and one player presses Start.

## Changes to taradino

The changes are the commits on this branch on top of upstream `main` (upstream's own `web` branch is an
earlier, separate Emscripten port that rewrites the main loop; this one keeps the game's loops and
uses Asyncify instead):

1. **Build to WebAssembly:** Emscripten's SDL2/SDL2_mixer ports in CMake; music defaults to OPL3 via
   libADLMIDI (SDL_mixer has no MIDI here); `IN_PumpEvents` yields to the browser every 15 ms, since
   several loops poll input without sleeping; fixed 640x480 window (the page scales it).
2. **Netplay transport** (`rt_com.c`): taradino has none. The page decides the roles before the game
   starts (`Module.rottnet`) and the game sends packets to "nodes" the way Apogee's ROTTIPX driver
   did: node 0 is yourself; for a client node 1 is the server; for the server node 1 is its own player
   and nodes 2.. the others. `UpdateClientControls` yields to the browser, because the game waits for
   packets in busy loops and WebSocket messages only arrive when the browser gets a turn.
3. **Wide screens (Hor+):** resolutions from 640x480 up to 1024x480. A full-width view is projected
   as if it were the original 640-wide one (`ProjectionCenterX` in `rt_view.c`), so the middle of the
   picture is unchanged and the extra columns show more to the sides; the angle table continues its
   atan curve past 45 degrees. The status bars reach the edges, and the 320x200 screens (menus,
   titles) and cinematics stay 4:3 in the middle. 640x480 renders exactly as before. The page picks
   the width from the window's shape (`Classic 4:3` keeps 640); game logic does not depend on it, so
   players with different widths can play together.
4. **Lobby choices** (`rt_menu.c`, `Web_AutoBattle`): the page's lobby picks codename, character and
   colour, and for the host battle mode, teams and level, so the game skips its COMM-BAT menus.
   The shareware version only allows Normal, Collector and Hunter.

## The page and the relay

`web/index.html` is the whole page: loading and unpacking the data, the full-version file import,
the lobby and the glue between the game and the relay. Messages in a relay room are
`[channel][data]`: 0 = lobby JSON (`here`, `start`), 1 = a game packet.

`web/relay/server.py` is the relay (Python, `websockets`). Every connection gets a random 4-byte id;
a client sends `[dest id][packet]` and the relay forwards `[source id][packet]` to that client or, for
`0xFFFFFFFF`, to everyone else in the room. It has no game logic, only limits so it is useless as a
general-purpose pipe: allowed `Origin`s (env `ORIGINS`, comma-separated), 12 connections per IP
(from `X-Real-IP`), 64 KB/s out per client (packet size × recipients), 4 hours per connection and
15 minutes idle. It logs one JSON line per connection. The same relay serves Red Alert
([vanilla-conquer-web](https://github.com/kampvogn/vanilla-conquer-web)), hence the path
`/redalert/net` the page connects to; put it behind your web server's TLS, e.g. for nginx:

```nginx
location = /redalert/net {
    proxy_pass http://relay:8081;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header X-Real-IP $remote_addr;
    proxy_read_timeout 3600s;
}
```

## Building

Everything runs in Docker:

```sh
docker build -t taradino-emsdk -f web/Dockerfile.emsdk web   # Emscripten 4.0.10 + SDL2
web/build-deps.sh                                             # libADLMIDI → web/deps
web/build-wasm.sh                                             # shareware → build-wasm/
SHAREWARE=OFF BUILD_DIR=build-full web/build-wasm.sh          # full version → build-full/
web/stage.sh                                                  # page + builds + data → web/stage/
```

Serve `web/stage/` with any static web server. The `.wasm` files should be served as
`application/wasm`. Bump `taradino.js?v=N` in `index.html` when a cached copy must be replaced.

## Tests

The tests drive real browsers with Playwright in Docker; screenshots go to `web/shots/`.

```sh
npm i --prefix web/test playwright-core@1.62.1
ACTIONS="click:0.5,0.5;wait:20000;shot:game" URL=http://localhost:8099/ web/test/run.sh
PLAYERS=3 ACTIONS="both:js:document.getElementById('mpopen').click();all:wait:4000;A:js:document.getElementById('mpstart').click();all:wait:20000;both:key:Enter;all:wait:4000;both:shot:mp" web/test/mp.sh
```

`mp.sh` starts a relay container and two or three browsers on a private Docker network. The
`stack` step (and `hang.cjs`) pauses a page and prints the WebAssembly call stack; build with
`EXTRA_LDFLAGS=--profiling-funcs` to get function names. `ACTIONS` is split on `;`, so JavaScript in
a `js:` step cannot contain one.

## Licences

Taradino is GPL-2.0-or-later (see `COPYING`), and so are these changes. libADLMIDI is LGPL-2.1+/GPL.
Rise of the Triad's data is © Apogee Software; it is not part of this repository.
