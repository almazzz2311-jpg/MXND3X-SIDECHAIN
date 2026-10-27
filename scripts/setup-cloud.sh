#!/usr/bin/env bash
# Rootless Debian 13 cloud setup. Uses signed official APT indexes and verified TLS.
set -euo pipefail
cd "$(dirname "$0")/.."
deps=/workspace/deps
mkdir -p "$deps/python" "$deps/debs" "$deps/apt/lists/partial" "$deps/apt/cache/archives/partial" "$deps/sysroot"
if [[ ! -x "$deps/python/cmake/data/bin/cmake" ]]; then
 pip install --target "$deps/python" cmake==3.31.6 ninja==1.11.1.4
fi
if [[ ! -d "$deps/JUCE/.git" ]]; then
 git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git "$deps/JUCE"
fi
[[ "$(git -C "$deps/JUCE" rev-parse HEAD)" == 51a8a6d7aeae7326956d747737ccf1575e61e209 ]] || { echo 'JUCE checkout is not the pinned 8.0.6 revision' >&2; exit 1; }
if [[ ! -f "$deps/sysroot/usr/share/pkgconfig/bzip2.pc" || ! -x "$deps/sysroot/usr/bin/Xvfb" ]]; then
 printf '%s\n' 'deb https://deb.debian.org/debian trixie main' > "$deps/apt/sources.list"
 apt_options=(-o "Dir::Etc::sourcelist=$deps/apt/sources.list" -o Dir::Etc::sourceparts=- -o "Dir::State::lists=$deps/apt/lists" -o "Dir::Cache=$deps/apt/cache")
 apt-get "${apt_options[@]}" update
 packages=(libasound2-dev libasound2t64 libfreetype-dev libfreetype6 libfontconfig-dev libfontconfig1 libx11-dev libxext-dev libxinerama-dev libxinerama1 libxrandr-dev libxrandr2 libxcursor-dev libxcursor1 libxrender-dev libxfixes-dev libxft-dev libharfbuzz-dev libharfbuzz0b libharfbuzz-subset0 libgraphite2-3 libpng-dev libpng16-16t64 libbrotli-dev libbz2-dev x11proto-dev xtrans-dev libxcb1-dev libice-dev libsm-dev xvfb xserver-common libxfont2 x11-xkb-utils xfonts-base xfonts-utils xfonts-encodings libfontenc1 libxkbfile1 libunwind8)
 requested=();for package in "${packages[@]}"; do requested+=("$package/trixie"); done
 (cd "$deps/debs"; apt-get "${apt_options[@]}" download "${requested[@]}")
 for package in "$deps/debs/"*.deb; do dpkg-deb -x "$package" "$deps/sysroot"; done
fi
# Development .so links can point at libraries already provided by the cloud image.
python3 - <<'PY'
from pathlib import Path
root=Path('/workspace/deps/sysroot/usr/lib/x86_64-linux-gnu')
for p in root.glob('*.so'):
    if p.is_symlink() and not p.exists():
        target=p.readlink(); system=Path('/usr/lib/x86_64-linux-gnu')/target.name
        if system.exists(): (p.parent/target.name).symlink_to(system)
PY
bash scripts/cloud-build.sh
