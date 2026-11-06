#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT

git clone --quiet https://github.com/SonixQMK/SonixFlasherC.git "$build_dir/source"
git -C "$build_dir/source" checkout --quiet --detach e6ba5db11f703f89fd29a5bc0e56ed1ae42604c3
make -C "$build_dir/source" BACKEND=libusb
install -Dm755 "$build_dir/source/sonixflasher" "$HOME/.local/bin/sonixflasher"
printf '%s\n' "Installed $HOME/.local/bin/sonixflasher"
