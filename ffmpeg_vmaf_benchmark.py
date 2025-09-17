#!/usr/bin/env python3

import csv
import os
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import threading
import argparse
from typing import Dict, Tuple, Any

# Type alias for row data
RowData = Dict[str, Any]

# -----------------------------
# Argument parsing
# -----------------------------
parser = argparse.ArgumentParser(description="Transcode videos and compute VMAF from a CSV input.")
parser.add_argument("--input_csv", required=True, help="Path to input CSV file (required).")
parser.add_argument("--source_video", required=True, help="Path to source video (required).")
parser.add_argument("--output_dir", default="out", help="Directory to save output videos (default: out).")
parser.add_argument("--output_csv", default="output_results.csv", help="CSV file to write results (default: output_results.csv).")
parser.add_argument("--max_transcode_workers", type=int, default=1, help="Maximum number of concurrent transcodings (default: 1).")
parser.add_argument("--max_vmaf_workers", type=int, default=2, help="Maximum number of concurrent VMAF calculations (default: 2).")
parser.add_argument("--sequential", action="store_true", help="Execute transcoding and VMAF sequentially instead of in parallel.")

args = parser.parse_args()

input_csv = args.input_csv
source_video = args.source_video
output_dir = args.output_dir
output_csv = args.output_csv
max_transcode_workers = args.max_transcode_workers
max_vmaf_workers = args.max_vmaf_workers
sequential = args.sequential

# -----------------------------
# Prepare environment
# -----------------------------
os.makedirs(output_dir, exist_ok=True)
csv_lock = threading.Lock()

