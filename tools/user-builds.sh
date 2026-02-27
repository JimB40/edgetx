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
root_cmake="${root_dir}/CMakeLists.txt"

radios_config="${script_dir}/radio-targets.tsv"
use_fzf=1
preselect=""
saved_idx=""
config_file="${root_dir}/.user-builds-config"
user_builds_dir="${root_dir}/user-builds"

fw_major="$(awk -F'"' '/set\(VERSION_MAJOR/{print $2; exit}' "${root_cmake}")"
fw_minor="$(awk -F'"' '/set\(VERSION_MINOR/{print $2; exit}' "${root_cmake}")"
fw_revision="$(awk -F'"' '/set\(VERSION_REVISION/{print $2; exit}' "${root_cmake}")"
fw_semver="${fw_major}.${fw_minor}.${fw_revision}"
if [[ -n "${EDGETX_VERSION_TAG:-}" ]]; then
  fw_version="${EDGETX_VERSION_TAG}"
else
  fw_prefix="${EDGETX_VERSION_PREFIX:-pre}"
  fw_suffix="${EDGETX_VERSION_SUFFIX:-selfbuild}"
  fw_version="${fw_prefix}-${fw_semver}-${fw_suffix}"
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --txt)
      use_fzf=0
      ;;
    --select=*)
      preselect="${1#*=}"
      ;;
    --select)
      shift
      preselect="${1:-}"
      ;;
  esac
  shift
done

if [[ ! -f "${radios_config}" ]]; then
  echo "Missing radios config: ${radios_config}"
  echo "Generate it with:"
  echo "  tools/radio-targets.py --export-tsv tools/radio-targets.tsv"
  exit 1
fi

if [[ -d "${root_dir}/build" ]]; then
  if [[ "${use_fzf}" -eq 1 && -t 1 ]] && command -v fzf >/dev/null 2>&1; then
    clean_selection="$(printf "No (default)\nYes (remove build folder)\n" | fzf --height=20% --reverse --prompt="Clean build? This will remove build folder! " --no-multi || true)"
    if [[ -z "${clean_selection}" ]]; then
      return 0 2>/dev/null || exit 0
    fi
    if [[ "${clean_selection}" == "Yes (remove build folder)" ]]; then
      rm -rf "${root_dir}/build"
      echo "Removed ${root_dir}/build"
    fi
  else
    printf "Clean build? This will remove build folder! [y/N]: "
    read -r clean_answer
    if [[ "${clean_answer}" =~ ^[Yy]$ ]]; then
      rm -rf "${root_dir}/build"
      echo "Removed ${root_dir}/build"
    fi
  fi
fi

if [[ "${use_fzf}" -eq 1 && -t 1 ]] && command -v fzf >/dev/null 2>&1; then
  build_selection="$(printf "2. simulator\n1. firmware\n" | fzf --height=30% --reverse --prompt="Select build type: " --no-multi || true)"
  if [[ -z "${build_selection}" ]]; then
    return 0 2>/dev/null || exit 0
  fi
  build_choice="$(echo "${build_selection}" | awk '{print $1}' | sed 's/\.//')"
else
  echo "Build type:"
  echo "  1) firmware"
  echo "  2) simulator"
  printf "Select build type [2]: "
  read -r build_choice
  build_choice="${build_choice:-2}"
fi

case "${build_choice}" in
  1)
    build_target="firmware"
    build_label="firmware"
    ;;
  2)
    build_target="libsimulator"
    build_label="simulator"
    ;;
  *)
    echo "Invalid build type selection."
    exit 1
    ;;
esac

# Firmware builds require the ARM cross-compiler toolchain.
if [[ "${build_target}" == "firmware" ]]; then
  if ! command -v arm-none-eabi-gcc >/dev/null 2>&1 || ! command -v arm-none-eabi-g++ >/dev/null 2>&1; then
    echo "Missing required ARM toolchain for firmware builds:"
    echo "  - arm-none-eabi-gcc"
    echo "  - arm-none-eabi-g++"
    echo "Install the GNU Arm Embedded Toolchain and ensure those binaries are in PATH."
    exit 1
  fi

  # EdgeTX firmware build is pinned to Arm GNU Toolchain 14.2.rel1 (compiler version 14.2.1).
  required_arm_gcc_version="14.2.1"
  arm_gpp_version="$(arm-none-eabi-g++ -dumpfullversion -dumpversion 2>/dev/null || true)"
  if [[ -z "${arm_gpp_version}" ]]; then
    arm_gpp_version="$(arm-none-eabi-g++ --version 2>/dev/null | awk 'NR==1 {print $NF}')"
  fi
  if [[ "${arm_gpp_version}" != "${required_arm_gcc_version}" ]]; then
    echo "Unsupported ARM toolchain version: ${arm_gpp_version:-unknown}"
    echo "Required version: ${required_arm_gcc_version} (Arm GNU Toolchain 14.2.rel1)"
    echo "Install/update arm-none-eabi toolchain and ensure the correct version is first in PATH."
    exit 1
  fi
