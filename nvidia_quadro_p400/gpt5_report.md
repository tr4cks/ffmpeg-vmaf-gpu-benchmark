# NVIDIA Quadro P400 – HEVC NVENC CQ/VMAF Benchmark (FFmpeg)

## Table of Contents

* [What I looked at](#what-i-looked-at)
* [Executive summary (actionable)](#executive-summary-actionable)
* [Details and evidence](#details-and-evidence)

  * [1) Rate–distortion behavior](#1-rate–distortion-behavior-cq-vs-sizequality-8-bit-default-nvenc)
  * [2) Presets (p1 ⇢ p7) on 10-bit HEVC](#2-presets-p1--p7-on-10bit-hevc)
  * [3) 8-bit vs 10-bit](#3-8bit-vs-10bit-same-cq-similar-flags)
  * [4) tune hq and long GOP](#4-tune-hq-and-long-gop)
  * [5) Speed behavior](#5-speed-behavior)
  * [6) CQ decimal steps](#6-cq-decimal-steps)
  * [7) VMAF negative variants](#7-vmaf-negative-variants)
* [Recommendations](#recommendations)

  * [Near-transparent (VMAF ≈ 96–97)](#neartransparent-vmaf--9697)
  * [Balanced (VMAF ≈ 94–95.5)](#balanced-best-sizequality-trade-vmaf--94955)
  * [Bandwidth-first (VMAF ≈ 92–94)](#bandwidthfirst-vmaf--9294)
* [Notes](#notes)
* [Cheat-sheet takeaways for Quadro P400 + NVENC](#cheatsheet-takeaways-for-quadro-p400--nvenc)

---

## What I looked at

* For each run in your CSV I considered:

  * Encoding flags (the command-line snippet under flags)
  * Constant quality target (quality, i.e., -cq\:v value)
  * Output size and ratio\_size (relative to source)
  * walltime/usertime (elapsed and CPU time)
  * VMAF statistics: min, max, mean, harmonic mean (both regular and negative variants)

---

## Executive summary (actionable)

* CQ dominates quality and size. On this clip, going from CQ 22 to CQ 28 halves the size (−55%) for a small average VMAF drop (≈ −3.2). CQ 26 is a good “visually high” point; CQ 28 is an excellent size/quality trade; CQ ≥ 30 starts to show more visible loss (VMAF ≤ \~92).
* Presets p1 ⇢ p7 barely change quality or size at the same CQ, but they do change speed a lot. On Quadro P400, p7 is \~2× slower than p1 with essentially identical VMAF and size. p3–p4 are the sweet spots.
* 10-bit (yuv420p10le) slightly improves compression efficiency. At the same CQ, files are typically \~4–7% smaller with very similar VMAF. If your pipeline supports 10-bit decode, prefer 10-bit.
* tune hq and very long GOPs (e.g., -g 600 -keyint\_min 600) had negligible impact on this content. They neither improved VMAF nor changed size meaningfully.
* Decimal CQ steps (e.g., 26.1, 26.2) usually do nothing. NVENC quantizes internally; many adjacent decimals produced identical size and VMAF. Use integers.
* If you care about worst-case frames, don’t go too low. vmaf\_min dips quickly above CQ \~30. Staying at CQ 26–28 keeps worst frames in the high-80s/low-90s VMAF for this source.

---

## Details and evidence

### 1) Rate–distortion behavior (CQ vs size/quality, 8-bit, default NVENC)

* With plain -c\:v hevc\_nvenc -cq\:v X (effectively the default p4 behavior):

  * CQ 22: 36,090 KB, VMAF mean ≈ 97.29, vmaf\_min ≈ 84.93, walltime ≈ 3.15 s
  * CQ 26: 21,283 KB, VMAF mean ≈ 95.57, vmaf\_min ≈ 80.88, walltime ≈ 2.88 s
  * CQ 28: 16,194 KB, VMAF mean ≈ 94.06, vmaf\_min ≈ 76.20, walltime ≈ 2.86 s
  * CQ 30: 12,384 KB, VMAF mean ≈ 92.22, vmaf\_min ≈ 70.45, walltime ≈ 2.79 s
  * CQ 34: 7,106 KB, VMAF mean ≈ 87.20, vmaf\_min ≈ 59.91, walltime ≈ 2.73 s
* Takeaways:

  * Size falls almost linearly with CQ; quality falls gently until \~CQ 28 then more rapidly.
  * A “high quality but efficient” range is CQ 26–28 (VMAF ≈ 94–95.6 on this clip).
  * If you need VMAF ≥ 96, cap CQ at 25 or lower.

---

### 2) Presets (p1 ⇢ p7) on 10-bit HEVC

* At fixed CQ the presets barely changed VMAF or size, but changed speed a lot.

  * Example at CQ 28 (yuv420p10le -rc vbr -b\:v 0 -c\:v hevc\_nvenc -preset pX -tune hq):

    * p1: 15,344 KB, VMAF ≈ 93.62, walltime ≈ 3.45 s
    * p3: 15,345 KB, VMAF ≈ 93.64, walltime ≈ 3.30 s
    * p5: 15,320 KB, VMAF ≈ 93.72, walltime ≈ 3.55 s
    * p7: 15,236 KB, VMAF ≈ 93.72, walltime ≈ 4.95 s
* Takeaways:

  * p7 adds time without measurable quality/size benefit on this GPU and content.
  * p3–p4 deliver near-max speed with the same quality and size as p5–p7.

---

### 3) 8-bit vs 10-bit (same CQ, similar flags)

* Across CQ values, 10-bit outputs were consistently smaller (≈ 4–7%) for essentially the same VMAF.

  * CQ 26, preset p4:

    * 8-bit: 21,283 KB, VMAF ≈ 95.57
    * 10-bit: 20,147 KB, VMAF ≈ 95.38 (−0.19), size −5.3%
  * CQ 30, preset p4:

    * 8-bit: 12,384 KB, VMAF ≈ 92.22
    * 10-bit: 11,827 KB, VMAF ≈ 91.66 (−0.56), size −4.5%
* Takeaway: If decode/compatibility is fine, use yuv420p10le for a modest size reduction at the same CQ.

---

### 4) tune hq and long GOP

* Adding -tune hq didn’t move VMAF or size in any meaningful way (multiple direct pairs show identical metrics to the third decimal).
* For your clip, -g 600 -keyint\_min 600 didn’t change size/quality/speed versus default GOP. Likely the default GOP was already large enough for the sequence length and scene structure.

---

### 5) Speed behavior

* Higher CQ (lower bitrate) encodes ran slightly faster (fewer bits to produce).
* Preset dominated runtime:

  * 10-bit CQ 22: p1 ≈ 3.16 s vs p7 ≈ 6.32 s with virtually identical VMAF (97.426 vs 97.417) and size (34,386 KB vs 34,058 KB).

---

### 6) CQ decimal steps

* Many decimal CQ steps produced byte-for-byte identical files and identical VMAF (e.g., 26.0/26.1/26.2 or 26.3/26.4). NVENC quantizes CQ internally; use integer CQ values.

---

### 7) VMAF negative variants

* vmaf\_neg\_mean/harmonic were consistently \~0.7–1.0 lower than the regular means and followed the same trends. Conclusions do not change whether you look at regular or negative VMAF.

---

## Recommendations

### Near-transparent (VMAF ≈ 96–97)

```bash
# 8-bit
ffmpeg -i in -c:v hevc_nvenc -rc vbr -b:v 0 -cq:v 24 -preset p4 -pix_fmt yuv420p out.mkv

# 10-bit
ffmpeg -i in -c:v hevc_nvenc -rc vbr -b:v 0 -cq:v 24 -preset p4 -pix_fmt yuv420p10le out.mkv
```

* Why: CQ 24–25 kept VMAF ≳ 96; p4 is fast and indistinguishable in quality from slower presets.

---

### Balanced (best size/quality trade; VMAF ≈ 94–95.5)

```bash
# 8-bit
ffmpeg -i in -c:v hevc_nvenc -rc vbr -b:v 0 -cq:v 26 -preset p4 -pix_fmt yuv420p out.mkv

# 10-bit
ffmpeg -i in -c:v hevc_nvenc -rc vbr -b:v 0 -cq:v 26 -preset p4 -pix_fmt yuv420p10le out.mkv
```

* Why: CQ 26 cut size \~40% vs CQ 22 while keeping VMAF ≈ 95–96 on this clip; 10-bit shaves a few more percent.

---

### Bandwidth-first (VMAF ≈ 92–94)

```bash
# 8-bit
ffmpeg -i in -c:v hevc_nvenc -rc vbr -b:v 0 -cq:v 28 -preset p4 -pix_fmt yuv420p out.mkv

# 10-bit
ffmpeg -i in -c:v hevc_nvenc -rc vbr -b:v 0 -cq:v 28 -preset p4 -pix_fmt yuv420p10le out.mkv
```

* Why: CQ 28 roughly halves the size vs CQ 22 with only a few VMAF points lost (subjectively still good on many contents).

---

## Notes

* You can drop -tune hq and long GOP tweaks (-g/-keyint\_min): they did not help here.
* If you prefer the simplest form, -c\:v hevc\_nvenc -cq\:v X without -rc/-b\:v worked equivalently in your data (NVENC’s “CQ mode”). Using -rc vbr -b\:v 0 is a more explicit CQ workflow, but both produced the same results on this GPU/driver.
* Results are content-dependent. High-motion/noisy content may need lower CQ (e.g., 24–26) to preserve worst-case frames.
* VMAF ≈ 95+ is generally “very high quality,” ≈ 93–95 “good/excellent,” ≈ 90–92 “acceptable,” < 90 “visible losses likely,” but subjective perception varies.

---

## Cheat-sheet takeaways for Quadro P400 + NVENC

* Use integer CQ: 24–26 for high quality, 27–28 for smaller files.
* Prefer preset p3/p4; avoid p7 unless you must (it’s slower with no quality gain).
* Prefer 10-bit (yuv420p10le) when compatible; expect \~5% smaller files at the same CQ.
* Skip -tune hq and very long GOP; they didn’t improve anything in this benchmark.