# Initialize CSV fieldnames
fieldnames = [
    "index", "quality", "flags", "size", "ratio_size", "walltime", "usertime",
    "vmaf_min", "vmaf_max", "vmaf_mean", "vmaf_harmonic_mean",
    "vmaf_neg_min", "vmaf_neg_max", "vmaf_neg_mean", "vmaf_neg_harmonic_mean"
]
with open(output_csv, 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    writer.writeheader()

# -----------------------------
# Helper functions
# -----------------------------
def human_size_kb(num_bytes: int) -> str:
    """Convert bytes to kilobytes with 2 decimals."""
    return f"{num_bytes / 1024:.2f} KB"

def human_time_hms_ms_us(seconds: float) -> str:
    """
    Convert seconds to hh:mm:ss:ms:µs format.
    Examples:
      0.000345 -> "00:00:00:000:345"
      0.003456 -> "00:00:00:03:456"
      2.345678 -> "00:00:02:345:678"
    """
    h = int(seconds // 3600)
    m = int((seconds % 3600) // 60)
    s = int(seconds % 60)
    ms = int((seconds - int(seconds)) * 1000)
    us = int((seconds*1_000_000) % 1000)  # remaining microseconds
    return f"{h:02d}:{m:02d}:{s:02d}:{ms:03d}:{us:03d}"

# -----------------------------
# Functions
# -----------------------------
def transcode_function(idx: int, row: RowData) -> Tuple[int, RowData]:
    """Perform video transcoding using FFmpeg and return metrics."""
    quality = row['quality']
    flags = row['flags']
    base_ext = os.path.splitext(source_video)[1]
    output_file = os.path.join(output_dir, f"{idx}{base_ext}")

    print(f"[TRANSCODE START] Index {idx}, Quality {quality}, Flags: {flags}")

    ffmpeg_cmd = f"ffmpeg -hide_banner -loglevel error -hwaccel cuda -i {source_video} -map 0:v {flags} {output_file}"

    start_wall = time.time()
    start_cpu = time.process_time()
    subprocess.run(ffmpeg_cmd, shell=True, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    walltime = time.time() - start_wall
    usertime = time.process_time() - start_cpu

    size = os.path.getsize(output_file)
    source_size = os.path.getsize(source_video)
    ratio_size = size / source_size if source_size > 0 else None

    print(f"[TRANSCODE END] Index {idx}, Output: {output_file}, Walltime: {walltime:.2f}s")

    return idx, {
        "quality": quality,
        "flags": flags,
        "output_file": output_file,  # kept internally but ignored in CSV
        "size": size,
        "ratio_size": ratio_size,
        "walltime": walltime,
        "usertime": usertime,
        "vmaf_min": "",
        "vmaf_max": "",
        "vmaf_mean": "",
        "vmaf_harmonic_mean": "",
        "vmaf_neg_min": "",
        "vmaf_neg_max": "",
        "vmaf_neg_mean": "",
        "vmaf_neg_harmonic_mean": ""
    }

def run_vmaf(idx: int, output_file: str) -> Tuple[Dict[str, float], Dict[str, float]]:
    """Compute VMAF and VMAF_neg scores using FFmpeg."""
    print(f"[VMAF START] Index {idx}, File: {output_file}")

    base_name = os.path.splitext(os.path.basename(output_file))[0]
    vmaf_file = os.path.join(output_dir, f"{base_name}_vmaf.json")
    vmaf_neg_file = os.path.join(output_dir, f"{base_name}_vmaf_neg.json")

    cmds = [
        f'ffmpeg -hide_banner -loglevel error -i {source_video} -i {output_file} -lavfi "libvmaf=log_fmt=json:log_path={vmaf_file}" -f null -',
        f'ffmpeg -hide_banner -loglevel error -i {source_video} -i {output_file} -lavfi "libvmaf=model=version=vmaf_v0.6.1neg:log_fmt=json:log_path={vmaf_neg_file}" -f null -'
    ]

    for cmd in cmds:
        subprocess.run(cmd, shell=True, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    with open(vmaf_file) as f:
        vmaf_data = json.load(f).get("pooled_metrics", {}).get("vmaf", {})
    with open(vmaf_neg_file) as f:
        vmaf_neg_data = json.load(f).get("pooled_metrics", {}).get("vmaf", {})

    print(f"[VMAF END] Index {idx}")

    return vmaf_data, vmaf_neg_data

def write_csv_partial(idx: int, row_data: RowData):
    """Write a partial row to CSV safely."""
    row_to_write = {k: v for k, v in row_data.items() if k in fieldnames}
    row_to_write["index"] = idx

    # Format human-readable values
    if row_to_write.get("ratio_size") is not None:
        row_to_write["ratio_size"] = round(row_to_write["ratio_size"], 3)
    if row_to_write.get("size") is not None:
        row_to_write["size"] = human_size_kb(row_to_write["size"])
    if row_to_write.get("walltime") is not None:
        row_to_write["walltime"] = human_time_hms_ms_us(row_to_write["walltime"])
    if row_to_write.get("usertime") is not None:
        row_to_write["usertime"] = human_time_hms_ms_us(row_to_write["usertime"])

    with csv_lock:
        with open(output_csv, 'a', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writerow(row_to_write)
            f.flush()

def update_csv_with_vmaf(idx: int, vmaf_result: Tuple[Dict[str, float], Dict[str, float]]):
    """Update CSV row with detailed VMAF metrics."""
    vmaf_data, vmaf_neg_data = vmaf_result
    with csv_lock:
        # Read all existing lines
        lines = []
        with open(output_csv, 'r', newline='') as f:
            reader = csv.DictReader(f)
            for row in reader:
                lines.append(row)
        # Update the corresponding row
        for row in lines:
            if int(row["index"]) == idx:
                row["vmaf_min"] = vmaf_data.get("min")
                row["vmaf_max"] = vmaf_data.get("max")
                row["vmaf_mean"] = vmaf_data.get("mean")
                row["vmaf_harmonic_mean"] = vmaf_data.get("harmonic_mean")

                row["vmaf_neg_min"] = vmaf_neg_data.get("min")
                row["vmaf_neg_max"] = vmaf_neg_data.get("max")
                row["vmaf_neg_mean"] = vmaf_neg_data.get("mean")
                row["vmaf_neg_harmonic_mean"] = vmaf_neg_data.get("harmonic_mean")
                break
        # Rewrite CSV
        with open(output_csv, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(lines)
            f.flush()

# -----------------------------
# Main execution
# -----------------------------
with open(input_csv, newline='') as csvfile:
    reader = list(csv.DictReader(csvfile, delimiter=';'))  # support CSV with ;

if sequential:
    # Sequential execution
    for idx, row in enumerate(reader):
        idx, row_data = transcode_function(idx, row)
        write_csv_partial(idx, row_data)

        vmaf_result = run_vmaf(idx, row_data["output_file"])
        update_csv_with_vmaf(idx, vmaf_result)
else:
    # Parallel execution

    # Thread pools
    transcode_executor = ThreadPoolExecutor(max_workers=max_transcode_workers)
    vmaf_executor = ThreadPoolExecutor(max_workers=max_vmaf_workers)

    # Submit transcoding jobs
    transcode_futures = [transcode_executor.submit(transcode_function, idx, row)
                     for idx, row in enumerate(reader)]

    # Process completed transcodings
    for fut in as_completed(transcode_futures):
        idx, row_data = fut.result()
        write_csv_partial(idx, row_data)

        # Submit VMAF calculation
        future_vmaf = vmaf_executor.submit(run_vmaf, idx, row_data["output_file"])
        future_vmaf.add_done_callback(lambda fut, idx=idx: update_csv_with_vmaf(idx, fut.result()))

    # Wait for all VMAF tasks to finish
    vmaf_executor.shutdown(wait=True)
    transcode_executor.shutdown(wait=True)

print("Pipeline completed successfully.")