fi

if [[ -z "${preselect}" ]]; then
  if [[ -f "${config_file}" ]]; then
    saved_idx="$(tr -d '[:space:]' < "${config_file}")"
  fi
  if [[ "${saved_idx}" =~ ^[0-9]+$ ]]; then
    preselect="${saved_idx}"
  fi
fi

if [[ -n "${preselect}" && "${preselect}" =~ ^[0-9]+$ && "${saved_idx}" != "${preselect}" ]]; then
  idx="${preselect}"
elif [[ "${use_fzf}" -eq 1 && -t 1 ]] && command -v fzf >/dev/null 2>&1; then
  radio_lines="$(awk -F '\t' 'NF >= 3 {printf "%s. %s\n", $1, $2}' "${radios_config}")"
  if [[ -n "${preselect}" && "${preselect}" =~ ^[0-9]+$ ]]; then
    radio_lines="$(printf '%s\n' "${radio_lines}" | awk -v n="${preselect}." '
      $1 == n { selected = $0; next }
      { others[++count] = $0 }
      END {
        if (selected != "") print selected
        for (i = 1; i <= count; i++) print others[i]
      }
    ')"
  else
    :
  fi
  selection="$(printf '%s\n' "${radio_lines}" | fzf --height=40% --reverse --prompt="Select radio: " --no-multi || true)"
  if [[ -z "${selection}" ]]; then
    return 0 2>/dev/null || exit 0
  fi
  idx="$(echo "${selection}" | awk '{print $1}' | sed 's/\.//')"
else
  echo "Available radios:"
  awk -F '\t' 'NF >= 3 {printf "%3d. %s\n", $1, $2}' "${radios_config}"
  echo
  if [[ -n "${preselect}" ]]; then
    printf "Select radio [%s]: " "${preselect}"
  else
    printf "Select radio: "
  fi
  read -r idx
  idx="${idx:-${preselect}}"
fi

if [[ -z "${idx}" ]]; then
  return 0 2>/dev/null || exit 0
fi

if [[ ! "${idx}" =~ ^[0-9]+$ ]]; then
  echo "Invalid selection."
  exit 1
fi

printf "%s\n" "${idx}" > "${config_file}"

selected_row="$(awk -F '\t' -v n="${idx}" '$1 == n {print $3 "\t" $4; exit}' "${radios_config}")"
if [[ -z "${selected_row}" ]]; then
  echo "Failed to resolve selection from ${radios_config}."
  exit 1
fi

pcb="$(printf '%s\n' "${selected_row}" | awk -F '\t' '{print $1}')"
pcbrev="$(printf '%s\n' "${selected_row}" | awk -F '\t' '{print $2}')"

export EDGETX_PCB="${pcb}"
export EDGETX_PCBREV="${pcbrev:-}"

echo "Building ${build_label}"
echo "Selected: PCB=${EDGETX_PCB} PCBREV=${EDGETX_PCBREV}"
echo
echo "Run:"
if [[ -n "${EDGETX_PCBREV}" ]]; then
  cmake_cmd=(cmake -S . -B build -DPCB="${EDGETX_PCB}" -DPCBREV="${EDGETX_PCBREV}")
else
  cmake_cmd=(cmake -S . -B build -DPCB="${EDGETX_PCB}")
fi
printf "  %q " "${cmake_cmd[@]}"
echo
echo "  cmake --build build --target ${build_target}"

model_id="${EDGETX_PCB}"
if [[ -n "${EDGETX_PCBREV}" ]]; then
  model_id="${EDGETX_PCBREV}"
fi
confirm_target_label="${build_label}"
if [[ "${build_target}" == "libsimulator" ]]; then
  confirm_target_label="simulator library"
fi
confirm_prompt="Configure and build ${confirm_target_label} for ${model_id}? "

