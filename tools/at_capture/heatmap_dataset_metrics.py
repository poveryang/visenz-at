#!/usr/bin/env python3
"""Compute first-pass metrics for heatmap validation captures."""

from __future__ import annotations

import argparse
import json
import statistics
from pathlib import Path
from typing import Any

import cv2
import numpy as np


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if line:
                records.append(json.loads(line))
    return records


def load_labels(path: Path | None) -> dict[str, dict[str, int]]:
    if path is None:
        return {}
    labels: dict[str, dict[str, int]] = {}
    for record in load_jsonl(path):
        image_path = str(record.get("image_path", ""))
        roi = record.get("roi") or record.get("target_roi")
        if image_path and isinstance(roi, dict):
            labels[image_path] = normalize_rect(roi)
    return labels


def normalize_rect(value: dict[str, Any] | None) -> dict[str, int]:
    if not isinstance(value, dict):
        return {"x": 0, "y": 0, "width": 0, "height": 0}
    return {
        "x": int(value.get("x", 0)),
        "y": int(value.get("y", 0)),
        "width": int(value.get("width", 0)),
        "height": int(value.get("height", 0)),
    }


def rect_area(rect: dict[str, int]) -> int:
    return max(0, rect["width"]) * max(0, rect["height"])


def rect_iou(a: dict[str, int], b: dict[str, int]) -> float:
    ax2 = a["x"] + a["width"]
    ay2 = a["y"] + a["height"]
    bx2 = b["x"] + b["width"]
    by2 = b["y"] + b["height"]
    ix1 = max(a["x"], b["x"])
    iy1 = max(a["y"], b["y"])
    ix2 = min(ax2, bx2)
    iy2 = min(ay2, by2)
    inter = max(0, ix2 - ix1) * max(0, iy2 - iy1)
    union = rect_area(a) + rect_area(b) - inter
    return float(inter / union) if union > 0 else 0.0


def center_inside(inner: dict[str, int], outer: dict[str, int]) -> bool:
    if rect_area(inner) <= 0 or rect_area(outer) <= 0:
        return False
    cx = inner["x"] + inner["width"] / 2.0
    cy = inner["y"] + inner["height"] / 2.0
    return outer["x"] <= cx <= outer["x"] + outer["width"] and outer["y"] <= cy <= outer["y"] + outer["height"]


def image_quality(path: Path) -> dict[str, float]:
    image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if image is None:
        return {
            "brightness": 0.0,
            "contrast": 0.0,
            "sharpness": 0.0,
            "saturation_ratio": 0.0,
            "noise_proxy": 0.0,
            "width": 0.0,
            "height": 0.0,
        }
    if image.ndim == 2:
        gray = image
    elif image.shape[2] == 4:
        gray = cv2.cvtColor(image, cv2.COLOR_BGRA2GRAY)
    else:
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    gray_f = gray.astype(np.float32)
    blur = cv2.GaussianBlur(gray_f, (5, 5), 0)
    high_pass = gray_f - blur
    return {
        "brightness": float(np.mean(gray_f)),
        "contrast": float(np.std(gray_f)),
        "sharpness": float(cv2.Laplacian(gray, cv2.CV_64F).var()),
        "saturation_ratio": float(np.mean((gray <= 3) | (gray >= 252))),
        "noise_proxy": float(np.std(high_pass)),
        "width": float(gray.shape[1]),
        "height": float(gray.shape[0]),
    }


def percentile(values: list[float], ratio: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round((len(ordered) - 1) * ratio)))
    return float(ordered[index])


def summarize(values: list[float]) -> dict[str, float]:
    if not values:
        return {"count": 0, "mean": 0.0, "min": 0.0, "p50": 0.0, "p90": 0.0, "max": 0.0}
    return {
        "count": len(values),
        "mean": float(statistics.fmean(values)),
        "min": float(min(values)),
        "p50": percentile(values, 0.5),
        "p90": percentile(values, 0.9),
        "max": float(max(values)),
    }


def get_group_value(record: dict[str, Any], sample: dict[str, Any], key: str) -> Any:
    axes = (((record.get("metadata") or {}).get("dataset") or {}).get("axes") or {})
    if key in axes:
        return axes[key]
    params = record.get("params") or {}
    if key in params:
        return params[key]
    return sample.get(key)


