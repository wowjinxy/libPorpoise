#!/bin/bash
./standalone/scripts/setup_build_gc.sh
meson configure -Dstandalone=true build_gc
meson compile -C build_gc
