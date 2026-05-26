#!/usr/bin/env python3
"""Collect heatmap validation samples through at_device_runner."""

from __future__ import annotations

import argparse
import itertools
import json
import time
from dataclasses import asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

from capture_client import CameraParams, CaptureServiceClient


def parse_range(value: str) -> list[int]:
    parts = [int(part.strip()) for part in value.split(":")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("range expects START:END:STEP")
    start, end, step = parts
    if step == 0:
        raise argparse.ArgumentTypeError("range step cannot be 0")
    if start < end and step < 0:
        raise argparse.ArgumentTypeError("range step must be positive when START < END")
    if start > end and step > 0:
        raise argparse.ArgumentTypeError("range step must be negative when START > END")

    result: list[int] = []
    current = start
    if step > 0:
        while current <= end:
            result.append(current)
            current += step
    else:
        while current >= end:
            result.append(current)
            current += step
    return result


def parse_int_list(value: str) -> list[int]:
    normalized = value.replace(";", ",").replace(" ", ",")
    items = [item.strip() for item in normalized.split(",") if item.strip()]
    if not items:
        raise argparse.ArgumentTypeError("list cannot be empty")
    return [int(item) for item in items]


def parse_light_combo(value: str) -> tuple[int, int, int, int]:
    text = value.strip()
    if len(text) == 4 and all(ch in "01" for ch in text):
        return tuple(int(ch) for ch in text)  # type: ignore[return-value]

    normalized = text.replace(",", " ").replace("_", " ")
    items = [item.strip() for item in normalized.split() if item.strip()]
    if len(items) != 4:
        raise argparse.ArgumentTypeError("light combo expects 1111 or four values")
    combo = tuple(1 if int(item) else 0 for item in items)
    return combo  # type: ignore[return-value]


def parse_light_combos(value: str) -> list[tuple[int, int, int, int]]:
    separators = value.replace("|", ";")
    chunks = [chunk.strip() for chunk in separators.split(";") if chunk.strip()]
    if not chunks:
        raise argparse.ArgumentTypeError("lights cannot be empty")
    return [parse_light_combo(chunk) for chunk in chunks]


def all_light_combos() -> list[tuple[int, int, int, int]]:
    return [tuple(values) for values in itertools.product((0, 1), repeat=4)]  # type: ignore[list-item]


def light_code(lights: tuple[int, int, int, int]) -> str:
    return "".join("1" if value else "0" for value in lights)


def sample_filename_stem(index: int, sample_count: int, params: CameraParams) -> str:
    sample_width = max(4, len(str(sample_count)))
    exposure = max(0, min(100000, int(params.exposure_us)))
    gain = max(0, min(999, int(params.gain)))
    focus = max(0, min(9999, int(params.focus)))
    return (
        f"s{index + 1:0{sample_width}d}_"
        f"e{exposure:06d}_g{gain:03d}_f{focus:04d}_l{light_code(params.lights)}"
    )


def axis_values(fixed: int, range_values: list[int] | None, list_values: list[int] | None = None) -> list[int]:
    if range_values is not None:
        return range_values
    if list_values is not None:
        return list_values
    return [fixed]


def build_plan(args: argparse.Namespace) -> list[tuple[CameraParams, dict]]:
    exposures = axis_values(args.exposure, args.exposure_range)
    gains = axis_values(args.gain, args.gain_range, args.gains)
    focuses = axis_values(args.focus, args.focus_range)
    lights = all_light_combos() if args.all_lights else parse_light_combos(args.lights)

    samples: list[tuple[CameraParams, dict]] = []
    for repeat in range(args.repeat):
        for light_combo in lights:
            for gain in gains:
                for exposure in exposures:
                    for focus in focuses:
                        params = CameraParams(
                            exposure_us=exposure,
                            gain=gain,
                            focus=focus,
                            lights=light_combo,
                        )
                        axes = {
                            "repeat": repeat,
                            "exposure_us": exposure,
                            "gain": gain,
                            "focus": focus,
                            "lights": list(light_combo),
                        }
                        samples.append((params, axes))
    return samples


def write_plan(args: argparse.Namespace, samples: list[tuple[CameraParams, dict]]) -> None:
    args.output_dir.mkdir(parents=True, exist_ok=True)
    plan = {
        "created_at": datetime.now(timezone.utc).isoformat(),
        "host": args.host,
        "port": args.port,
        "sample_count": len(samples),
        "capture_mode": "capture_heatmap" if args.with_heatmap else "capture",
        "overlay": args.overlay,
        "settle_ms": args.settle_ms,
        "repeat": args.repeat,
        "fixed": {
            "exposure_us": args.exposure,
            "gain": args.gain,
            "focus": args.focus,
            "lights": args.lights,
        },
        "ranges": {
            "exposure_range": args.exposure_range,
            "gain_range": args.gain_range,
            "gains": args.gains,
            "focus_range": args.focus_range,
            "all_lights": args.all_lights,
        },
        "note": args.note,
    }
    (args.output_dir / "sweep_plan.json").write_text(
        json.dumps(plan, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def run_capture(args: argparse.Namespace) -> None:
    samples = build_plan(args)
    write_plan(args, samples)
    print(f"planned samples: {len(samples)}")

    if args.dry_run:
        for index, (params, axes) in enumerate(samples[: args.print_limit]):
            stem = sample_filename_stem(index, len(samples), params)
            print(f"{index:04d} {stem}.png {asdict(params)} axes={axes}")
        if len(samples) > args.print_limit:
            print(f"... {len(samples) - args.print_limit} more")
        return

    client = CaptureServiceClient(host=args.host, port=args.port, timeout=args.timeout)
    client.connect()
    try:
        for index, (params, axes) in enumerate(samples):
            client.set_params(params)
            if args.settle_ms > 0:
                time.sleep(args.settle_ms / 1000.0)

            if args.with_heatmap:
                frame = client.capture_heatmap(overlay=args.overlay)
                capture_mode = "capture_heatmap"
            else:
                frame = client.capture()
                capture_mode = "capture"

            frame.metadata["dataset"] = {
                "sample_id": index,
                "sample_count": len(samples),
                "filename_stem": sample_filename_stem(index, len(samples), params),
                "capture_mode": capture_mode,
                "requested_params": params.to_request(),
                "axes": axes,
                "overlay": args.overlay,
                "settle_ms": args.settle_ms,
            }
            path = client.save_frame(
                frame,
                args.output_dir,
                note=args.note,
                filename_stem=frame.metadata["dataset"]["filename_stem"],
            )
            heatmap = (frame.trace or {}).get("heatmap") if frame.trace else None
            confidence = heatmap.get("confidence") if isinstance(heatmap, dict) else None
            print(
                f"[{index + 1}/{len(samples)}] saved={path.name} "
                f"exp={params.exposure_us} gain={params.gain} focus={params.focus} "
                f"lights={light_code(params.lights)} confidence={confidence}"
            )
    finally:
        client.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Collect heatmap validation dataset")
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--output-dir", type=Path, required=True)

    parser.add_argument("--exposure", type=int, default=1000)
    parser.add_argument("--exposure-range", type=parse_range)
    parser.add_argument("--gain", type=int, default=50)
    parser.add_argument("--gain-range", type=parse_range)
    parser.add_argument("--gains", type=parse_int_list)
    parser.add_argument("--focus", type=int, default=30)
    parser.add_argument("--focus-range", type=parse_range)
    parser.add_argument("--lights", default="1111")
    parser.add_argument("--all-lights", action="store_true")

    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--settle-ms", type=int, default=120)
    parser.add_argument("--with-heatmap", action="store_true", help="run heatmap inference while capturing")
    parser.add_argument("--overlay", action="store_true", help="with --with-heatmap, save heatmap overlay image")
    parser.add_argument(
        "--no-heatmap",
        action="store_false",
        dest="with_heatmap",
        help="capture raw image only; this is the default",
    )
    parser.add_argument("--note", default="")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--print-limit", type=int, default=20)
    args = parser.parse_args()

    if args.repeat < 1:
        raise SystemExit("--repeat must be >= 1")
    if args.overlay and not args.with_heatmap:
        raise SystemExit("--overlay requires --with-heatmap")
    run_capture(args)


if __name__ == "__main__":
    main()
