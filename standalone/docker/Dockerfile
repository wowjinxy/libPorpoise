FROM debian:13

ARG USE_DEVKITPRO=1

ENV DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture i386
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential meson libsdl2-2.0-0 libsdl2-dev xvfb \
    python3 wine wine32 wine64 libnss-unknown fuse file \
    ninja-build wget ca-certificates git clangd gcc-multilib \
    g++-multilib libsdl2-dev:i386

RUN mkdir -p /home
RUN chmod 777 /home
