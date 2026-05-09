#!/usr/bin/env python3
"""Capture-service client used by the AT engineering test loop.

The default protocol is intentionally small: a 4-byte network-order command
length followed by a UTF-8 command string. Image responses are a 4-byte length
followed by encoded image bytes. This matches the existing local prototype and
keeps the adapter replaceable when the real service path is provided.
"""

from __future__ import annotations

import json
import socket
import struct
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable, Optional

import cv2
import numpy as np


@dataclass
class CameraParams:
    exp_time: int = 1000
    exp_gain: int = 32
    focus_pos: int = 1
    lights: tuple[int, ...] = (1, 1, 1, 1)


@dataclass
class CaptureFrame:
    image: np.ndarray
    params: CameraParams
    captured_at: str
    metadata: dict


class CaptureServiceClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 8080, timeout: float = 5.0) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock: Optional[socket.socket] = None
        self.current_params = CameraParams()

    @property
    def connected(self) -> bool:
        return self.sock is not None

    def connect(self) -> None:
        if self.sock is not None:
            return
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(self.timeout)
        sock.connect((self.host, self.port))
        self.sock = sock

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    def send_command(self, command: str) -> None:
        if self.sock is None:
            raise RuntimeError("capture service is not connected")
        data = command.encode("utf-8")
        self.sock.sendall(struct.pack("!I", len(data)))
        self.sock.sendall(data)

    def set_params(self, params: CameraParams) -> None:
        """Set camera params using conservative, service-agnostic commands.

        The current known prototype supports SET_EXP_PARAMS and GET_FRAME.
        Focus/lights commands are emitted separately so the real service can
        support them without changing this client.
        """
        self.send_command(f"SET_EXP_PARAMS {params.exp_time} {params.exp_gain}")
        self._send_optional(f"SET_FOCUS_POS {params.focus_pos}")
        self._send_optional("SET_LIGHTS " + " ".join(str(x) for x in params.lights))
        self.current_params = params

    def capture(self) -> CaptureFrame:
        self.send_command("GET_FRAME")
        image_bytes = self._recv_blob()
        buffer = np.frombuffer(image_bytes, dtype=np.uint8)
        image = cv2.imdecode(buffer, cv2.IMREAD_UNCHANGED)
        if image is None:
            raise RuntimeError("capture service returned invalid image bytes")
        return CaptureFrame(
            image=image,
            params=self.current_params,
            captured_at=datetime.now().isoformat(timespec="seconds"),
            metadata={"host": self.host, "port": self.port},
        )

    def save_frame(self, frame: CaptureFrame, output_dir: Path) -> Path:
        output_dir.mkdir(parents=True, exist_ok=True)
        stamp = frame.captured_at.replace(":", "").replace("-", "")
        image_path = output_dir / f"capture_{stamp}.png"
        meta_path = output_dir / f"capture_{stamp}.json"
        cv2.imwrite(str(image_path), frame.image)
        meta = {
            "captured_at": frame.captured_at,
            "params": asdict(frame.params),
            "metadata": frame.metadata,
            "image": image_path.name,
        }
        meta_path.write_text(json.dumps(meta, indent=2), encoding="utf-8")
        return image_path

    def _send_optional(self, command: str) -> None:
        try:
            self.send_command(command)
        except OSError:
            raise
        except Exception:
            # Optional commands are allowed to be unsupported by early services.
            pass

    def _recv_blob(self) -> bytes:
        header = self._recv_exact(4)
        size = struct.unpack("!I", header)[0]
        if size <= 0:
            raise RuntimeError("capture service returned empty image")
        return self._recv_exact(size)

    def _recv_exact(self, size: int) -> bytes:
        if self.sock is None:
            raise RuntimeError("capture service is not connected")
        chunks: list[bytes] = []
        remaining = size
        while remaining > 0:
            chunk = self.sock.recv(remaining)
            if not chunk:
                raise RuntimeError("capture service disconnected")
            chunks.append(chunk)
            remaining -= len(chunk)
        return b"".join(chunks)


def parse_lights(value: str) -> tuple[int, ...]:
    items: Iterable[str] = value.replace(",", " ").split()
    return tuple(int(x) for x in items)

