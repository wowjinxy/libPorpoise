#!/bin/bash
meson setup build
meson configure -Dbuild_target=linux build
meson configure -Dstandalone=true build
meson compile -C build
