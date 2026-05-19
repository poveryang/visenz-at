#!/usr/bin/env python3
"""Minimal desktop tool for AT runner engineering tests."""

from __future__ import annotations

import argparse
import json
import tkinter as tk
from pathlib import Path
from tkinter import ttk

import cv2
from PIL import Image, ImageTk

from capture_client import CameraParams, CaptureServiceClient


class ATMvpGui(tk.Tk):
    PREVIEW_HEIGHT = 440
    PREVIEW_FALLBACK_WIDTH = 640
    PARAM_APPLY_DELAY_MS = 350
    EXPOSURE_RANGE_US = (1, 40000)
    GAIN_RANGE = (1, 128)
    FOCUS_RANGE = (0, 1023)

    def __init__(self, host: str, port: int, output_dir: Path) -> None:
        super().__init__()
        self.title("AT Runner MVP")
        self.geometry("1180x760")
        self.output_dir = output_dir
        self.trace_path = output_dir / "at_runner_trace.jsonl"
        self.client = CaptureServiceClient(host=host, port=port)
        self.last_frame = None
        self.preview_source_image = None
        self.preview = None
        self.at_running = False
        self.at_remaining_steps = 0
        self.param_apply_after_id = None
        self.suspend_param_apply = False

        self.host_var = tk.StringVar(value=host)
        self.port_var = tk.IntVar(value=port)
        self.exp_var = tk.IntVar(value=1000)
        self.gain_var = tk.IntVar(value=50)
        self.focus_var = tk.IntVar(value=30)
        self.light_vars = [tk.IntVar(value=1) for _ in range(4)]
        self.at_steps_var = tk.StringVar(value="80")
        self.at_interval_var = tk.StringVar(value="30")
        self.status_var = tk.StringVar(value="Disconnected")
        self.service_status_var = tk.StringVar(value="{}")
        self.at_trace_var = tk.StringVar(value="{}")

        self._build_layout()
        self._attach_param_traces()

    def _build_layout(self) -> None:
        root = ttk.Frame(self, padding=10)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=3, uniform="main_vertical")
        root.rowconfigure(1, weight=1, uniform="main_vertical")

        top = ttk.Frame(root)
        top.grid(row=0, column=0, sticky=tk.NSEW)
        top.columnconfigure(0, weight=3, uniform="top_horizontal")
        top.columnconfigure(1, weight=4, uniform="top_horizontal")
        top.rowconfigure(0, weight=1)

        left = ttk.Frame(top)
        left.grid(row=0, column=0, sticky=tk.NSEW, padx=(0, 10))
        right = ttk.Frame(top)
        right.grid(row=0, column=1, sticky=tk.NSEW)
        right.columnconfigure(0, weight=1)
        right.rowconfigure(0, weight=1)

        controls = ttk.LabelFrame(left, text="Runner Service")
        controls.pack(fill=tk.X)
        controls.columnconfigure(1, weight=1)

        ttk.Label(controls, text="Host").grid(row=0, column=0, sticky=tk.W, padx=4, pady=4)
        ttk.Entry(controls, textvariable=self.host_var, width=18).grid(row=0, column=1, sticky=tk.EW, padx=4, pady=4)
        ttk.Label(controls, text="Port").grid(row=0, column=2, sticky=tk.W, padx=(8, 4), pady=4)
        ttk.Entry(controls, textvariable=self.port_var, width=7).grid(row=0, column=3, padx=4, pady=4)
        ttk.Button(controls, text="Connect", command=self.connect).grid(row=0, column=4, padx=(8, 4), pady=4)
        ttk.Button(controls, text="Status", command=self.refresh_status).grid(row=0, column=5, padx=4, pady=4)

        params = ttk.LabelFrame(left, text="Camera Params")
        params.pack(fill=tk.X, pady=8)
        params.columnconfigure(1, weight=1)
        ttk.Label(params, text="Exposure").grid(row=0, column=0, sticky=tk.W, padx=4, pady=4)
        tk.Scale(
            params,
            variable=self.exp_var,
            from_=self.EXPOSURE_RANGE_US[0],
            to=self.EXPOSURE_RANGE_US[1],
            orient=tk.HORIZONTAL,
            resolution=1,
            showvalue=True,
        ).grid(row=0, column=1, sticky=tk.EW, padx=4, pady=4)
        ttk.Label(params, text="Gain").grid(row=1, column=0, sticky=tk.W, padx=4, pady=4)
        tk.Scale(
            params,
            variable=self.gain_var,
            from_=self.GAIN_RANGE[0],
            to=self.GAIN_RANGE[1],
            orient=tk.HORIZONTAL,
            resolution=1,
            showvalue=True,
        ).grid(row=1, column=1, sticky=tk.EW, padx=4, pady=4)
        ttk.Label(params, text="Focus").grid(row=2, column=0, sticky=tk.W, padx=4, pady=4)
        tk.Scale(
            params,
            variable=self.focus_var,
            from_=self.FOCUS_RANGE[0],
            to=self.FOCUS_RANGE[1],
            orient=tk.HORIZONTAL,
            resolution=1,
            showvalue=True,
        ).grid(row=2, column=1, sticky=tk.EW, padx=4, pady=4)
        ttk.Label(params, text="Lights").grid(row=3, column=0, sticky=tk.W, padx=4, pady=4)
        lights_frame = ttk.Frame(params)
        lights_frame.grid(row=3, column=1, sticky=tk.W, padx=4, pady=4)
        for index, var in enumerate(self.light_vars):
            ttk.Checkbutton(
                lights_frame,
                text=str(index + 1),
                variable=var,
                command=self._apply_params_now,
            ).grid(row=0, column=index, padx=(0, 10), pady=0)

        ttk.Button(params, text="Capture", command=self.capture).grid(row=4, column=0, padx=4, pady=(8, 4))
        ttk.Button(params, text="Save Last", command=self.save_last).grid(row=4, column=1, sticky=tk.W, padx=4, pady=(8, 4))

        at_controls = ttk.LabelFrame(left, text="AT Execution")
        at_controls.pack(fill=tk.X, pady=8)
        ttk.Label(at_controls, text="Max Steps").grid(row=0, column=0, sticky=tk.W, padx=4, pady=4)
        ttk.Entry(at_controls, textvariable=self.at_steps_var, width=8).grid(row=0, column=1, padx=4, pady=4)
        ttk.Label(at_controls, text="Interval ms").grid(row=0, column=2, sticky=tk.W, padx=(12, 4), pady=4)
        ttk.Entry(at_controls, textvariable=self.at_interval_var, width=8).grid(row=0, column=3, padx=4, pady=4)
        ttk.Button(at_controls, text="Reset AT", command=self.reset_at).grid(row=1, column=0, padx=4, pady=(8, 4))
        ttk.Button(at_controls, text="AT Step", command=self.at_step).grid(row=1, column=1, padx=4, pady=(8, 4))
        ttk.Button(at_controls, text="Run AT", command=self.run_at).grid(row=1, column=2, padx=4, pady=(8, 4))
        ttk.Button(at_controls, text="Stop AT", command=self.stop_at).grid(row=1, column=3, padx=4, pady=(8, 4))

        preview_frame = ttk.LabelFrame(right, text="Preview")
        preview_frame.grid(row=0, column=0, sticky=tk.NSEW)
        self.preview_canvas = tk.Canvas(preview_frame, height=self.PREVIEW_HEIGHT, bg="#111111", highlightthickness=0)
        self.preview_canvas.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        self.preview_canvas.bind("<Configure>", self._on_preview_resize)

        bottom = ttk.Frame(root)
        bottom.grid(row=1, column=0, sticky=tk.NSEW, pady=(8, 0))
        bottom.columnconfigure(0, weight=1)
        bottom.rowconfigure(0, weight=1)
        bottom.rowconfigure(1, weight=1)

        status_frame = ttk.LabelFrame(bottom, text="Status")
        status_frame.grid(row=0, column=0, sticky=tk.NSEW)
        ttk.Label(status_frame, textvariable=self.status_var, wraplength=1120, justify=tk.LEFT).pack(
            fill=tk.X, padx=4, pady=(4, 0)
        )
        ttk.Label(status_frame, textvariable=self.service_status_var, wraplength=1120, justify=tk.LEFT).pack(
            fill=tk.X, padx=4, pady=4
        )

        trace_frame = ttk.LabelFrame(bottom, text="Trace")
        trace_frame.grid(row=1, column=0, sticky=tk.NSEW, pady=(8, 0))
        ttk.Label(trace_frame, textvariable=self.at_trace_var, wraplength=1120, justify=tk.LEFT).pack(
            fill=tk.X, padx=4, pady=4
        )

    def connect(self) -> None:
        self.client.close()
        self.client = CaptureServiceClient(host=self.host_var.get(), port=int(self.port_var.get()))
        try:
            self.client.connect()
            self._update_params_from_status(self.client.last_status)
            self.status_var.set("Connected")
            self._update_service_status(self.client.last_status)
        except Exception as exc:
            self.status_var.set(f"Connect failed: {exc}")

    def refresh_status(self) -> None:
        try:
            status = self.client.get_status()
            self._update_params_from_status(status)
            self._update_service_status(status)
            self.status_var.set("Status refreshed")
        except Exception as exc:
            self.status_var.set(f"Status failed: {exc}")

    def current_params(self) -> CameraParams:
        return CameraParams(
            exposure_us=int(self.exp_var.get()),
            gain=int(self.gain_var.get()),
            focus=int(self.focus_var.get()),
            lights=tuple(int(var.get()) for var in self.light_vars),  # type: ignore[arg-type]
        )

    def set_params(self) -> None:
        try:
            params = self.current_params()
            status = self.client.set_params(params)
            self.status_var.set("Params sent")
            self._update_service_status(status)
            self._write_trace({"event": "set_params", "params": self._params_json(params), "status": status})
        except Exception as exc:
            self.status_var.set(f"Set params failed: {exc}")

    def _attach_param_traces(self) -> None:
        for var in (self.exp_var, self.gain_var, self.focus_var, *self.light_vars):
            var.trace_add("write", self._schedule_param_apply)

    def _schedule_param_apply(self, *_args) -> None:
        if self.suspend_param_apply or not self.client.connected:
            return
        if self.param_apply_after_id is not None:
            self.after_cancel(self.param_apply_after_id)
        self.param_apply_after_id = self.after(self.PARAM_APPLY_DELAY_MS, self._apply_params_from_ui)

    def _apply_params_now(self, _event=None) -> None:
        if self.param_apply_after_id is not None:
            self.after_cancel(self.param_apply_after_id)
            self.param_apply_after_id = None
        self._apply_params_from_ui()

    def _apply_params_from_ui(self) -> None:
        self.param_apply_after_id = None
        if not self.client.connected:
            return
        try:
            params = self.current_params()
        except ValueError:
            self.status_var.set("Camera params are incomplete")
            return
        try:
            status = self.client.set_params(params)
            self.status_var.set("Params applied")
            self._update_service_status(status)
            self._write_trace({"event": "auto_set_params", "params": self._params_json(params), "status": status})
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
                    "params": self._params_json(frame.params),
                    "shape": list(frame.image.shape),
                    "encoding": frame.encoding,
                    "status": frame.status,
                }
            )
        except Exception as exc:
            self.status_var.set(f"Capture failed: {exc}")

    def reset_at(self) -> None:
        try:
            params = self.current_params()
            at_state = self.client.reset_at(params)
            self._update_service_status(self.client.last_status)
            self.status_var.set("AT reset")
            self._write_trace({"event": "reset_at", "params": self._params_json(params), "at": at_state})
            self.at_trace_var.set(json.dumps(at_state, ensure_ascii=False, sort_keys=True))
        except Exception as exc:
            self.status_var.set(f"AT reset failed: {exc}")

    def at_step(self) -> bool:
        try:
            frame = self.client.at_step()
            self.last_frame = frame
            self._show_image(frame.image)
            self._update_params_from_status(frame.status)
            self._update_service_status(frame.status)
            if frame.trace is not None:
                self.at_trace_var.set(json.dumps(frame.trace, ensure_ascii=False, sort_keys=True))
            self.status_var.set(self._at_status_text(frame))
            self._write_trace(
                {
                    "event": "at_step",
                    "captured_at": frame.captured_at,
                    "params": self._params_json(frame.params),
                    "shape": list(frame.image.shape),
                    "encoding": frame.encoding,
                    "status": frame.status,
                    "at": frame.at,
                    "trace": frame.trace,
                }
            )
            return bool(frame.at and frame.at.get("finished"))
        except Exception as exc:
            self.status_var.set(f"AT step failed: {exc}")
            self.at_running = False
            return True

    def run_at(self) -> None:
        if self.at_running:
            return
        try:
            params = self.current_params()
            at_state = self.client.reset_at(params)
            self.at_remaining_steps = max(1, int(self.at_steps_var.get()))
            self.at_running = True
            self._write_trace({"event": "run_at_start", "params": self._params_json(params), "at": at_state})
            self.status_var.set("AT running")
            self.after(1, self._run_at_next)
        except Exception as exc:
            self.at_running = False
            self.status_var.set(f"Run AT failed: {exc}")

    def stop_at(self) -> None:
        self.at_running = False
        self.status_var.set("AT stopped")
        self._write_trace({"event": "run_at_stop"})

    def _run_at_next(self) -> None:
        if not self.at_running:
            return
        if self.at_remaining_steps <= 0:
            self.at_running = False
            self.status_var.set("AT stopped: max steps reached")
            self._write_trace({"event": "run_at_done", "reason": "max_steps"})
            return

        finished = self.at_step()
        self.at_remaining_steps -= 1
        if finished:
            self.at_running = False
            self._write_trace({"event": "run_at_done", "reason": "at_finished"})
            return
        try:
            interval_ms = max(1, int(self.at_interval_var.get()))
        except ValueError:
            interval_ms = 30
        self.after(interval_ms, self._run_at_next)

    def save_last(self) -> None:
        if self.last_frame is None:
            self.status_var.set("No frame to save")
            return
        path = self.client.save_frame(self.last_frame, self.output_dir)
        self.status_var.set(f"Saved {path}")
        self._write_trace(
            {
                "event": "save_frame",
                "image_path": str(path),
                "captured_at": self.last_frame.captured_at,
                "params": self._params_json(self.last_frame.params),
            }
        )

    def _show_image(self, image) -> None:
        self.preview_source_image = image
        self._render_preview_image()

    def _on_preview_resize(self, _event) -> None:
        if self.preview_source_image is not None:
            self._render_preview_image()

    def _render_preview_image(self) -> None:
        image = self.preview_source_image
        if image is None:
            return

        max_w = self.preview_canvas.winfo_width()
        if max_w <= 1:
            max_w = self.PREVIEW_FALLBACK_WIDTH
        max_h = self.preview_canvas.winfo_height()
        if max_h <= 1:
            max_h = self.PREVIEW_HEIGHT

        rgb = image
        if len(image.shape) == 2:
            rgb = cv2.cvtColor(image, cv2.COLOR_GRAY2RGB)
        elif image.shape[2] == 4:
            rgb = cv2.cvtColor(image, cv2.COLOR_BGRA2RGB)
        else:
            rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        h, w = rgb.shape[:2]
        scale = min(max_w / w, max_h / h, 1.0)
        resized_w = max(1, int(w * scale))
        resized_h = max(1, int(h * scale))
        resized = cv2.resize(rgb, (resized_w, resized_h), interpolation=cv2.INTER_AREA)
        pil_image = Image.fromarray(resized)
        self.preview = ImageTk.PhotoImage(pil_image)
        self.preview_canvas.delete("all")
        self.preview_canvas.create_image(max_w // 2, max_h // 2, image=self.preview, anchor=tk.CENTER)

    def _write_trace(self, event: dict) -> None:
        self.output_dir.mkdir(parents=True, exist_ok=True)
        with self.trace_path.open("a", encoding="utf-8") as fp:
            fp.write(json.dumps(event, ensure_ascii=False) + "\n")

    def _update_service_status(self, status: dict) -> None:
        self.service_status_var.set(json.dumps(status, ensure_ascii=False, sort_keys=True))

    def _update_params_from_status(self, status: dict) -> None:
        params = status.get("params", {})
        if not isinstance(params, dict):
            return
        self.suspend_param_apply = True
        try:
            self.exp_var.set(self._clamp_int(
                params.get("exposure_us", self.exp_var.get()),
                self.EXPOSURE_RANGE_US,
            ))
            self.gain_var.set(self._clamp_int(params.get("gain", self.gain_var.get()), self.GAIN_RANGE))
            self.focus_var.set(self._clamp_int(params.get("focus", self.focus_var.get()), self.FOCUS_RANGE))
            lights = params.get("lights")
            if isinstance(lights, list) and len(lights) == 4:
                for var, value in zip(self.light_vars, lights):
                    var.set(1 if int(value) else 0)
        finally:
            self.suspend_param_apply = False

    @staticmethod
    def _clamp_int(value, value_range: tuple[int, int]) -> int:
        lower, upper = value_range
        return max(lower, min(upper, int(value)))

    @staticmethod
    def _at_status_text(frame) -> str:
        if frame.trace:
            step = frame.trace.get("step")
            action = frame.trace.get("action")
            difficulty = frame.trace.get("difficulty")
            finished = frame.trace.get("finished")
            return f"AT step={step} difficulty={difficulty} action={action} finished={finished}"
        return "AT step completed"

    @staticmethod
    def _params_json(params: CameraParams) -> dict:
        return {
            "exposure_us": params.exposure_us,
            "gain": params.gain,
            "focus": params.focus,
            "lights": list(params.lights),
        }


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
