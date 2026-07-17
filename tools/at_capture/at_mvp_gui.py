#!/usr/bin/env python3
"""Minimal desktop tool for AT runner engineering tests."""

from __future__ import annotations

import argparse
import json
import threading
import tkinter as tk
from pathlib import Path
from tkinter import ttk

import cv2
from PIL import Image, ImageTk

from capture_client import CameraParams, CaptureFrame, CaptureServiceClient


class ATMvpGui(tk.Tk):
    PREVIEW_FALLBACK = (720, 450)
    PARAM_APPLY_DELAY_MS = 350
    EXPOSURE_RANGE_US = (1, 40000)
    GAIN_RANGE = (1, 128)
    FOCUS_RANGE = (0, 420)
    ASYNC_POLL_MS = 300
    LIVE_PREVIEW_MS = 800
    DEFAULT_START_PARAMS = CameraParams(exposure_us=1000, gain=10, focus=0, lights=(1, 1, 1, 1))

    def __init__(self, host: str, port: int, output_dir: Path) -> None:
        super().__init__()
        self.title("AT Runner MVP")
        self.geometry("1280x820")
        self.minsize(960, 640)
        self.output_dir = output_dir
        self.trace_path = output_dir / "at_runner_trace.jsonl"
        self.client = CaptureServiceClient(host=host, port=port)
        self.last_frame = None
        self.preview_source_image = None
        self.preview = None
        self.at_running = False
        self.last_preview_version = 0
        self.param_apply_after_id = None
        self.suspend_param_apply = False
        self.live_preview = False
        self.live_preview_after_id = None
        self.busy = False

        self.host_var = tk.StringVar(value=host)
        self.port_var = tk.IntVar(value=port)
        start = self.DEFAULT_START_PARAMS
        self.exp_var = tk.IntVar(value=start.exposure_us)
        self.gain_var = tk.IntVar(value=start.gain)
        self.focus_var = tk.IntVar(value=start.focus)
        self.light_vars = [tk.IntVar(value=int(v)) for v in start.lights]
        self.at_steps_var = tk.StringVar(value="80")
        self.preview_every_var = tk.StringVar(value="5")
        self.live_var = tk.BooleanVar(value=False)
        self.jpeg_preview_var = tk.BooleanVar(value=True)
        self.jpeg_save_var = tk.BooleanVar(value=True)
        self.status_var = tk.StringVar(value="Disconnected")

        self._build_layout()
        self._attach_param_traces()
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.after(200, self.connect)

    def _build_layout(self) -> None:
        root = ttk.Frame(self, padding=8)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(1, weight=1)
        root.rowconfigure(1, weight=1)

        # Connection bar
        bar = ttk.Frame(root)
        bar.grid(row=0, column=0, columnspan=2, sticky=tk.EW, pady=(0, 8))
        bar.columnconfigure(6, weight=1)
        ttk.Label(bar, text="Host").grid(row=0, column=0, padx=(0, 4))
        ttk.Entry(bar, textvariable=self.host_var, width=16).grid(row=0, column=1, padx=(0, 8))
        ttk.Label(bar, text="Port").grid(row=0, column=2, padx=(0, 4))
        ttk.Entry(bar, textvariable=self.port_var, width=6).grid(row=0, column=3, padx=(0, 8))
        ttk.Button(bar, text="Connect", command=self.connect).grid(row=0, column=4, padx=(0, 4))
        ttk.Button(bar, text="Refresh", command=self.refresh_status).grid(row=0, column=5, padx=(0, 12))
        ttk.Label(bar, textvariable=self.status_var).grid(row=0, column=6, sticky=tk.W)

        # Left controls
        left = ttk.Frame(root)
        left.grid(row=1, column=0, sticky=tk.NS, padx=(0, 10))
        left.columnconfigure(0, weight=1)

        cam = ttk.LabelFrame(left, text="Camera", padding=6)
        cam.pack(fill=tk.X)
        cam.columnconfigure(1, weight=1)
        self._add_scale(cam, 0, "Exposure", self.exp_var, self.EXPOSURE_RANGE_US)
        self._add_scale(cam, 1, "Gain", self.gain_var, self.GAIN_RANGE)
        self._add_scale(cam, 2, "Focus", self.focus_var, self.FOCUS_RANGE)

        ttk.Label(cam, text="Lights").grid(row=3, column=0, sticky=tk.W, padx=2, pady=4)
        lights = ttk.Frame(cam)
        lights.grid(row=3, column=1, sticky=tk.W, padx=2, pady=4)
        for index, var in enumerate(self.light_vars):
            ttk.Checkbutton(lights, text=str(index + 1), variable=var, command=self._apply_params_now).pack(
                side=tk.LEFT, padx=(0, 8)
            )

        cam_btns = ttk.Frame(cam)
        cam_btns.grid(row=4, column=0, columnspan=2, sticky=tk.EW, pady=(8, 0))
        ttk.Button(cam_btns, text="Snap", command=self.capture).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(cam_btns, text="Heatmap", command=self.capture_heatmap).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(cam_btns, text="Save", command=self.save_last).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Checkbutton(
            cam_btns,
            text="Live",
            variable=self.live_var,
            command=self._toggle_live_preview,
        ).pack(side=tk.LEFT, padx=(8, 0))

        enc_btns = ttk.Frame(cam)
        enc_btns.grid(row=5, column=0, columnspan=2, sticky=tk.W, pady=(6, 0))
        ttk.Checkbutton(enc_btns, text="JPEG preview", variable=self.jpeg_preview_var).pack(
            side=tk.LEFT, padx=(0, 12)
        )
        ttk.Checkbutton(enc_btns, text="JPEG save", variable=self.jpeg_save_var).pack(side=tk.LEFT)

        at = ttk.LabelFrame(left, text="AT", padding=6)
        at.pack(fill=tk.X, pady=(10, 0))
        at.columnconfigure(1, weight=1)
        ttk.Label(at, text="Max steps").grid(row=0, column=0, sticky=tk.W, padx=2, pady=2)
        ttk.Entry(at, textvariable=self.at_steps_var, width=8).grid(row=0, column=1, sticky=tk.W, padx=2, pady=2)
        ttk.Label(at, text="Preview every").grid(row=1, column=0, sticky=tk.W, padx=2, pady=2)
        ttk.Entry(at, textvariable=self.preview_every_var, width=8).grid(row=1, column=1, sticky=tk.W, padx=2, pady=2)
        ttk.Label(at, text="(0=off)").grid(row=1, column=2, sticky=tk.W)

        at_btns = ttk.Frame(at)
        at_btns.grid(row=2, column=0, columnspan=3, sticky=tk.EW, pady=(8, 0))
        ttk.Button(at_btns, text="Reset", command=self.reset_at).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(at_btns, text="Step", command=self.at_step).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(at_btns, text="Run", command=self.run_at).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(at_btns, text="Stop", command=self.stop_at).pack(side=tk.LEFT)

        info = ttk.LabelFrame(left, text="Trace", padding=4)
        info.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        self.trace_text = tk.Text(info, height=14, width=36, wrap=tk.WORD, font=("Consolas", 9))
        self.trace_text.pack(fill=tk.BOTH, expand=True)
        self.trace_text.insert("1.0", "Connect to load preview and status.")
        self.trace_text.configure(state=tk.DISABLED)

        # Preview
        preview = ttk.LabelFrame(root, text="Preview", padding=4)
        preview.grid(row=1, column=1, sticky=tk.NSEW)
        preview.columnconfigure(0, weight=1)
        preview.rowconfigure(0, weight=1)
        self.preview_canvas = tk.Canvas(preview, bg="#111111", highlightthickness=0)
        self.preview_canvas.grid(row=0, column=0, sticky=tk.NSEW)
        self.preview_canvas.bind("<Configure>", self._on_preview_resize)
        self.preview_canvas.create_text(
            20,
            20,
            anchor=tk.NW,
            fill="#888888",
            font=("Segoe UI", 11),
            text="No image yet — Connect will Snap automatically",
        )

    @staticmethod
    def _add_scale(parent, row: int, label: str, var: tk.IntVar, value_range: tuple[int, int]) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky=tk.W, padx=2, pady=2)
        tk.Scale(
            parent,
            variable=var,
            from_=value_range[0],
            to=value_range[1],
            orient=tk.HORIZONTAL,
            resolution=1,
            showvalue=True,
            length=220,
        ).grid(row=row, column=1, sticky=tk.EW, padx=2, pady=2)

    def _preview_encoding(self) -> str:
        return "jpeg" if self.jpeg_preview_var.get() else "png"

    def _save_encoding(self) -> str:
        return "jpeg" if self.jpeg_save_var.get() else "png"

    @staticmethod
    def _normalize_encoding(encoding: str | None) -> str:
        if encoding in ("jpg", "jpeg"):
            return "jpeg"
        return encoding or "jpeg"

    def _frame_for_save(self, frame: CaptureFrame) -> CaptureFrame:
        want = self._save_encoding()
        have = self._normalize_encoding(frame.encoding)
        if have == want:
            return frame
        if want == "jpeg":
            ok, buf = cv2.imencode(".jpg", frame.image, [int(cv2.IMWRITE_JPEG_QUALITY), 80])
        else:
            ok, buf = cv2.imencode(".png", frame.image)
        if not ok:
            raise RuntimeError(f"failed to re-encode as {want}")
        return CaptureFrame(
            image=frame.image,
            image_bytes=buf.tobytes(),
            encoding=want,
            params=frame.params,
            captured_at=frame.captured_at,
            status=frame.status,
            metadata=frame.metadata,
            trace=frame.trace,
            at=frame.at,
        )

    def connect(self) -> None:
        self._stop_live_preview()
        self.client.close()
        self.client = CaptureServiceClient(host=self.host_var.get(), port=int(self.port_var.get()))
        self.status_var.set("Connecting…")
        self._background(self._connect_and_snap, self._on_connect_done)

    def _connect_and_snap(self):
        self.client.connect()
        status = self.client.last_status
        frame = self.client.capture(encoding=self._preview_encoding())
        return status, frame

    def _on_connect_done(self, result) -> None:
        status, frame = result
        self._update_params_from_status(status)
        self.last_frame = frame
        self._show_image(frame.image)
        self.status_var.set(
            f"Connected · {status.get('device')} {status.get('width')}x{status.get('height')} · "
            f"preview {frame.image.shape[1]}x{frame.image.shape[0]} ({frame.encoding})"
        )
        self._set_trace({"connected": True, "status": self._compact_status(status)})
        self._write_trace(
            {
                "event": "connect_capture",
                "params": self._params_json(frame.params),
                "shape": list(frame.image.shape),
                "encoding": frame.encoding,
                "status": status,
            }
        )

    def refresh_status(self) -> None:
        self._background(self.client.get_status, self._on_status)

    def _on_status(self, status: dict) -> None:
        self._update_params_from_status(status)
        self.status_var.set(
            f"Status · opened={status.get('opened')} "
            f"exp={status.get('params', {}).get('exposure_us')} "
            f"gain={status.get('params', {}).get('gain')} "
            f"focus={status.get('params', {}).get('focus')}"
        )
        self._set_trace({"status": self._compact_status(status)})

    def current_params(self) -> CameraParams:
        return CameraParams(
            exposure_us=int(self.exp_var.get()),
            gain=int(self.gain_var.get()),
            focus=int(self.focus_var.get()),
            lights=tuple(int(var.get()) for var in self.light_vars),  # type: ignore[arg-type]
        )

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
        if not self.client.connected or self.at_running:
            return
        try:
            params = self.current_params()
            status = self.client.set_params(params)
            self.status_var.set(
                f"Params · e={params.exposure_us} g={params.gain} f={params.focus} l={list(params.lights)}"
            )
            self._write_trace({"event": "auto_set_params", "params": self._params_json(params), "status": status})
        except Exception as exc:
            self.status_var.set(f"Set params failed: {exc}")

    def capture(self) -> None:
        if self.busy:
            return
        encoding = self._preview_encoding()
        self._background(lambda: self.client.capture(encoding=encoding), self._on_capture)

    def _on_capture(self, frame) -> None:
        self.last_frame = frame
        self._show_image(frame.image)
        self.status_var.set(
            f"Snap {frame.image.shape[1]}x{frame.image.shape[0]} ({frame.encoding})"
        )
        self._write_trace(
            {
                "event": "capture",
                "captured_at": frame.captured_at,
                "params": self._params_json(frame.params),
                "shape": list(frame.image.shape),
                "encoding": frame.encoding,
            }
        )

    def capture_heatmap(self) -> None:
        if self.busy:
            return
        encoding = self._preview_encoding()
        self._background(
            lambda: self.client.capture_heatmap(overlay=True, encoding=encoding),
            self._on_heatmap,
        )

    def _on_heatmap(self, frame) -> None:
        self.last_frame = frame
        self._show_image(frame.image)
        self.status_var.set(
            f"Heatmap {frame.image.shape[1]}x{frame.image.shape[0]} ({frame.encoding})"
        )
        self._set_trace(frame.trace or {"heatmap": True})
        self._write_trace(
            {
                "event": "capture_heatmap",
                "captured_at": frame.captured_at,
                "params": self._params_json(frame.params),
                "shape": list(frame.image.shape),
                "encoding": frame.encoding,
                "trace": frame.trace,
            }
        )

    def reset_at(self) -> None:
        """Reset AT state and restore start params: e=1000 g=10 f=0 lights=1111."""
        params = self.DEFAULT_START_PARAMS
        self.suspend_param_apply = True
        try:
            self.exp_var.set(params.exposure_us)
            self.gain_var.set(params.gain)
            self.focus_var.set(params.focus)
            for var, value in zip(self.light_vars, params.lights):
                var.set(1 if int(value) else 0)
        finally:
            self.suspend_param_apply = False
        try:
            at_state = self.client.reset_at(params)
            self.status_var.set(
                f"AT reset · e={params.exposure_us} g={params.gain} f={params.focus} l={list(params.lights)}"
            )
            self._set_trace(at_state)
            self._write_trace({"event": "reset_at", "params": self._params_json(params), "at": at_state})
        except Exception as exc:
            self.status_var.set(f"AT reset failed: {exc}")

    def at_step(self) -> bool:
        if self.busy:
            return True
        try:
            frame = self.client.at_step(encoding=self._preview_encoding())
            self.last_frame = frame
            self._show_image(frame.image)
            self._update_params_from_status(frame.status)
            if frame.trace is not None:
                self._set_trace(frame.trace)
                self._sync_sliders_from_trace(frame.trace)
            self.status_var.set(self._at_status_text(frame))
            self._write_trace(
                {
                    "event": "at_step",
                    "captured_at": frame.captured_at,
                    "params": self._params_json(frame.params),
                    "encoding": frame.encoding,
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
        if self.at_running or self.busy:
            return
        self._stop_live_preview()
        try:
            params = self.current_params()
            max_steps = max(1, int(self.at_steps_var.get()))
            preview_every = max(0, int(self.preview_every_var.get()))
            self.status_var.set("Starting device AT…")
            self._background(
                lambda: self.client.run_at_async(params, max_steps, preview_every),
                lambda state: self._async_run_started(params, state),
            )
        except Exception as exc:
            self.at_running = False
            self.status_var.set(f"Run AT failed: {exc}")

    def stop_at(self) -> None:
        if not self.at_running:
            return
        self.status_var.set("Stopping device AT…")
        self._background(self.client.stop_at_async, self._async_run_update)

    def _async_run_started(self, params: CameraParams, state: dict) -> None:
        self.at_running = True
        self.last_preview_version = 0
        self._write_trace({"event": "run_at_async_start", "params": self._params_json(params), "at": state})
        self._async_run_update(state)
        self.after(self.ASYNC_POLL_MS, self._poll_async_run)

    def _poll_async_run(self) -> None:
        if not self.at_running:
            return

        def poll() -> tuple[dict, object | None]:
            state = self.client.get_run_status()
            version = int(state.get("preview_version", 0))
            frame = (
                self.client.get_preview(encoding=self._preview_encoding())
                if version > self.last_preview_version
                else None
            )
            return state, frame

        self._background(poll, self._async_poll_complete)

    def _async_poll_complete(self, result: tuple[dict, object | None]) -> None:
        state, frame = result
        preview_version = int(state.get("preview_version", 0))
        if frame is not None:
            self.last_frame = frame
            self._show_image(frame.image)
            self.last_preview_version = preview_version
            step = state.get("last_trace", {}).get("step", preview_version)
            try:
                saved = self._frame_for_save(frame)
                image_path = self.client.save_frame(
                    saved, self.output_dir, note="async_at_preview", filename_stem=f"at_preview_{step:03d}"
                )
            except Exception as exc:
                self.status_var.set(f"Preview save failed: {exc}")
                image_path = None
            self._write_trace(
                {
                    "event": "async_preview_saved",
                    "step": step,
                    "preview_version": preview_version,
                    "encoding": getattr(frame, "encoding", None),
                    "image_path": str(image_path) if image_path else None,
                }
            )
        self._async_run_update(state)
        if self.at_running:
            self.after(self.ASYNC_POLL_MS, self._poll_async_run)

    def _async_run_update(self, state: dict) -> None:
        last_trace = state.get("last_trace")
        if isinstance(last_trace, dict):
            self._set_trace(last_trace)
            self._sync_sliders_from_trace(last_trace)
            self._write_trace({"event": "run_at_async_step", "trace": last_trace})
        running = bool(state.get("running"))
        self.at_running = running
        self.status_var.set(
            f"Device AT: {state.get('completed_steps', 0)}/{state.get('max_steps', 0)} "
            f"— {state.get('finish_reason', 'Running')}"
        )
        if not running:
            self._write_trace({"event": "run_at_async_done", "at": state})

    def _sync_sliders_from_trace(self, trace: dict) -> None:
        """Keep GUI sliders aligned with AT params without pushing set_params."""
        params = trace.get("next_params")
        if not isinstance(params, dict):
            params = trace.get("current_params")
        if not isinstance(params, dict):
            return
        mapped = {
            "exposure_us": params.get("exposure_us", params.get("exp_time")),
            "gain": params.get("gain", params.get("exp_gain")),
            "focus": params.get("focus", params.get("focus_pos")),
            "lights": params.get("lights"),
        }
        self._update_params_from_dict(mapped)

    def _toggle_live_preview(self) -> None:
        if self.live_var.get():
            if not self.client.connected:
                self.live_var.set(False)
                self.status_var.set("Connect first for live preview")
                return
            if self.at_running:
                self.live_var.set(False)
                self.status_var.set("Stop AT run before live preview")
                return
            self.live_preview = True
            self.status_var.set("Live preview on")
            self._schedule_live_preview()
        else:
            self._stop_live_preview()
            self.status_var.set("Live preview off")

    def _schedule_live_preview(self) -> None:
        if self.live_preview_after_id is not None:
            self.after_cancel(self.live_preview_after_id)
        self.live_preview_after_id = self.after(self.LIVE_PREVIEW_MS, self._live_preview_tick)

    def _live_preview_tick(self) -> None:
        self.live_preview_after_id = None
        if not self.live_preview or not self.client.connected or self.at_running or self.busy:
            if self.live_preview:
                self._schedule_live_preview()
            return
        self._background(
            lambda: self.client.capture(encoding=self._preview_encoding()),
            self._on_live_frame,
        )

    def _on_live_frame(self, frame) -> None:
        self.last_frame = frame
        self._show_image(frame.image)
        if self.live_preview:
            self._schedule_live_preview()

    def _stop_live_preview(self) -> None:
        self.live_preview = False
        self.live_var.set(False)
        if self.live_preview_after_id is not None:
            self.after_cancel(self.live_preview_after_id)
            self.live_preview_after_id = None

    def _background(self, action, on_success) -> None:
        if self.busy:
            return
        self.busy = True

        def worker() -> None:
            try:
                value = action()
            except Exception as exc:
                self.after(0, lambda err=exc: self._on_bg_error(err))
                return
            self.after(0, lambda result=value: self._on_bg_success(on_success, result))

        threading.Thread(target=worker, daemon=True).start()

    def _on_bg_error(self, exc: Exception) -> None:
        self.busy = False
        self.status_var.set(f"Request failed: {exc}")

    def _on_bg_success(self, on_success, value) -> None:
        self.busy = False
        on_success(value)

    def save_last(self) -> None:
        if self.last_frame is None:
            self.status_var.set("No frame to save")
            return
        try:
            frame = self._frame_for_save(self.last_frame)
            path = self.client.save_frame(frame, self.output_dir)
            self.status_var.set(f"Saved {path} ({frame.encoding})")
            self._write_trace(
                {
                    "event": "save_frame",
                    "image_path": str(path),
                    "captured_at": self.last_frame.captured_at,
                    "params": self._params_json(self.last_frame.params),
                    "encoding": frame.encoding,
                }
            )
        except Exception as exc:
            self.status_var.set(f"Save failed: {exc}")

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
        max_h = self.preview_canvas.winfo_height()
        if max_w <= 1:
            max_w = self.PREVIEW_FALLBACK[0]
        if max_h <= 1:
            max_h = self.PREVIEW_FALLBACK[1]

        if len(image.shape) == 2:
            rgb = cv2.cvtColor(image, cv2.COLOR_GRAY2RGB)
        elif image.shape[2] == 4:
            rgb = cv2.cvtColor(image, cv2.COLOR_BGRA2RGB)
        else:
            rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        h, w = rgb.shape[:2]
        scale = min(max_w / w, max_h / h)
        resized = cv2.resize(rgb, (max(1, int(w * scale)), max(1, int(h * scale))), interpolation=cv2.INTER_AREA)
        self.preview = ImageTk.PhotoImage(Image.fromarray(resized))
        self.preview_canvas.delete("all")
        self.preview_canvas.create_image(max_w // 2, max_h // 2, image=self.preview, anchor=tk.CENTER)

    def _set_trace(self, payload) -> None:
        text = json.dumps(self._compact_trace(payload), ensure_ascii=False, indent=2, sort_keys=True)
        self.trace_text.configure(state=tk.NORMAL)
        self.trace_text.delete("1.0", tk.END)
        self.trace_text.insert("1.0", text)
        self.trace_text.configure(state=tk.DISABLED)

    def _write_trace(self, event: dict) -> None:
        self.output_dir.mkdir(parents=True, exist_ok=True)
        with self.trace_path.open("a", encoding="utf-8") as fp:
            fp.write(json.dumps(event, ensure_ascii=False) + "\n")

    def _update_params_from_status(self, status: dict) -> None:
        params = status.get("params", {})
        if isinstance(params, dict):
            self._update_params_from_dict(params)

    def _update_params_from_dict(self, params: dict) -> None:
        self.suspend_param_apply = True
        try:
            if params.get("exposure_us") is not None:
                self.exp_var.set(self._clamp_int(params.get("exposure_us"), self.EXPOSURE_RANGE_US))
            if params.get("gain") is not None:
                self.gain_var.set(self._clamp_int(params.get("gain"), self.GAIN_RANGE))
            if params.get("focus") is not None:
                self.focus_var.set(self._clamp_int(params.get("focus"), self.FOCUS_RANGE))
            lights = params.get("lights")
            if isinstance(lights, list) and len(lights) == 4:
                for var, value in zip(self.light_vars, lights):
                    var.set(1 if int(value) else 0)
        finally:
            self.suspend_param_apply = False

    def _on_close(self) -> None:
        self._stop_live_preview()
        try:
            self.client.close()
        except Exception:
            pass
        self.destroy()

    @staticmethod
    def _compact_status(status: dict) -> dict:
        params = status.get("params") if isinstance(status.get("params"), dict) else {}
        return {
            "device": status.get("device"),
            "opened": status.get("opened"),
            "size": f"{status.get('width')}x{status.get('height')}",
            "params": {
                "exposure_us": params.get("exposure_us"),
                "gain": params.get("gain"),
                "focus": params.get("focus"),
                "lights": params.get("lights"),
            },
        }

    @staticmethod
    def _compact_trace(payload) -> dict:
        if not isinstance(payload, dict):
            return {"value": payload}
        keys = (
            "step",
            "stage",
            "phase",
            "action",
            "finished",
            "finish_reason",
            "difficulty",
            "need_decode",
            "heatmap",
            "connected",
            "status",
            "quality",
            "candidate",
            "reset",
            "current_params",
            "next_params",
        )
        compact = {k: payload[k] for k in keys if k in payload}
        return compact or payload

    @staticmethod
    def _clamp_int(value, value_range: tuple[int, int]) -> int:
        lower, upper = value_range
        return max(lower, min(upper, int(value)))

    @staticmethod
    def _at_status_text(frame) -> str:
        if frame.trace:
            return (
                f"AT step={frame.trace.get('step')} "
                f"action={frame.trace.get('action')} "
                f"finished={frame.trace.get('finished')}"
            )
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
