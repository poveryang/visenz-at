#!/usr/bin/env python3
"""Minimal desktop capture tool for AT engineering tests."""

from __future__ import annotations

import argparse
import json
import tkinter as tk
from pathlib import Path
from tkinter import ttk

import cv2
from PIL import Image, ImageTk

from capture_client import CameraParams, CaptureServiceClient, parse_lights


class ATMvpGui(tk.Tk):
    def __init__(self, host: str, port: int, output_dir: Path) -> None:
        super().__init__()
        self.title("AT Capture MVP")
        self.geometry("980x720")
        self.output_dir = output_dir
        self.trace_path = output_dir / "capture_trace.jsonl"
        self.client = CaptureServiceClient(host=host, port=port)
        self.last_frame = None
        self.preview = None

        self.host_var = tk.StringVar(value=host)
        self.port_var = tk.IntVar(value=port)
        self.exp_var = tk.IntVar(value=1000)
        self.gain_var = tk.IntVar(value=32)
        self.focus_var = tk.IntVar(value=1)
        self.lights_var = tk.StringVar(value="1 1 1 1")
        self.status_var = tk.StringVar(value="Disconnected")

        self._build_layout()

    def _build_layout(self) -> None:
        root = ttk.Frame(self, padding=10)
        root.pack(fill=tk.BOTH, expand=True)

        controls = ttk.LabelFrame(root, text="Capture Service")
        controls.pack(fill=tk.X)

        ttk.Label(controls, text="Host").grid(row=0, column=0, sticky=tk.W, padx=4, pady=4)
        ttk.Entry(controls, textvariable=self.host_var, width=18).grid(row=0, column=1, padx=4, pady=4)
        ttk.Label(controls, text="Port").grid(row=0, column=2, sticky=tk.W, padx=4, pady=4)
        ttk.Entry(controls, textvariable=self.port_var, width=8).grid(row=0, column=3, padx=4, pady=4)
        ttk.Button(controls, text="Connect", command=self.connect).grid(row=0, column=4, padx=4, pady=4)
        ttk.Label(controls, textvariable=self.status_var).grid(row=0, column=5, sticky=tk.W, padx=8, pady=4)

        params = ttk.LabelFrame(root, text="Camera Params")
        params.pack(fill=tk.X, pady=8)
        fields = [
            ("Exposure", self.exp_var),
            ("Gain", self.gain_var),
            ("Focus", self.focus_var),
        ]
        for idx, (label, var) in enumerate(fields):
            ttk.Label(params, text=label).grid(row=0, column=idx * 2, sticky=tk.W, padx=4, pady=4)
            ttk.Entry(params, textvariable=var, width=10).grid(row=0, column=idx * 2 + 1, padx=4, pady=4)
        ttk.Label(params, text="Lights").grid(row=0, column=6, sticky=tk.W, padx=4, pady=4)
        ttk.Entry(params, textvariable=self.lights_var, width=18).grid(row=0, column=7, padx=4, pady=4)
        ttk.Button(params, text="Set Params", command=self.set_params).grid(row=0, column=8, padx=4, pady=4)
        ttk.Button(params, text="Capture", command=self.capture).grid(row=0, column=9, padx=4, pady=4)
        ttk.Button(params, text="Save Last", command=self.save_last).grid(row=0, column=10, padx=4, pady=4)

        preview_frame = ttk.LabelFrame(root, text="Preview")
        preview_frame.pack(fill=tk.BOTH, expand=True)
        self.preview_label = ttk.Label(preview_frame, anchor=tk.CENTER)
        self.preview_label.pack(fill=tk.BOTH, expand=True)

    def connect(self) -> None:
        self.client.close()
        self.client = CaptureServiceClient(host=self.host_var.get(), port=int(self.port_var.get()))
        try:
            self.client.connect()
            self.status_var.set("Connected")
        except Exception as exc:
            self.status_var.set(f"Connect failed: {exc}")

    def current_params(self) -> CameraParams:
        return CameraParams(
            exp_time=int(self.exp_var.get()),
            exp_gain=int(self.gain_var.get()),
            focus_pos=int(self.focus_var.get()),
            lights=parse_lights(self.lights_var.get()),
        )

    def set_params(self) -> None:
        try:
            params = self.current_params()
            self.client.set_params(params)
            self.status_var.set("Params sent")
            self._write_trace({"event": "set_params", "params": params.__dict__})
        except Exception as exc:
            self.status_var.set(f"Set params failed: {exc}")

    def capture(self) -> None:
        try:
            frame = self.client.capture()
            self.last_frame = frame
            self._show_image(frame.image)
            self.status_var.set(f"Captured {frame.image.shape[1]}x{frame.image.shape[0]}")
            self._write_trace(
                {
                    "event": "capture",
                    "captured_at": frame.captured_at,
                    "params": frame.params.__dict__,
                    "shape": list(frame.image.shape),
                }
            )
        except Exception as exc:
            self.status_var.set(f"Capture failed: {exc}")

    def save_last(self) -> None:
        if self.last_frame is None:
            self.status_var.set("No frame to save")
            return
        path = self.client.save_frame(self.last_frame, self.output_dir)
        self.status_var.set(f"Saved {path}")

    def _show_image(self, image) -> None:
        max_w = max(1, self.preview_label.winfo_width())
        max_h = max(1, self.preview_label.winfo_height())
        rgb = image
        if len(image.shape) == 2:
            rgb = cv2.cvtColor(image, cv2.COLOR_GRAY2RGB)
        elif image.shape[2] == 4:
            rgb = cv2.cvtColor(image, cv2.COLOR_BGRA2RGB)
        else:
            rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        h, w = rgb.shape[:2]
        scale = min(max_w / w, max_h / h, 1.0)
        resized = cv2.resize(rgb, (int(w * scale), int(h * scale)), interpolation=cv2.INTER_AREA)
        pil_image = Image.fromarray(resized)
        self.preview = ImageTk.PhotoImage(pil_image)
        self.preview_label.configure(image=self.preview)

    def _write_trace(self, event: dict) -> None:
        self.output_dir.mkdir(parents=True, exist_ok=True)
        with self.trace_path.open("a", encoding="utf-8") as fp:
            fp.write(json.dumps(event, ensure_ascii=False) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="AT capture MVP GUI")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--output-dir", default="captures")
    args = parser.parse_args()

    app = ATMvpGui(args.host, args.port, Path(args.output_dir))
    app.mainloop()


if __name__ == "__main__":
    main()

