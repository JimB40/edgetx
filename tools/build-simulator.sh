#!/usr/bin/env bash
set -euo pipefail

cleanup() {
  stty sane 2>/dev/null || true
}

trap 'echo; echo "Cancelled."; cleanup; return 130 2>/dev/null || exit 130' INT
trap 'cleanup' EXIT

if [[ -n "${BASH_SOURCE:-}" ]]; then
  script_path="${BASH_SOURCE[0]}"
elif [[ -n "${ZSH_VERSION:-}" ]]; then
  script_path="${(%):-%N}"
else
  script_path="$0"
fi

script_dir="$(cd "$(dirname "${script_path}")" && pwd)"
root_dir="$(cd "${script_dir}/.." && pwd)"

list_cmd="${script_dir}/list-radios.py"
use_fzf=1
preselect=""

for arg in "$@"; do
  case "${arg}" in
    --txt)
      use_fzf=0
      ;;
    --select=*)
      preselect="${arg#*=}"
      ;;
    --select)
      shift
      preselect="${1:-}"
      ;;
  esac
done

if [[ ! -x "${list_cmd}" ]]; then
  echo "Missing executable: ${list_cmd}"
  echo "Run: chmod +x tools/list-radios.py"
  exit 1
fi

if [[ -n "${preselect}" ]]; then
  idx="${preselect}"
elif [[ "${use_fzf}" -eq 1 && -t 1 ]] && command -v fzf >/dev/null 2>&1; then
  selection="$("${list_cmd}" | awk 'NR>1 && $1 ~ /^[0-9]+\.$/ {print $0}' | fzf --height=40% --reverse --prompt="Select radio for simulator build: " --no-multi || true)"
  if [[ -z "${selection}" ]]; then
    return 0 2>/dev/null || exit 0
  fi
  idx="$(echo "${selection}" | awk '{print $1}' | sed 's/\.//')"
else
  "${list_cmd}"
  echo
  printf "Select radio for simulator build: "
  read -r idx
fi

if [[ -z "${idx}" ]]; then
  return 0 2>/dev/null || exit 0
fi

if [[ ! "${idx}" =~ ^[0-9]+$ ]]; then
  echo "Invalid selection."
  exit 1
fi

flags="$("${list_cmd}" --select "${idx}")"
if [[ -z "${flags}" ]]; then
  echo "Failed to resolve selection."
  exit 1
fi

pcb="$(echo "${flags}" | awk -F= '{print $2}' | awk '{print $1}')"
pcbrev="$(echo "${flags}" | awk -F= '{print $3}' | awk '{print $1}')"

export EDGETX_PCB="${pcb}"
export EDGETX_PCBREV="${pcbrev:-}"

echo "Building simulator"
echo "Selected: PCB=${EDGETX_PCB} PCBREV=${EDGETX_PCBREV}"
echo
printf "Clean build (rm -rf build)? [y/N]: "
read -r clean_answer
if [[ "${clean_answer}" =~ ^[Yy]$ ]]; then
  rm -rf "${root_dir}/build"
  echo "Removed ${root_dir}/build"
fi

echo "Run:"
if [[ -n "${EDGETX_PCBREV}" ]]; then
  cmake_cmd=(cmake -S . -B build -DPCB="${EDGETX_PCB}" -DPCBREV="${EDGETX_PCBREV}")
else
  cmake_cmd=(cmake -S . -B build -DPCB="${EDGETX_PCB}")
fi
printf "  %q " "${cmake_cmd[@]}"
echo
echo "  cmake --build build --target simulator"

printf "Configure and build simulator now? [y/N]: "
read -r run_answer
if [[ "${run_answer}" =~ ^[Yy]$ ]]; then
  (cd "${root_dir}" && "${cmake_cmd[@]}")
  (cd "${root_dir}" && cmake --build build --target simulator)
fi
