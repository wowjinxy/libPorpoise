#!/bin/bash

echo Setting up MSys2 environment!

pacman -S --noconfirm make p7zip git python3 meson flex bison ninja mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 zip
git config --global core.autocrlf true
