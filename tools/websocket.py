#!/usr/bin/env python3

from __future__ import annotations

import asyncio
import json
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
                    match topic:
                        case "health":
                            print(f"[~] ping from {socket.remote_address}")
                            await socket.send(json.dumps({"action": "pong"}))

                        case _:
                            data = message.get("data")
                            echo_topic = f"{topic}_echo"
                            echo = json.dumps({"topic": echo_topic, "data": data})
                            print(
                                f"[~] publish on '{topic}', echoing to '{echo_topic}': {data!r}"
                            )
                            targets = set(subscribers.get(echo_topic, set()))
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
    print(f"[*] listening on ws://{host}:{port}")
    async with websockets.serve(handler, host, port):
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
