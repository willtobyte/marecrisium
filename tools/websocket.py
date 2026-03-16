#!/usr/bin/env python3

from __future__ import annotations

import asyncio
import json
import signal
from collections import defaultdict

import websockets

subscribers: dict[str, set] = defaultdict(set)


async def handler(socket) -> None:
    print(f"[+] connected: {socket.remote_address}")
    try:
        async for payload in socket:
            try:
                message = json.loads(payload)
            except json.JSONDecodeError:
                print(f"[!] invalid json: {payload!r}")
                continue

            action = message.get("action")
            topic = message.get("topic")

            if not action or not topic:
                continue

            match action:
                case "subscribe":
                    subscribers[topic].add(socket)
                    print(f"[>] {socket.remote_address} subscribed to '{topic}'")

                case "unsubscribe":
                    subscribers[topic].discard(socket)
                    print(f"[<] {socket.remote_address} unsubscribed from '{topic}'")

                case "publish":
                    data = message.get("data")
                    echo = json.dumps({"topic": topic, "data": data})
                    print(f"[~] publish on '{topic}': {data!r}")
                    targets = set(subscribers.get(topic, set())) - {socket}
                    if targets:
                        await asyncio.gather(*[t.send(echo) for t in targets])

    except websockets.ConnectionClosedOK:
        pass
    except websockets.ConnectionClosedError as e:
        print(f"[!] connection error: {e}")
    finally:
        for topic_subscribers in subscribers.values():
            topic_subscribers.discard(socket)
        print(f"[-] disconnected: {socket.remote_address}")


async def main() -> None:
    host = "localhost"
    port = 8080
    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGINT, stop.set_result, None)
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    print(f"[*] listening on ws://{host}:{port}")
    async with websockets.serve(handler, host, port):
        await stop
    print("[*] shutting down")


if __name__ == "__main__":
    asyncio.run(main())