def compute_metrics(args: argparse.Namespace) -> None:
    records = load_jsonl(args.manifest)
    labels = load_labels(args.labels)
    output_root = args.manifest.parent
    sample_metrics: list[dict[str, Any]] = []

    for record in records:
        rel_path = str(record.get("image_path", ""))
        image_path = output_root / rel_path
        quality = image_quality(image_path)
        trace = record.get("trace") if isinstance(record.get("trace"), dict) else {}
        heatmap = trace.get("heatmap") if isinstance(trace.get("heatmap"), dict) else {}
        perf = trace.get("heatmap_perf") if isinstance(trace.get("heatmap_perf"), dict) else {}
        roi = normalize_rect(heatmap.get("roi") if isinstance(heatmap, dict) else None)
        image_area = max(1.0, quality["width"] * quality["height"])

        label_roi = labels.get(rel_path)
        metadata_label = ((record.get("metadata") or {}).get("label_roi") or None)
        if label_roi is None and isinstance(metadata_label, dict):
            label_roi = normalize_rect(metadata_label)

        sample = {
            "image_path": rel_path,
            "params": record.get("params", {}),
            "brightness": quality["brightness"],
            "contrast": quality["contrast"],
            "sharpness": quality["sharpness"],
            "saturation_ratio": quality["saturation_ratio"],
            "noise_proxy": quality["noise_proxy"],
            "heatmap_available": bool(heatmap.get("available", False)) if isinstance(heatmap, dict) else False,
            "heatmap_confidence": float(heatmap.get("confidence", 0.0)) if isinstance(heatmap, dict) else 0.0,
            "feature_strength": float(heatmap.get("feature_strength", 0.0)) if isinstance(heatmap, dict) else 0.0,
            "roi_area_ratio": rect_area(roi) / image_area,
            "infer_ms": float(perf.get("last_model_ms", 0.0)) if isinstance(perf, dict) else 0.0,
        }
        if label_roi is not None:
            sample["label_iou"] = rect_iou(roi, label_roi)
            sample["roi_center_inside_label"] = center_inside(roi, label_roi)
        sample_metrics.append(sample)

    summary = {
        "manifest": str(args.manifest),
        "sample_count": len(sample_metrics),
        "heatmap_available_rate": (
            sum(1 for item in sample_metrics if item["heatmap_available"]) / len(sample_metrics)
            if sample_metrics
            else 0.0
        ),
        "confidence": summarize([item["heatmap_confidence"] for item in sample_metrics]),
        "feature_strength": summarize([item["feature_strength"] for item in sample_metrics]),
        "roi_area_ratio": summarize([item["roi_area_ratio"] for item in sample_metrics]),
        "brightness": summarize([item["brightness"] for item in sample_metrics]),
        "contrast": summarize([item["contrast"] for item in sample_metrics]),
        "sharpness": summarize([item["sharpness"] for item in sample_metrics]),
        "infer_ms": summarize([item["infer_ms"] for item in sample_metrics if item["infer_ms"] > 0.0]),
    }
    if any("label_iou" in item for item in sample_metrics):
        labelled = [item for item in sample_metrics if "label_iou" in item]
        summary["labelled_count"] = len(labelled)
        summary["label_iou"] = summarize([item["label_iou"] for item in labelled])
        summary["roi_center_inside_label_rate"] = (
            sum(1 for item in labelled if item["roi_center_inside_label"]) / len(labelled)
            if labelled
            else 0.0
        )

    if args.group_by:
        groups: dict[str, list[dict[str, Any]]] = {}
        for record, sample in zip(records, sample_metrics):
            key = "|".join(f"{field}={get_group_value(record, sample, field)}" for field in args.group_by)
            groups.setdefault(key, []).append(sample)
        summary["groups"] = {
            key: {
                "sample_count": len(items),
                "available_rate": sum(1 for item in items if item["heatmap_available"]) / len(items),
                "confidence": summarize([item["heatmap_confidence"] for item in items]),
                "roi_area_ratio": summarize([item["roi_area_ratio"] for item in items]),
                "brightness": summarize([item["brightness"] for item in items]),
                "sharpness": summarize([item["sharpness"] for item in items]),
            }
            for key, items in sorted(groups.items())
        }

    args.output.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    if args.samples_output:
        with args.samples_output.open("w", encoding="utf-8") as fp:
            for sample in sample_metrics:
                fp.write(json.dumps(sample, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(json.dumps(summary, ensure_ascii=False, indent=2))


def main() -> None:
    parser = argparse.ArgumentParser(description="Compute heatmap dataset metrics")
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("heatmap_metrics.json"))
    parser.add_argument("--samples-output", type=Path)
    parser.add_argument("--labels", type=Path, help="optional JSONL: {image_path, roi:{x,y,width,height}}")
    parser.add_argument("--group-by", nargs="*", default=["gain", "lights"])
    args = parser.parse_args()
    compute_metrics(args)


if __name__ == "__main__":
    main()
