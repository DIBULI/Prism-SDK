#!/usr/bin/env bash
set -euo pipefail
# Supports classic sources.list and Ubuntu 24.04+ deb822 files.
for sources_file in /etc/apt/sources.list /etc/apt/sources.list.d/*.sources; do
  [ -f "$sources_file" ] || continue
  sed -i -E \
    's#https?://([a-z0-9.-]+\.)?(archive|security).ubuntu.com/ubuntu#http://mirrors.tuna.tsinghua.edu.cn/ubuntu#g; s#https?://ports.ubuntu.com/ubuntu-ports#http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports#g' \
    "$sources_file"
done
