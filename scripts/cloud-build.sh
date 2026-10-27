#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
deps=/workspace/deps
export PKG_CONFIG_PATH="$deps/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$deps/sysroot/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PKG_CONFIG_SYSROOT_DIR="$deps/sysroot"
export CPLUS_INCLUDE_PATH="$deps/sysroot/usr/include:$deps/sysroot/usr/include/freetype2${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
export LIBRARY_PATH="$deps/sysroot/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
cmake_bin="$deps/python/cmake/data/bin/cmake"
"$cmake_bin" -S . -B build -DJUCE_DIR="$deps/JUCE" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$deps/sysroot/usr" -DCMAKE_INCLUDE_PATH="$deps/sysroot/usr/include" -DCMAKE_LIBRARY_PATH="$deps/sysroot/usr/lib/x86_64-linux-gnu"
"$cmake_bin" --build build --parallel 4
export DISPLAY="${DISPLAY:-:99}"
if [[ ! -S /tmp/.X11-unix/X${DISPLAY#:} ]]; then
 mkdir -p /tmp/.X11-unix
 "$deps/sysroot/usr/bin/Xvfb" "$DISPLAY" -screen 0 1600x1200x24 -nolisten tcp -ac -fp "$deps/sysroot/usr/share/fonts/X11/misc" > /tmp/mxnd3x-xvfb.log 2>&1 &
 mxnd3x_xvfb_pid=$!
 trap 'kill "$mxnd3x_xvfb_pid" 2>/dev/null || true' EXIT
 for attempt in {1..50}; do [[ -S /tmp/.X11-unix/X${DISPLAY#:} ]] && break; sleep 0.1; done
fi
"$deps/python/cmake/data/bin/ctest" --test-dir build --output-on-failure
