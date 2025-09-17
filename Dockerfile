FROM nvidia/cuda:13.0.1-devel-ubuntu24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Paris

# Set timezone and update & upgrade packages
RUN ln -sf /usr/share/zoneinfo/$TZ /etc/localtime && \
    echo $TZ > /etc/timezone && \
    apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
        git python3 python3-pip python3-virtualenv nasm ninja-build xxd \
        pkg-config libx264-dev libx265-dev yasm libnuma1 libnuma-dev

# Compile and install libvmaf
RUN TMPDIR=$(mktemp -d) && cd $TMPDIR && \
    git clone https://github.com/Netflix/vmaf.git --branch v3.0.0 && \
    cd vmaf && \
    python3 -m virtualenv .venv && \
    . .venv/bin/activate && \
    pip install meson && \
    cd libvmaf && \
    meson setup build --buildtype release && \
    ninja -vC build && \
    ninja -vC build install && \
    deactivate && \
    cd / && rm -rf $TMPDIR

# Install NVENC/NVDEC headers
RUN TMPDIR=$(mktemp -d) && cd $TMPDIR && \
    git clone https://git.videolan.org/git/ffmpeg/nv-codec-headers.git --branch n12.1.14.0 && \
    cd nv-codec-headers && \
    make install && \
    cd / && rm -rf $TMPDIR

# Compile and install FFmpeg
RUN TMPDIR=$(mktemp -d) && cd $TMPDIR \
    git clone https://git.ffmpeg.org/ffmpeg.git --branch n8.0 && \
    cd ffmpeg && \
    ./configure \
      --disable-static \
      --enable-shared \
      --enable-nonfree \
      --enable-gpl \
      --enable-libx264 \
      --enable-libx265 \
      --enable-cuvid \
      --enable-nvdec \
      --enable-nvenc \
      --enable-libvmaf && \
    make -j4 && \
    make install && \
    ldconfig && \
    cd / && rm -rf $TMPDIR

# Final cleanup
RUN apt-get clean && rm -rf /var/lib/apt/lists/*

CMD ["bash"]
