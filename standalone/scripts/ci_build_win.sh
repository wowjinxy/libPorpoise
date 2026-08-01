#!/bin/bash
meson setup build --buildtype=release
meson configure -Dbuild_target=win64 build
meson configure -Dstandalone=true build
meson compile -C build
