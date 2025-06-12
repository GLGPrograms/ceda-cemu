FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt update && apt upgrade -y
RUN apt install -y git cmake gcc g++ clang-tidy clang-format libsdl2-dev libsdl2-mixer-dev
RUN apt install -y meson ninja-build libffi-dev libgit2-dev
RUN apt install -y doxygen sphinx python3-breathe

RUN git clone https://github.com/Snaipe/Criterion.git && cd Criterion && git checkout master && meson build && meson install -C build

RUN useradd -s /bin/bash --create-home builder
USER builder
RUN mkdir -p /home/builder/workspace

