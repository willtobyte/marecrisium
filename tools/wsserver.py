#!/usr/bin/env python3

from __future__ import annotations

import asyncio
import json
from collections import defaultdict

import websockets

subscribers: dict[str, set] = defaultdict(set)


async def handler(ws) -> None:
    print(f"[+] connected: {ws.remote_address}")
    try:
        async for raw in ws:
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                print(f"[!] invalid json: {raw!r}")
                continue

            action = msg.get("action")
            topic = msg.get("topic")

            if not action or not topic:
                continue

            if action == "subscribe":
                subscribers[topic].add(ws)
                print(f"[>] {ws.remote_address} subscribed to '{topic}'")

            elif action == "unsubscribe":
                subscribers[topic].discard(ws)
                print(f"[<] {ws.remote_address} unsubscribed from '{topic}'")

            elif action == "publish":
                data = msg.get("data")
                echo_topic = f"{topic}_echo"
                echo = json.dumps({"topic": echo_topic, "data": data})
                print(f"[~] publish on '{topic}', echoing to '{echo_topic}': {data!r}")
                targets = set(subscribers.get(echo_topic, set()))
                if targets:
                    await asyncio.gather(*[t.send(echo) for t in targets])

    except websockets.ConnectionClosedOK:
        pass
    except websockets.ConnectionClosedError as e:
        print(f"[!] connection error: {e}")
    finally:
        for topic_subscribers in subscribers.values():
            topic_subscribers.discard(ws)
        print(f"[-] disconnected: {ws.remote_address}")


async def main() -> None:
    host = "localhost"
    port = 8080
    print(f"[*] listening on ws://{host}:{port}")
    async with websockets.serve(handler, host, port):
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
