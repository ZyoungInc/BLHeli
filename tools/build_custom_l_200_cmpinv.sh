#!/usr/bin/env bash
set -Eeuo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
local_keil_dir="$project_dir/../KeilC51"
if [[ -d "$local_keil_dir/wineprefix" && -z "${WINEPREFIX:-}" ]]; then
    export WINEPREFIX="$local_keil_dir/wineprefix"
fi
installed_keil_bin_dir="$local_keil_dir/wineprefix/drive_c/Keil_v5/C51/BIN"
if [[ -d "$installed_keil_bin_dir" ]]; then
    default_keil_bin_dir="$installed_keil_bin_dir"
else
    default_keil_bin_dir="$local_keil_dir/C51/BIN"
fi
keil_bin_dir="${1:-${KEIL_PATH:-$default_keil_bin_dir}}"
hex_file="$project_dir/build/hex/CUSTOM_L_200_CMPINV_WITH_BOOTLOADER.hex"
baseline_hex="$project_dir/build/hex/CUSTOM_L_50.hex"

for tool_name in make wine objcopy xxd sha256sum mktemp wc dd cmp; do
    command -v "$tool_name" >/dev/null || {
        printf 'Missing build dependency: %s\n' "$tool_name" >&2
        exit 1
    }
done

for tool_name in AX51.exe LX51.exe Ohx51.exe; do
    test -f "$keil_bin_dir/$tool_name" || {
        printf 'Missing Keil tool: %s\n' "$keil_bin_dir/$tool_name" >&2
        exit 1
    }
done

make -C "$project_dir" custom_l_50 custom_l_200_cmpinv "KEIL_PATH=$keil_bin_dir"

temp_bin="$(mktemp)"
baseline_bin="$(mktemp)"
trap 'rm -f -- "$temp_bin" "$baseline_bin"' EXIT
objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x2000 "$hex_file" "$temp_bin"
objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x2000 "$baseline_hex" "$baseline_bin"
test "$(wc -c < "$temp_bin")" -eq 8192 || {
    printf 'HEX is not an 8KB BB10 image.\n' >&2
    exit 1
}

layout_tag="$(dd if="$temp_bin" bs=1 skip=$((0x1a40)) count=16 status=none)"
mcu_tag="$(dd if="$temp_bin" bs=1 skip=$((0x1a50)) count=16 status=none)"
test "$layout_tag" = '#Y_L_200#       ' || {
    printf 'Wrong layout tag: %s\n' "$layout_tag" >&2
    exit 1
}
test "$mcu_tag" = '#BLHELI$EFM8B10#' || {
    printf 'Wrong BB10 MCU tag: %s\n' "$mcu_tag" >&2
    exit 1
}
test "$(xxd -p -s 0x1c00 -l 4 "$temp_bin")" != 'ffffffff' || {
    printf 'Bootloader segment at 0x1C00 is blank.\n' >&2
    exit 1
}
cmp -i $((0x1c00)):$((0x1c00)) -n $((0x400)) "$baseline_bin" "$temp_bin" || {
    printf 'Bootloader region differs from the original X_L_50 build.\n' >&2
    exit 1
}

printf 'Verified 8KB BB10 HEX, Y_L_200/MCU tags, and identical 0x1C00-0x1FFF bootloader region:\n%s\n' "$hex_file"
sha256sum "$hex_file"
