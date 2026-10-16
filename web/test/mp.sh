#!/bin/sh
# Multiplayer: a relay container (web/relay) and two or three browsers on a private docker network.
# Example: PLAYERS=2 ACTIONS="both:js:document.getElementById('mpopen').click();all:wait:4000;A:js:document.getElementById('mpstart').click();all:wait:20000;both:key:Enter;all:wait:4000;both:shot:mp" web/test/mp.sh
# Env: ACTIONS, PLAYERS (2), VERBOSE (all console output), ROOM, STAGE, LOGLINES (relay log lines, 5), PW_NODE_MODULES
T=$(cd "$(dirname "$0")" && pwd); WEB=$(dirname "$T")
mkdir -p "$WEB/shots"
docker build -q -t taradino-relay "$WEB/relay" >/dev/null
docker network create rott-test >/dev/null 2>&1
docker rm -f rott-relay-test >/dev/null 2>&1
docker run -d --rm --name rott-relay-test --network rott-test -e ORIGINS=http://localhost:8099 taradino-relay >/dev/null
docker run --rm --shm-size=2g --network rott-test -e ACTIONS -e PLAYERS -e VERBOSE -e URL="http://localhost:8099/?net=ws://rott-relay-test:8081/redalert/net${ROOM:+#room=$ROOM}" \
  -v "${PW_NODE_MODULES:-$T/node_modules}":/pw/node_modules -v "$T":/s -v "$WEB/shots":/out -v "${STAGE:-$WEB/stage}":/web \
  mcr.microsoft.com/playwright:v1.62.1-jammy sh -c "cd /web && (python3 -m http.server 8099 >/dev/null 2>&1 &) && sleep 1 && timeout 400 node /s/mp.cjs"
docker logs rott-relay-test 2>&1 | tail -${LOGLINES:-5}
docker stop rott-relay-test >/dev/null