if [[ "${use_fzf}" -eq 1 && -t 1 ]] && command -v fzf >/dev/null 2>&1; then
  run_selection="$(printf "Yes (default)\nNo\n" | fzf --height=20% --reverse --prompt="${confirm_prompt}" --no-multi || true)"
  if [[ -z "${run_selection}" ]]; then
    return 0 2>/dev/null || exit 0
  fi
  run_answer="y"
  if [[ "${run_selection}" == "Yes" || "${run_selection}" == "Yes (default)" ]]; then
    run_answer="y"
  elif [[ "${run_selection}" == "No" || "${run_selection}" == "No (default)" ]]; then
    run_answer="n"
  fi
else
  printf "%s[Y/n]: " "${confirm_prompt}"
  read -r run_answer
  if [[ -z "${run_answer}" ]]; then
    run_answer="y"
  fi
fi

if [[ "${run_answer}" =~ ^[Yy]$ ]]; then
  if [[ "${build_label}" == "simulator" ]]; then
    app_macos_dir="${root_dir}/build/native/simulator.app/Contents/MacOS"
    existing_app_bin=""
    if [[ -d "${app_macos_dir}" ]]; then
      existing_app_bin="$(find "${app_macos_dir}" -maxdepth 1 -type f ! -name '*.dylib' | head -n 1 || true)"
    fi

    if [[ -z "${existing_app_bin}" ]]; then
      # No existing simulator executable yet; build it without asking.
      build_target="simulator"
    else
      rebuild_simulator_exe="n"
      if [[ "${use_fzf}" -eq 1 && -t 1 ]] && command -v fzf >/dev/null 2>&1; then
        rebuild_selection="$(printf "No (default)\nYes\n" | fzf --height=20% --reverse --prompt="Rebuild simulator executable? " --no-multi || true)"
        if [[ -z "${rebuild_selection}" ]]; then
          return 0 2>/dev/null || exit 0
        fi
        if [[ "${rebuild_selection}" == "Yes" ]]; then
          rebuild_simulator_exe="y"
        fi
      else
        printf "Rebuild simulator executable? [y/N]: "
        read -r rebuild_answer
        if [[ "${rebuild_answer}" =~ ^[Yy]$ ]]; then
          rebuild_simulator_exe="y"
        fi
      fi

      if [[ "${rebuild_simulator_exe}" == "y" ]]; then
        build_target="simulator"
      else
        build_target="libsimulator"
      fi
    fi
  fi

  (cd "${root_dir}" && "${cmake_cmd[@]}")
  (cd "${root_dir}" && cmake --build build --target "${build_target}")

  target_name="${EDGETX_PCB}"
  if [[ -n "${EDGETX_PCBREV}" ]]; then
    target_name="${target_name}-${EDGETX_PCBREV}"
  fi
  pcb_slug="$(printf '%s' "${EDGETX_PCB}" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9._-' '-')"
  pcbrev_slug="$(printf '%s' "${EDGETX_PCBREV}" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9._-' '-')"
  target_slug="$(printf '%s' "${target_name}" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9._-' '-')"
  version_slug="$(printf '%s' "${fw_version}" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9._-' '-')"
  stamp="$(date +%Y%m%d-%H%M%S)"
  mkdir -p "${user_builds_dir}"
  if [[ "${build_target}" == "firmware" ]]; then
    out_dir="${user_builds_dir}"
  else
    out_dir="${user_builds_dir}/${build_label}-${target_slug}-${stamp}"
  fi
  copied=0

  if [[ "${build_target}" == "firmware" ]]; then
    fw_radio_slug="${target_slug}"
    if [[ -n "${pcbrev_slug}" ]]; then
      fw_radio_slug="${pcbrev_slug}"
    fi
    fw_base_name="${fw_radio_slug}-${version_slug}"
    if [[ -f "${root_dir}/build/arm-none-eabi/firmware.bin" ]]; then
      cp "${root_dir}/build/arm-none-eabi/firmware.bin" "${out_dir}/${fw_base_name}.bin"
      copied=1
    fi
    if [[ -f "${root_dir}/build/arm-none-eabi/firmware.uf2" ]]; then
      cp "${root_dir}/build/arm-none-eabi/firmware.uf2" "${out_dir}/${fw_base_name}.uf2"
      copied=1
    fi
  else
    shopt -s nullglob
    all_sim_libs=( "${root_dir}"/build/native/libedgetx-*-simulator.* "${root_dir}"/build/native/plugins/libedgetx-*-simulator.* )
    shopt -u nullglob

    declare -a selected_sim_libs=()
    declare -a lib_name_keys=()
    declare -a key_hits=()
    if [[ -n "${pcbrev_slug}" ]]; then
      lib_name_keys+=( "${pcbrev_slug}" )
    fi
    lib_name_keys+=( "${target_slug}" "${pcb_slug}" )

    for key in "${lib_name_keys[@]}"; do
      [[ -z "${key}" ]] && continue
      shopt -s nullglob
      key_hits=( "${root_dir}"/build/native/libedgetx-"${key}"-simulator.* "${root_dir}"/build/native/plugins/libedgetx-"${key}"-simulator.* )
      shopt -u nullglob
      if [[ ${key_hits[0]+set} ]]; then
        for lib in "${key_hits[@]}"; do
          [[ -z "${lib}" ]] && continue
          skip=0
          if [[ ${selected_sim_libs[0]+set} ]]; then
            for existing in "${selected_sim_libs[@]}"; do
              if [[ "${existing}" == "${lib}" ]]; then
                skip=1
                break
              fi
            done
          fi
          if [[ "${skip}" -eq 0 ]]; then
            selected_sim_libs+=( "${lib}" )
          fi
        done
      fi
    done

    if [[ "${#selected_sim_libs[@]}" -eq 0 && "${#all_sim_libs[@]}" -gt 0 ]]; then
      # Fallback: use the newest available simulator library when naming mismatch occurs.
      newest_lib="$(ls -t "${all_sim_libs[@]}" 2>/dev/null | head -n 1 || true)"
      if [[ -n "${newest_lib}" ]]; then
        selected_sim_libs+=( "${newest_lib}" )
      fi
    fi

    app_macos_dir="${root_dir}/build/native/simulator.app/Contents/MacOS"
    if [[ ! -d "${app_macos_dir}" ]]; then
      echo "simulator.app not found; building simulator executable once..."
      (cd "${root_dir}" && cmake --build build --target simulator)
    fi
    app_bin="$(find "${app_macos_dir}" -maxdepth 1 -type f ! -name '*.dylib' | head -n 1 || true)"
    if [[ -z "${app_bin}" ]]; then
      echo "simulator.app executable missing; building simulator executable once..."
      (cd "${root_dir}" && cmake --build build --target simulator)
      app_bin="$(find "${app_macos_dir}" -maxdepth 1 -type f ! -name '*.dylib' | head -n 1 || true)"
      if [[ -z "${app_bin}" ]]; then
        echo "simulator.app is missing its executable in ${app_macos_dir}"
        exit 1
      fi
    fi

    packed=0
    up_to_date=0
    if [[ ${selected_sim_libs[0]+set} ]]; then
      for lib in "${selected_sim_libs[@]}"; do
        [[ -z "${lib}" ]] && continue
        lib_name="$(basename "${lib}")"
        app_lib="${app_macos_dir}/${lib_name}"
        if [[ -f "${app_lib}" ]]; then
          if [[ "${lib}" -nt "${app_lib}" ]]; then
            cp "${lib}" "${app_macos_dir}/"
            packed=$((packed + 1))
          else
            up_to_date=$((up_to_date + 1))
          fi
        else
          cp "${lib}" "${app_macos_dir}/"
          packed=$((packed + 1))
        fi
      done
    fi
    if [[ "${packed}" -gt 0 || "${up_to_date}" -gt 0 ]]; then
      simulator_bundle="${user_builds_dir}/simulator-${fw_semver}.app"
      rm -rf "${simulator_bundle}"
      cp -R "${root_dir}/build/native/simulator.app" "${simulator_bundle}"
      if [[ "${packed}" -gt 0 ]]; then
        echo "Packed/updated ${packed} selected simulator libraries into: ${app_macos_dir}"
      else
        echo "Selected simulator library already up to date in simulator.app"
      fi
      echo "Simulator app written to: ${simulator_bundle}"
      copied=1
    fi
  fi

  if [[ "${copied}" -eq 1 ]]; then
    if [[ "${build_target}" == "firmware" ]]; then
      echo "Artifacts copied to: ${out_dir}"
    fi
  else
    echo "Build succeeded, but no artifacts were found to copy."
  fi
fi
