#!/usr/bin/env bash
# =============================================================================
# codectx — construtor multiplataforma
#
# Gera binarios para:
#   linux/<host>          : build nativo (make)
#   linux/x86_64|aarch64|arm32 : containers docker --platform (como no CI)
#   windows/amd64|x86     : mingw-w64 (binario estatico, zero dependencias)
#
# Uso:
#   ./cross.sh                # tudo que for possivel no ambiente atual
#   ./cross.sh native         # apenas o alvo nativo
#   ./cross.sh windows        # apenas os alvos Windows (mingw)
#   ./cross.sh linux          # apenas os alvos Linux via docker
# =============================================================================
set -euo pipefail

OUT=dist
VERSION=$(grep -m1 '^VERSION' Makefile | awk '{print $3}')
mkdir -p "$OUT"

ok()   { printf '  \033[32mOK\033[0m    %s\n' "$1"; }
skip() { printf '  \033[33mSKIP\033[0m  %s\n' "$1"; }
fail() { printf '  \033[31mERRO\033[0m  %s\n' "$1"; exit 1; }

tem() { command -v "$1" > /dev/null 2>&1; }

build_linux_container() {
  local arch=$1 platform=$2
  if ! tem docker; then skip "linux/$arch (docker indisponivel)"; return; fi
  echo "  -> linux/$arch (container $platform)"
  docker run --rm --platform "$platform" \
    -v "$(pwd):/src" -w /src \
    debian:bookworm-slim \
    bash -c '
      apt-get update -qq &&
      apt-get install -y -qq g++ make > /dev/null 2>&1 &&
      make all' || fail "linux/$arch"
  cp codectx "$OUT/codectx-linux-$arch"
  ok "dist/codectx-linux-$arch"
}

build_windows_mingw() {
  local arch=$1 cxx=$2
  if ! tem "$cxx"; then
    skip "windows/$arch (instale: apt install g++-mingw-w64-${arch/amd64/x86-64})"
    return
  fi
  echo "  -> windows/$arch ($cxx)"
  make clean > /dev/null
  # ld PE acrescenta .exe ao -o sem extensao
  make TARGET_OS=Windows_NT CXX="$cxx" all > /dev/null
  cp codectx.exe "$OUT/codectx-windows-$arch.exe"
  ok "dist/codectx-windows-$arch.exe"
}

case "${1:-all}" in
  native)
    echo "[native $(uname -m)]"
    make clean > /dev/null && make all > /dev/null
    cp codectx "$OUT/codectx-linux-$(uname -m)"
    ok "dist/codectx-linux-$(uname -m)"
    ;;
  windows)
    echo "[windows via mingw-w64] v$VERSION"
    build_windows_mingw amd64 x86_64-w64-mingw32-g++
    make clean > /dev/null
    build_windows_mingw x86 i686-w64-mingw32-g++
    ;;
  linux)
    echo "[linux via containers docker]"
    HOST_ARCH=$(uname -m)
    case "$HOST_ARCH" in
      x86_64)  build_linux_container x86_64 linux/amd64 ;;
      aarch64) build_linux_container aarch64 linux/arm64 ;;
    esac
    build_linux_container arm32 linux/arm/v7
    ;;
  all)
    echo "[codectx v$VERSION — build multiplataforma]"
    "$0" native || true
    echo ""
    "$0" windows || true
    echo ""
    "$0" linux || true
    echo ""
    echo "Artefatos em ./$OUT:"
    ls -lh "$OUT" 2> /dev/null | tail -n +2 | awk '{print "  " $9 " (" $5 ")"}'
    ;;
  *) echo "uso: $0 [all|native|windows|linux]" && exit 1 ;;
esac

make clean > /dev/null 2>&1 || true
