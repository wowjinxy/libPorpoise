#!/bin/bash
meson setup build
meson configure -Dbuild_target=win64 build
meson configure -Dstandalone=true build
meson compile -C build
