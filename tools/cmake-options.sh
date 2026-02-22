#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  tools/cmake-options.sh [build_dir]

Shows all cached CMake options (names, types, and help strings).
Requires an existing build directory with CMakeCache.txt.

Examples:
  cmake -S . -B build -DPCB=X10 -DPCBREV=TX16S
  tools/cmake-options.sh build

If you want to (re)configure first:
  cmake -S . -B build -DPCB=X10 -DPCBREV=TX16S
  tools/cmake-options.sh build
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

build_dir="${1:-build}"
cache_file="${build_dir}/CMakeCache.txt"

if [[ ! -f "${cache_file}" ]]; then
  echo "CMake cache not found at: ${cache_file}"
  echo "Run configure first, then re-run this script."
  echo
  usage
  exit 1
fi

cmake -LAH -N -S . -B "${build_dir}"
