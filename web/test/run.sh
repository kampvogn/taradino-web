#!/bin/sh
# One player: serve web/stage (see stage.sh) and drive it with page.cjs in Playwright's Chromium.
# Needs playwright-core 1.62.1 in web/test/node_modules (npm i --prefix web/test playwright-core@1.62.1)
# or PW_NODE_MODULES. Screenshots go to web/shots. Env: ACTIONS, URL, VIEW, TOUCH, BROWSER, NOAUTOPLAY, STAGE
T=$(cd "$(dirname "$0")" && pwd); WEB=$(dirname "$T")
mkdir -p "$WEB/shots"
docker run --rm --shm-size=1g -e URL -e VIEW -e TOUCH -e BROWSER -e NOAUTOPLAY -e ACTIONS \
  -v "${PW_NODE_MODULES:-$T/node_modules}":/pw/node_modules -v "$T":/s -v "$WEB/shots":/out -v "${STAGE:-$WEB/stage}":/web \
  mcr.microsoft.com/playwright:v1.62.1-jammy sh -c "cd /web && (python3 -m http.server 8099 >/dev/null 2>&1 &) && sleep 1 && timeout 300 node /s/page.cjs"
