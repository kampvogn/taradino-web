"""WebSocket relay for network games on kampvogn.dk (Red Alert, Rise of the Triad).

A virtual LAN per room: every connection gets a random 4-byte id (used by the
game like an IPv4 address). Binary messages from a client are
[dest id (4)][packet]; the relay forwards [source id (4)][packet] to the
destination, or to everyone else in the room when dest is 0xFFFFFFFF.
No game logic here, only forwarding with limits: games send a few KB/s, so
the limits keep the relay useless as a general-purpose pipe.
"""

import asyncio
import json
import logging
import os
import random
import re
import struct
import time
from urllib.parse import parse_qs, urlparse

from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed

BROADCAST = 0xFFFFFFFF
MAX_PACKET = 2100          # Red Alert: 1024 (WinsockBufferType); ROTT: 2048 + CRC + channel byte
MAX_PER_ROOM = 12          # RA: 8 players; ROTT: 11
MAX_ROOMS = 200
MAX_PER_IP = 12            # a LAN party behind one NAT
MAX_RATE = 1000            # packets per second per client (RA lockstep ~15-60; a ROTT server sends one per player)
BYTE_RATE = 64_000         # bytes per second a client may make the relay send (packet size x recipients)
BYTE_BURST = 256_000       # short bursts above BYTE_RATE (game start, level load)
MAX_SESSION = 4 * 3600     # seconds per connection
IDLE = 15 * 60             # seconds without a message from the client
ORIGINS = [o or None for o in os.environ.get(
    'ORIGINS', 'https://kampvogn.dk,https://www.kampvogn.dk').split(',')]   # empty entry = no Origin header

rooms = {}                 # room name -> {id: websocket}
per_ip = {}                # client ip -> open connections


def log(**kw):
    print(json.dumps({'t': time.strftime('%Y-%m-%dT%H:%M:%S'), **kw}), flush=True)


def room_name(path):
    q = parse_qs(urlparse(path).query)
    name = (q.get('room') or ['lobby'])[0].lower()
    return name if re.fullmatch(r'[a-z0-9-]{1,40}', name) else 'lobby'


async def relay(ws, room, cid, stats):
    window, count = time.monotonic(), 0
    tokens, last = BYTE_BURST, time.monotonic()
    async for msg in ws:
        stats['last'] = now = time.monotonic()
        if not isinstance(msg, bytes) or not 4 < len(msg) <= 4 + MAX_PACKET:
            continue
        stats['in'] += len(msg)
        if now - window >= 1:
            stats['peak'] = max(stats['peak'], count)
            window, count = now, 0
        count += 1
        if count > MAX_RATE:
            stats['drop'] += 1
            continue
        dest, = struct.unpack('>I', msg[:4])
        if dest == BROADCAST:
            targets = [w for i, w in room.items() if i != cid]
        else:
            targets = [room[dest]] if dest in room else []
        cost = (len(msg) + 4) * len(targets)
        tokens = min(BYTE_BURST, tokens + (now - last) * BYTE_RATE)
        last = now
        if cost > tokens:
            stats['drop'] += 1
            continue
        tokens -= cost
        stats['out'] += cost
        out = struct.pack('>I', cid) + msg[4:]
        for w in targets:
            try:
                await w.send(out)
            except ConnectionClosed:
                pass


async def watchdog(ws, stats):
    while True:
        await asyncio.sleep(30)
        now = time.monotonic()
        if now - stats['start'] > MAX_SESSION:
            stats['why'] = 'session limit'
        elif now - stats['last'] > IDLE:
            stats['why'] = 'idle'
        else:
            continue
        await ws.close(1008, stats['why'])
        return


async def handler(ws):
    ip = ws.request.headers.get('X-Real-IP') or ws.remote_address[0]
    name = room_name(ws.request.path)
    if per_ip.get(ip, 0) >= MAX_PER_IP:
        log(ev='reject', ip=ip, room=name, why='ip limit')
        await ws.close(1013, 'too many connections')
        return
    room = rooms.get(name)
    if room is None:
        if len(rooms) >= MAX_ROOMS:
            log(ev='reject', ip=ip, room=name, why='too many rooms')
            await ws.close(1013, 'too many rooms')
            return
        room = rooms[name] = {}
    if len(room) >= MAX_PER_ROOM:
        log(ev='reject', ip=ip, room=name, why='room full')
        await ws.close(1013, 'room full')
        return
    while True:
        cid = random.randint(1, BROADCAST - 1)
        if cid not in room:
            break
    room[cid] = ws
    per_ip[ip] = per_ip.get(ip, 0) + 1
    start = time.monotonic()
    stats = {'start': start, 'last': start, 'in': 0, 'out': 0, 'drop': 0, 'peak': 0, 'why': 'closed'}
    dog = asyncio.create_task(watchdog(ws, stats))
    try:
        await ws.send(json.dumps({'type': 'hello', 'id': cid, 'room': name, 'peers': len(room) - 1}))
        await relay(ws, room, cid, stats)
    except ConnectionClosed:
        pass
    finally:
        dog.cancel()
        room.pop(cid, None)
        if not room:
            rooms.pop(name, None)
        per_ip[ip] -= 1
        if not per_ip[ip]:
            del per_ip[ip]
        secs = time.monotonic() - start
        log(ev='close', ip=ip, room=name, secs=round(secs), kb_in=stats['in'] // 1024, kb_out=stats['out'] // 1024,
            out_rate=round(stats['out'] / max(secs, 1)), peak_pps=stats['peak'], drop=stats['drop'], why=stats['why'])


async def main():
    # refused handshakes (wrong Origin, scanners) would log a traceback each
    logging.getLogger('websockets.server').setLevel(logging.CRITICAL)
    async with serve(handler, '0.0.0.0', int(os.environ.get('PORT', '8081')), origins=ORIGINS,
                     max_size=8192, ping_interval=20, ping_timeout=20):
        await asyncio.Future()


if __name__ == '__main__':
    asyncio.run(main())
