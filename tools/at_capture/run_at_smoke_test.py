#!/usr/bin/env python3
"""Headless smoke test for at_device_runner --server."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "at_capture"))

from capture_client import CameraParams, CaptureServiceClient  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke test at_device_runner service")
    parser.add_argument("--host", default="10.80.184.167")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--steps", type=int, default=3)
    args = parser.parse_args()

    client = CaptureServiceClient(host=args.host, port=args.port, timeout=30.0)
    try:
        client.connect()
        status = client.get_status()
        if not status.get("opened"):
            print("FAIL: camera not opened", status, file=sys.stderr)
            return 1
        print("OK get_status", status.get("device"), status.get("width"), "x", status.get("height"))

        params = CameraParams()
        at_state = client.reset_at(params)
        print("OK reset_at", at_state)

        for i in range(args.steps):
            frame = client.at_step()
            trace = frame.trace or {}
            at_info = frame.at or {}
            print(
                f"OK at_step[{i}] step={trace.get('step')} "
                f"action={trace.get('action')} finished={at_info.get('finished')}"
            )
            if at_info.get("finished"):
                break

        client.request({"command": "shutdown"})
        print("PASS smoke test")
        return 0
    except Exception as exc:  # noqa: BLE001
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    finally:
        client.close()


if __name__ == "__main__":
    raise SystemExit(main())
