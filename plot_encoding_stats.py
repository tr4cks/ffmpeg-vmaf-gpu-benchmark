import pandas as pd
import matplotlib.pyplot as plt
import re
import argparse

# -------------------------------
# 1. Parse command-line arguments
# -------------------------------
parser = argparse.ArgumentParser(description="Plot encoding statistics from a CSV file")
parser.add_argument("csv_file", help="Path to the CSV file containing encoding results")
args = parser.parse_args()
csv_file = args.csv_file

# -------------------------------
# 2. Load and prepare the data
# -------------------------------
df = pd.read_csv(csv_file, sep=",")  # adjust sep="," if needed

# Convert 'size' to numeric (KB -> float)
df["size"] = df["size"].str.replace(" KB", "", regex=False).astype(float)

# Function to convert "00:00:03:150:353" to seconds (approx)
def parse_time(t):
    parts = re.split("[:]", t)
    if len(parts) == 5:
        h, m, s, ms, us = map(int, parts)
        return h*3600 + m*60 + s + ms/1000 + us/1e6
    else:
        return None

df["walltime_sec"] = df["walltime"].apply(parse_time)
df["usertime_sec"] = df["usertime"].apply(parse_time)

# -------------------------------
# 3. Sort data by quality
# -------------------------------
df_sorted = df.sort_values(by="quality")

# -------------------------------
# 4. Basic statistics
# -------------------------------
basic_stats = {
    "size_min": df["size"].min(),
    "size_max": df["size"].max(),
    "vmaf_min": df["vmaf_mean"].min(),
    "vmaf_max": df["vmaf_mean"].max(),
    "encoding_time_min": df["walltime_sec"].min(),
    "encoding_time_max": df["walltime_sec"].max()
}

# -------------------------------
# 5. Graphs
# -------------------------------
# Relative size vs quality
plt.figure(figsize=(6,4))
plt.plot(df_sorted["quality"], df_sorted["ratio_size"], marker="o", label="Relative Size")
plt.xlabel("Quality Factor (CQ)")
plt.ylabel("Size Ratio")
plt.title("Relative Size vs CQ")
plt.legend()
plt.grid(True)
plt.show()

# Average VMAF vs quality
plt.figure(figsize=(6,4))
plt.plot(df_sorted["quality"], df_sorted["vmaf_mean"], marker="o", color="green", label="Average VMAF")
plt.xlabel("Quality Factor (CQ)")
plt.ylabel("VMAF Score")
plt.title("Average VMAF vs CQ")
plt.legend()
plt.grid(True)
plt.show()
