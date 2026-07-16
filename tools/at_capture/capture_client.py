#!/usr/bin/env python3
"""Client adapter for the device-side AT runner service.

Protocol summary:
- request: uint32 network-order JSON length + UTF-8 JSON command
- response: uint32 network-order JSON header length + UTF-8 JSON header + optional image bytes

The Windows AT MVP talks to the device-side at_device_runner service through this file.
AT core should not depend on the camera SDK or protocol details directly.
"""

from __future__ import annotations

import json
import socket
import struct
import threading
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable, Optional

import cv2
import numpy as np


@dataclass(frozen=True)
class CameraParams:
    exposure_us: int = 1000
    gain: int = 50
    focus: int = 30
    lights: tuple[int, int, int, int] = (1, 1, 1, 1)

    def to_request(self) -> dict[str, Any]:
        return {
            "exposure_us": self.exposure_us,
            "gain": self.gain,
            "focus": self.focus,
            "lights": list(self.lights),
        }


@dataclass
class CaptureFrame:
    image: np.ndarray
    image_bytes: bytes
    encoding: str
    params: CameraParams
    captured_at: str
    status: dict[str, Any]
    metadata: dict[str, Any]
    trace: dict[str, Any] | None = None
    at: dict[str, Any] | None = None


class CaptureServiceClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 8080, timeout: float = 10.0) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock: Optional[socket.socket] = None
        self.current_params = CameraParams()
        self.last_status: dict[str, Any] = {}
        self._request_lock = threading.Lock()

    @property
    def connected(self) -> bool:
        return self.sock is not None

    def connect(self) -> None:
        if self.sock is not None:
            return
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.get_status()

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    def get_status(self) -> dict[str, Any]:
        header, _ = self.request({"command": "get_status"})
        self._ensure_ok(header, "get_status")
        self.last_status = dict(header.get("status", {}))
        self.current_params = _params_from_status(self.last_status, self.current_params)
        return self.last_status

    def set_params(self, params: CameraParams) -> dict[str, Any]:
        command = {"command": "set_params", "params": params.to_request(), **params.to_request()}
        header, _ = self.request(command)
        self._ensure_ok(header, "set_params")
        self.current_params = params
        self.last_status = dict(header.get("status", {}))
        return self.last_status

    def close_lights(self) -> dict[str, Any]:
        header, _ = self.request({"command": "close_lights"})
        self._ensure_ok(header, "close_lights")
        self.last_status = dict(header.get("status", {}))
        self.current_params = _params_from_status(self.last_status, self.current_params)
        return self.last_status

    def capture(self, encoding: str = "png") -> CaptureFrame:
        header, image_bytes = self.request({"command": "capture", "encoding": encoding})
        self._ensure_ok(header, "capture")
        return self._frame_from_response(header, image_bytes, encoding, "capture")

    def capture_heatmap(self, encoding: str = "png", overlay: bool = False) -> CaptureFrame:
        header, image_bytes = self.request(
            {"command": "capture_heatmap", "encoding": encoding, "overlay": overlay}
        )
        self._ensure_ok(header, "capture_heatmap")
        return self._frame_from_response(header, image_bytes, encoding, "capture_heatmap")

    def reset_at(self, params: CameraParams | None = None) -> dict[str, Any]:
        command: dict[str, Any] = {"command": "reset_at"}
        if params is not None:
            command.update({"params": params.to_request(), **params.to_request()})
        header, _ = self.request(command)
        self._ensure_ok(header, "reset_at")
        self.last_status = dict(header.get("status", {}))
        self.current_params = _params_from_status(self.last_status, params or self.current_params)
        return dict(header.get("at", {}))

    def at_step(self, encoding: str = "png") -> CaptureFrame:
        header, image_bytes = self.request({"command": "at_step", "encoding": encoding})
        self._ensure_ok(header, "at_step")
        return self._frame_from_response(header, image_bytes, encoding, "at_step")

    def run_at_async(self, params: CameraParams, max_steps: int, preview_every: int = 0) -> dict[str, Any]:
        header, _ = self.request({
            "command": "run_at_async", "max_steps": max_steps, "preview_every": preview_every,
            "params": params.to_request(), **params.to_request(),
        })
        self._ensure_ok(header, "run_at_async")
        return dict(header.get("at", {}))

    def get_run_status(self) -> dict[str, Any]:
        header, _ = self.request({"command": "get_run_status"})
        self._ensure_ok(header, "get_run_status")
        return dict(header.get("at", {}))

    def stop_at_async(self) -> dict[str, Any]:
        header, _ = self.request({"command": "stop_at_async"})
        self._ensure_ok(header, "stop_at_async")
        return dict(header.get("at", {}))

    def get_preview(self) -> CaptureFrame | None:
        header, image_bytes = self.request({"command": "get_preview"})
        self._ensure_ok(header, "get_preview")
        if not image_bytes:
            return None
        return self._frame_from_response(header, image_bytes, "png", "get_preview")

    def save_frame(
        self,
        frame: CaptureFrame,
        output_dir: Path,
        note: str = "",
        filename_stem: str | None = None,
    ) -> Path:
        image_dir = output_dir / "images"
        image_dir.mkdir(parents=True, exist_ok=True)

        stamp = _filename_stamp(frame.captured_at)
        extension = frame.encoding if frame.encoding else "png"
        stem = filename_stem or f"capture_{stamp}"
        image_path = image_dir / f"{stem}.{extension}"
        image_path.write_bytes(frame.image_bytes)

        record = {
            "timestamp": frame.captured_at,
            "image_path": str(image_path.relative_to(output_dir)),
            "params": asdict(frame.params),
            "status": frame.status,
            "encoding": frame.encoding,
            "metadata": frame.metadata,
            "note": note,
        }
        if frame.trace is not None:
            record["trace"] = frame.trace
        if frame.at is not None:
            record["at"] = frame.at
        manifest = output_dir / "manifest.jsonl"
        manifest.parent.mkdir(parents=True, exist_ok=True)
        with manifest.open("a", encoding="utf-8") as fp:
            fp.write(json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n")
        return image_path

    def request(self, command: dict[str, Any]) -> tuple[dict[str, Any], bytes]:
        with self._request_lock:
            if self.sock is None:
                raise RuntimeError("capture service is not connected")

            payload = json.dumps(command, separators=(",", ":")).encode("utf-8")
            self.sock.sendall(struct.pack("!I", len(payload)))
            self.sock.sendall(payload)

            header_size = struct.unpack("!I", self._recv_exact(4))[0]
            header = json.loads(self._recv_exact(header_size).decode("utf-8"))
            image_size = int(header.get("image", {}).get("size", 0))
            image = self._recv_exact(image_size) if image_size else b""
            return header, image

    def _frame_from_response(
        self, header: dict[str, Any], image_bytes: bytes, fallback_encoding: str, event: str
    ) -> CaptureFrame:
        if not image_bytes:
            raise RuntimeError(f"{event} response did not include image data")

        buffer = np.frombuffer(image_bytes, dtype=np.uint8)
        image = cv2.imdecode(buffer, cv2.IMREAD_UNCHANGED)
        if image is None:
            raise RuntimeError(f"{event} returned invalid image bytes")

        self.last_status = dict(header.get("status", {}))
        self.current_params = _params_from_status(self.last_status, self.current_params)
        response_image = header.get("image", {})
        return CaptureFrame(
            image=image,
            image_bytes=image_bytes,
            encoding=str(response_image.get("encoding", fallback_encoding)),
            params=self.current_params,
            captured_at=datetime.now(timezone.utc).isoformat(),
            status=self.last_status,
            metadata={"host": self.host, "port": self.port, "protocol": "at-runner", "event": event},
            trace=header.get("trace") if isinstance(header.get("trace"), dict) else None,
            at=header.get("at") if isinstance(header.get("at"), dict) else None,
        )

    def _recv_exact(self, size: int) -> bytes:
        if self.sock is None:
            raise RuntimeError("capture service is not connected")
        chunks: list[bytes] = []
        remaining = size
        while remaining:
            chunk = self.sock.recv(remaining)
            if not chunk:
                raise RuntimeError("capture service disconnected")
            chunks.append(chunk)
            remaining -= len(chunk)
        return b"".join(chunks)

    @staticmethod
    def _ensure_ok(header: dict[str, Any], command: str) -> None:
        if header.get("ok"):
            return
        error = header.get("error", {})
        code = error.get("code", "unknown")
        message = error.get("message", "")
        raise RuntimeError(f"{command} failed: {code}: {message}")


def parse_lights(value: str) -> tuple[int, int, int, int]:
    items: Iterable[str] = value.replace(",", " ").split()
    values = tuple(int(x) for x in items)
    if len(values) != 4:
        raise ValueError("lights expects exactly four values")
    return values  # type: ignore[return-value]


def _params_from_status(status: dict[str, Any], fallback: CameraParams) -> CameraParams:
    params = status.get("params")
    if not isinstance(params, dict):
        return fallback
    try:
        return CameraParams(
            exposure_us=int(params.get("exposure_us", fallback.exposure_us)),
            gain=int(params.get("gain", fallback.gain)),
            focus=int(params.get("focus", fallback.focus)),
            lights=parse_lights(" ".join(str(v) for v in params.get("lights", fallback.lights))),
        )
    except Exception:
        return fallback


def _filename_stamp(timestamp: str) -> str:
    safe = timestamp.replace("+00:00", "Z")
    for old, new in ((":", ""), ("-", ""), (".", "")):
        safe = safe.replace(old, new)
    return safe
