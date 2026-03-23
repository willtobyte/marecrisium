#!/usr/bin/env python3

from __future__ import annotations

import asyncio
import signal
import sys
from collections import defaultdict
from datetime import datetime, timezone

import cbor2

SUBSCRIBE = 1 << 0
UNSUBSCRIBE = 1 << 1
PUBLISH = 1 << 2

OPCODE_NAMES = {
    SUBSCRIBE: "SUBSCRIBE",
    UNSUBSCRIBE: "UNSUBSCRIBE",
    PUBLISH: "PUBLISH",
}

subscribers: dict[int, set] = defaultdict(set)


def log(tag: str, msg: str) -> None:
    ts = datetime.now(timezone.utc).strftime("%H:%M:%S.%f")[:-3]
    line = f"[{ts}] [{tag}] {msg}"
    print(line, flush=True)


def describe_opcode(opcode: int) -> str:
    flags = [name for flag, name in OPCODE_NAMES.items() if opcode & flag]
    return "|".join(flags) if flags else f"UNKNOWN(0x{opcode:02x})"


def dump_subscribers() -> None:
    if not subscribers:
        log("STATE", "subscribers: (empty)")
        return
    for topic, socks in subscribers.items():
        addrs = [str(s.remote_address) for s in socks]
        log("STATE", f"  topic={topic}: {addrs}")


async def handler(socket) -> None:
    log("+", f"connected: {socket.remote_address}  id={id(socket):#x}")
    dump_subscribers()
    try:
        async for payload in socket:
            log(
                "RAW",
                f"from {socket.remote_address}  {len(payload)} bytes: {payload[:120]!r}",
            )
            try:
                message = cbor2.loads(payload)
            except Exception as exc:
                log(
                    "!",
                    f"invalid cbor from {socket.remote_address}: {exc}  raw={payload!r}",
                )
                continue

            log("MSG", f"decoded: {message!r}")

            if not isinstance(message, list) or len(message) < 2:
                log(
                    "!",
                    f"dropping malformed message (not a list or len<2): {message!r}",
                )
                continue

            opcode = message[0]
            topic = message[1]

            if not isinstance(opcode, int) or not isinstance(topic, int):
                log(
                    "!",
                    f"dropping message: bad types opcode={type(opcode).__name__} topic={type(topic).__name__}",
                )
                continue

            log(
                "OP",
                f"{describe_opcode(opcode)} on topic={topic} from {socket.remote_address}",
            )

            if opcode & SUBSCRIBE:
                subscribers[topic].add(socket)
                log(
                    ">",
                    f"{socket.remote_address} subscribed to topic={topic}  (now {len(subscribers[topic])} subscriber(s))",
                )
                dump_subscribers()

            if opcode & UNSUBSCRIBE:
                subscribers[topic].discard(socket)
                log(
                    "<",
                    f"{socket.remote_address} unsubscribed from topic={topic}  (now {len(subscribers[topic])} subscriber(s))",
                )
                dump_subscribers()

            if opcode & PUBLISH:
                data = message[2] if len(message) >= 3 else None
                echo = cbor2.dumps([PUBLISH, topic, data])
                all_subs = subscribers.get(topic, set())
                targets = set(all_subs) - {socket}
                log(
                    "~",
                    f"publish on topic={topic}: {data!r}  subscribers={len(all_subs)} targets={len(targets)}",
                )
                if targets:
                    for t in targets:
                        log("->", f"forwarding to {t.remote_address}")
                    await asyncio.gather(*[t.send(echo) for t in targets])
                    log("~", f"forwarded to {len(targets)} client(s)")
                else:
                    log("~", f"no targets for topic={topic} (sender excluded)")

    except Exception as e:
        if "ConnectionClosed" not in type(e).__name__:
            import traceback

            log("!", f"connection error ({type(e).__name__}): {e}")
            traceback.print_exc(file=sys.stderr)
    finally:
        for topic, topic_subscribers in subscribers.items():
            if socket in topic_subscribers:
                topic_subscribers.discard(socket)
                log("-", f"removed {socket.remote_address} from topic={topic}")
        log("-", f"disconnected: {socket.remote_address}")
        dump_subscribers()


async def main() -> None:
    import websockets

    host = "localhost"
    port = 8080
    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGINT, stop.set_result, None)
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    log("*", f"listening on ws://{host}:{port}")
    log("*", f"websockets version: {websockets.__version__}")
    async with websockets.serve(handler, host, port):
        await stop
    log("*", "shutting down")


if __name__ == "__main__":
    asyncio.run(main())
