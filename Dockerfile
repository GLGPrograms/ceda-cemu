FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt update && apt upgrade -y
RUN apt install -y build-essential git cmake clang-tidy clang-format wget
RUN apt install -y doxygen sphinx python3-breathe

RUN mkdir -p /build

RUN apt install -y pkg-config ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
    libaudio-dev libfribidi-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev libthai-dev
RUN cd /build && \
    wget https://github.com/libsdl-org/SDL/releases/download/release-3.4.2/SDL3-3.4.2.tar.gz -O SDL.tar.gz && \
    tar xf SDL.tar.gz && \
    cd SDL3-3.4.2 && \
    cmake -B build -DSDL_X11_XTEST=OFF && \
    make -C build -j && \
    make -C build install

RUN cd /build && \
    wget https://github.com/libsdl-org/SDL_mixer/archive/refs/tags/release-3.2.0.tar.gz && \
    tar xf release-3.2.0.tar.gz && \
    cd SDL_mixer-release-3.2.0 && \
    cmake -B build && \
    make -C build -j && \
    make -C build install

RUN apt install -y meson
RUN cd /build && \
    wget https://github.com/Snaipe/Criterion/releases/download/v2.4.3/criterion-2.4.3.tar.xz && \
    tar xf criterion-2.4.3.tar.xz && \
    cd criterion-2.4.3 && \
    meson build && \
    meson install -C build && \
    ldconfig

RUN apt install -y gcovr

RUN useradd -s /bin/bash --create-home builder
USER builder
RUN mkdir -p /home/builder/workspace

