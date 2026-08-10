#!/bin/bash
meson setup build
meson configure -Dbuild_target=linux64 build
meson configure -Dstandalone=true build
meson compile -C build
