#!/usr/bin/env python3
"""Capture a full device-side AT session and annotate frames locally.

The runner does no rendering work: it returns its normal PNG frame and JSON trace.
This client adds the information panel and ROI box on the host before saving images.
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import time
from datetime import datetime
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFont


def recv_exact(sock: socket.socket, size: int) -> bytes:
    chunks: list[bytes] = []
    remaining = size
    while remaining:
        chunk = sock.recv(remaining)
        if not chunk:
            raise RuntimeError("device disconnected")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def request(sock: socket.socket, command: dict[str, Any]) -> tuple[dict[str, Any], bytes]:
    payload = json.dumps(command, separators=(",", ":")).encode("utf-8")
    sock.sendall(struct.pack("!I", len(payload)) + payload)
    header_size = struct.unpack("!I", recv_exact(sock, 4))[0]
    header = json.loads(recv_exact(sock, header_size).decode("utf-8"))
    image_size = int(header.get("image", {}).get("size", 0))
    return header, recv_exact(sock, image_size) if image_size else b""


def font(size: int) -> ImageFont.ImageFont:
    candidates = [
        Path(r"C:\Windows\Fonts\consola.ttf"),
        Path(r"C:\Windows\Fonts\arial.ttf"),
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def annotate(image_bytes: bytes, trace: dict[str, Any], host_request_ms: float) -> Image.Image:
    from io import BytesIO

    image = Image.open(BytesIO(image_bytes)).convert("RGB")
    draw = ImageDraw.Draw(image, "RGBA")
    text_font = font(max(20, image.width // 64))
    small_font = font(max(18, image.width // 80))
    quality = trace.get("quality", {})
    heatmap = trace.get("heatmap", {})
    heatmap_perf = trace.get("heatmap_perf", {})
    current = trace.get("current_params", {})
    next_params = trace.get("next_params", {})

    lines = [
        f"AT step {trace.get('step')} | {trace.get('phase')} | {trace.get('action')}",
        f"focus {current.get('focus')} -> {next_params.get('focus')} | "
        f"exp {current.get('exposure_us')} -> {next_params.get('exposure_us')} | "
        f"gain {current.get('gain')} -> {next_params.get('gain')}",
        f"lights {current.get('lights')} -> {next_params.get('lights')}",
        f"brightness {quality.get('brightness', 0):.1f} | sharpness {quality.get('sharpness', 0):.1f} | "
        f"contrast {quality.get('contrast', 0):.1f}",
        f"time host-request {host_request_ms:.1f} ms | model {heatmap_perf.get('last_model_ms', 0):.1f} ms | "
        f"model-total {heatmap_perf.get('last_total_ms', 0):.1f} ms",
        f"detect {heatmap.get('available')} | conf {heatmap.get('confidence', 0):.3f} | "
        f"reason: {trace.get('reason', '')}",
    ]

    padding = max(16, image.width // 100)
    line_height = int(text_font.size * 1.35)
    panel_height = padding * 2 + line_height * len(lines)
    panel_width = image.width - padding * 2
    draw.rounded_rectangle(
        (padding, padding, padding + panel_width, padding + panel_height),
        radius=padding,
        fill=(0, 0, 0, 185),
    )
    y = padding * 2
    for index, line in enumerate(lines):
        draw.text((padding * 2, y), line, font=text_font if index < 2 else small_font, fill="white")
        y += line_height

    roi = heatmap.get("roi") or {}
    if heatmap.get("available") and roi.get("width", 0) > 0 and roi.get("height", 0) > 0:
        box = (roi["x"], roi["y"], roi["x"] + roi["width"], roi["y"] + roi["height"])
        draw.rectangle(box, outline=(0, 255, 80, 255), width=max(3, image.width // 400))
        label = f"ROI conf={heatmap.get('confidence', 0):.3f}"
        draw.text((box[0], max(panel_height + padding * 2, box[1] - line_height)), label,
                  font=small_font, fill=(0, 255, 80, 255), stroke_width=1, stroke_fill="black")
    return image


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.1.201")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--max-steps", type=int, default=48)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = args.output_dir or Path("captures") / f"at_device_{timestamp}"
    image_dir = output_dir / "images"
    image_dir.mkdir(parents=True, exist_ok=True)

    with socket.create_connection((args.host, args.port), timeout=15) as sock:
        sock.settimeout(90)
        reset, _ = request(sock, {"command": "reset_at", "exposure_us": 1000, "gain": 50,
                                  "focus": 30, "lights": [1, 1, 1, 1]})
        if not reset.get("ok"):
            raise RuntimeError(reset)

        records: list[dict[str, Any]] = []
        for _ in range(args.max_steps):
            step_start = time.perf_counter()
            response, image_bytes = request(sock, {"command": "at_step", "encoding": "png"})
            host_request_ms = (time.perf_counter() - step_start) * 1000.0
            if not response.get("ok"):
                raise RuntimeError(response)
            trace = response.get("trace", {})
            step = int(trace.get("step", len(records) + 1))
            filename = f"{step:03d}_{trace.get('phase', 'unknown')}_{trace.get('action', 'unknown')}.png"
            image_path = image_dir / filename
            annotation_start = time.perf_counter()
            annotate(image_bytes, trace, host_request_ms).save(image_path, "PNG", optimize=True)
            local_annotation_and_save_ms = (time.perf_counter() - annotation_start) * 1000.0
            timing = {
                "host_request_ms": round(host_request_ms, 3),
                "model_infer_ms": trace.get("heatmap_perf", {}).get("last_model_ms"),
                "model_total_ms": trace.get("heatmap_perf", {}).get("last_total_ms"),
                "local_annotation_and_save_ms": round(local_annotation_and_save_ms, 3),
                "end_to_end_ms": round(host_request_ms + local_annotation_and_save_ms, 3),
            }
            record = {"image": str(Path("images") / filename), "trace": trace,
                      "at": response.get("at", {}), "timing": timing}
            records.append(record)
            print(f"saved {image_path} | {host_request_ms:.1f} ms request + "
                  f"{local_annotation_and_save_ms:.1f} ms local | {trace.get('reason')}")
            if response.get("at", {}).get("finished"):
                break

    (output_dir / "trace.json").write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"complete: {len(records)} annotated frames -> {output_dir}")


if __name__ == "__main__":
    main()
