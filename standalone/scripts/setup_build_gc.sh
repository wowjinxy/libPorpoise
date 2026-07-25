#!/bin/bash
python3 standalone/tools/download_tool.py compilers compilers --tag 20251118
rm -rf compilers/X360
if [[ "$(uname -s)" =~ ^MSYS_NT.* ]]; then
    meson setup build_gc --cross-file=standalone/meson/cross_on_windows.ini
else
    python3 standalone/tools/download_tool.py wibo compilers/wibo --tag 1.2.0
    meson setup build_gc --cross-file=standalone/meson/cross_on_linux.ini
fi
