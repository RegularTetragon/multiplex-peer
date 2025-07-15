# this file will be rm'd once nix fixes their cross compilation with windows >:(
FROM debian:latest
RUN apt-get update
RUN apt-get upgrade
RUN apt-get install -y \
  build-essential \
  scons \
  pkg-config \
  libx11-dev \
  libxcursor-dev \
  libxinerama-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libasound2-dev \
  libpulse-dev \
  libudev-dev \
  libxi-dev \
  libxrandr-dev \
  libwayland-dev \
  mingw-w64
COPY . /root/multiplex-peer
WORKDIR /root/multiplex-peer
ENTRYPOINT scons