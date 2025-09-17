# FFmpeg GPU Transcoding & VMAF Benchmark

This project allows testing different **FFmpeg** configurations using GPU acceleration with an **NVIDIA Quadro P400**, and evaluating the visual quality of the generated videos with **VMAF**.

The goal is to compare multiple encoding settings on the same source video (containing diverse scenes: motion, static, dark, etc.), and produce a CSV report with both performance and quality metrics.

---

## 🎯 Goals

* Test different encoding configurations with **FFmpeg** and NVENC/NVDEC
* Measure the impact of each setting on:

  * output file size
  * encoding speed
  * perceived quality (**VMAF** and **VMAF NEG**)
* Generate a consolidated CSV report

---

## 📦 Project Contents

* **Dockerfile**

  * Builds **FFmpeg 8.0** with NVENC/NVDEC, libx264/libx265, and libvmaf support
  * Installs **VMAF v3.0.0** and NVENC headers
  * Based on CUDA, adapted for the **NVIDIA Quadro P400**

* **Python script (`ffmpeg_vmaf_benchmark.py`)**

  * Reads a CSV file describing FFmpeg encoding settings
  * Runs GPU-accelerated transcodes
  * Automatically computes **VMAF** and **VMAF NEG**
  * Produces a CSV report (file size, ratio, encoding time, quality metrics)

* **Examples**

  * Input CSV with encoding parameters
  * Output CSV with performance and quality results

---

## 🛠️ Requirements

* **NVIDIA GPU** with NVENC support (tested with Quadro P400)
* **Docker** and **NVIDIA Container Toolkit** installed
* A **source video** with varied scenes (motion, static, different lighting)

---

## 📦 Build the Docker image

```bash
docker build -t ffmpeg-vmaf-nvidia .
```

---

## ▶️ Usage

### 1. Prepare a configuration CSV

Example (`encoding_settings.csv`):

```csv
quality;flags
22;-c:v hevc_nvenc -cq:v 22
23;-c:v hevc_nvenc -cq:v 23
24;-c:v hevc_nvenc -cq:v 24
25;-c:v hevc_nvenc -cq:v 25
```

Each line defines a configuration to test:

* **quality**: identifier/quality parameter
* **flags**: FFmpeg options

---

### 2. Run the container

```bash
docker run --gpus all -it --rm \
    -v $(pwd):/workspace \
    ffmpeg-vmaf-nvidia bash
```

---

### 3. Launch the tests

Inside the container:

```bash
cd /workspace
python3 ffmpeg_vmaf_benchmark.py \
    --input_csv encoding_settings.csv \
    --source_video input.mp4 \
    --output_dir results_videos \
    --output_csv results.csv \
    --max_transcode_workers 1 \
    --max_vmaf_workers 2
```

---

## 📊 Example Results

### Example of generated CSV (`results.csv`):

```csv
index,quality,flags,size,ratio_size,walltime,usertime,vmaf_min,vmaf_max,vmaf_mean,vmaf_harmonic_mean,vmaf_neg_min,vmaf_neg_max,vmaf_neg_mean,vmaf_neg_harmonic_mean
0,22,"-c:v hevc_nvenc -cq:v 22","13706.75 KB",0.926,00:00:02:943:430,00:00:00:000:424,95.48,100.0,97.88,97.87,91.57,100.0,97.11,97.10
1,23,"-c:v hevc_nvenc -cq:v 23","11826.97 KB",0.799,00:00:02:929:708,00:00:00:001:107,95.35,100.0,97.86,97.85,91.70,100.0,97.01,97.00
```

Key columns:

* **size**: generated file size
* **ratio\_size**: output size / source size ratio
* **walltime**: actual transcoding time
* **usertime**: CPU time consumed
* **vmaf / vmaf\_neg**: min, max, mean, harmonic mean scores

---

## ⚙️ Script Options

| Option                    | Description                    | Default              |
| ------------------------- | ------------------------------ | -------------------- |
| `--input_csv`             | CSV file with FFmpeg configs   | **required**         |
| `--source_video`          | Source video path              | **required**         |
| `--output_dir`            | Directory for encoded videos   | `out`                |
| `--output_csv`            | CSV results file               | `output_results.csv` |
| `--max_transcode_workers` | Max parallel transcodes        | `1`                  |
| `--max_vmaf_workers`      | Max parallel VMAF calculations | `2`                  |
| `--sequential`            | sequential execution           | `false`              |

---

## 📝 Notes

* The Dockerfile includes CUDA and dependencies for the Quadro P400.
* The project can be used with any NVIDIA GPU supporting NVENC.
* VMAF is computed twice:

  * **Standard VMAF**
  * **VMAF NEG** (stricter model).

---

## 🔗 Inspired by

This project was inspired by the article: [Benchmarking FFMPEG's H.265 Options](https://scottstuff.net/posts/2025/03/17/benchmarking-ffmpeg-h265/)
