#!/usr/bin/env python3

import asyncio
import signal
import sys
from collections import defaultdict
from datetime import datetime, timezone

import json

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


def do_subscribe(socket, topic: int) -> None:
    subscribers[topic].add(socket)
    log(
        ">",
        f"{socket.remote_address} subscribed to topic={topic}  (now {len(subscribers[topic])} subscriber(s))",
    )
    dump_subscribers()


def do_unsubscribe(socket, topic: int) -> None:
    subscribers[topic].discard(socket)
    log(
        "<",
        f"{socket.remote_address} unsubscribed from topic={topic}  (now {len(subscribers[topic])} subscriber(s))",
    )
    dump_subscribers()


async def do_publish(socket, topic: int, message: list) -> None:
    data = message[2] if len(message) >= 3 else None
    echo = json.dumps([PUBLISH, topic, data])
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


async def handler(socket) -> None:
    log("+", f"connected: {socket.remote_address}  id={id(socket):#x}")
    dump_subscribers()
    try:
        async for payload in socket:
            log(
                "RAW",
                f"from {socket.remote_address}  {len(payload)} bytes: {payload[:120]!r}",
            )

            match payload:
                case bytes() | str() as raw if not raw:
                    log("!", f"empty payload from {socket.remote_address}")
                    continue
                case _:
                    pass

            try:
                message = json.loads(payload)
            except Exception as exc:
                log(
                    "!",
                    f"invalid json from {socket.remote_address}: {exc}  raw={payload!r}",
                )
                continue

            log("MSG", f"decoded: {message!r}")

            match message:
                case [int() as opcode, int() as topic, *_rest]:
                    pass
                case [_, _, *_]:
                    log(
                        "!",
                        f"dropping message: bad types opcode={type(message[0]).__name__} topic={type(message[1]).__name__}",
                    )
                    continue
                case _:
                    log(
                        "!",
                        f"dropping malformed message (not a list or len<2): {message!r}",
                    )
                    continue

            log(
                "OP",
                f"{describe_opcode(opcode)} on topic={topic} from {socket.remote_address}",
            )

            match opcode:
                case x if x == SUBSCRIBE:
                    do_subscribe(socket, topic)
                case x if x == UNSUBSCRIBE:
                    do_unsubscribe(socket, topic)
                case x if x == PUBLISH:
                    await do_publish(socket, topic, message)
                case x if x == SUBSCRIBE | UNSUBSCRIBE:
                    do_subscribe(socket, topic)
                    do_unsubscribe(socket, topic)
                case x if x == SUBSCRIBE | PUBLISH:
                    do_subscribe(socket, topic)
                    await do_publish(socket, topic, message)
                case x if x == UNSUBSCRIBE | PUBLISH:
                    do_unsubscribe(socket, topic)
                    await do_publish(socket, topic, message)
                case x if x == SUBSCRIBE | UNSUBSCRIBE | PUBLISH:
                    do_subscribe(socket, topic)
                    do_unsubscribe(socket, topic)
                    await do_publish(socket, topic, message)
                case _:
                    log("!", f"unknown opcode: 0x{opcode:02x}")

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
