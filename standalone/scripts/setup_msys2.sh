#!/bin/bash

echo Setting up MSys2 environment!

pacman -S --noconfirm make p7zip git python3 flex bison zip 

# Install 64 bit toolchain and libraries
pacman -S --noconfirm mingw-w64-ucrt-x86_64-meson mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2

# Install 32 bit toolchain and libraries
pacman -S --noconfirm mingw-w64-i686-meson mingw-w64-i686-ninja mingw-w64-i686-gcc mingw-w64-i686-SDL2

# Useful git settings
git config --global core.autocrlf true
