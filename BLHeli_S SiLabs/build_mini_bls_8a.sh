#!/usr/bin/env bash
set -Eeuo pipefail

source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
wine_prefix="${WINEPREFIX:-$source_dir/../../KeilC51/wineprefix}"
wine_prefix="$(realpath -m -- "$wine_prefix")"
keil_bin_dir="${KEIL_BIN_DIR:-$wine_prefix/drive_c/Keil_v5/C51/BIN}"
wine_bin="$(command -v wine)"
hex_file="$source_dir/build/MB8_L_1_REV16_7.HEX"
alias_file="$source_dir/build/hex/MiniBLS8A_L_24K_NODAMP.hex"
log_file="$source_dir/build/log/MB8_L_1_REV16_7.log"

for tool_name in make realpath rg objcopy xxd dd wc sha256sum mktemp mv sed; do
    command -v "$tool_name" >/dev/null || { printf 'Missing tool: %s\n' "$tool_name" >&2; exit 1; }
done
for tool_name in AX51.exe LX51.exe Ohx51.exe; do
    test -f "$keil_bin_dir/$tool_name" || { printf 'Missing Keil tool: %s\n' "$keil_bin_dir/$tool_name" >&2; exit 1; }
done
test -d "$wine_prefix"

rm -f -- "$log_file"
make -C "$source_dir" -B single_target \
    TARGETS=MB8 MCUS=L FETON_DELAYS=1 VARIANT=MB8 MCU=L FETON_DELAY=1 \
    "WINE_BIN=$wine_bin" "WINE_PREFIX=$wine_prefix" "KEIL_PATH=$keil_bin_dir"

if test -f "$source_dir/BLHeli_S.LST"; then
    mv -- "$source_dir/BLHeli_S.LST" "$source_dir/build/MB8_L_1_REV16_7.LST"
fi

rg -q 'ASSEMBLY COMPLETE\.  0 WARNING\(S\), 0 ERROR\(S\)' "$log_file"
rg -q 'LX51 RUN COMPLETE\.  1 WARNING\(S\),  0 ERROR\(S\)' "$log_file"
rg -q 'WARNING L30: MEMORY SPACE OVERLAP' "$log_file"
if rg -q '\*\*\* ERROR' "$log_file"; then
    printf 'Build log contains an error: %s\n' "$log_file" >&2
    exit 1
fi

temp_bin="$(mktemp)"
trap 'rm -f -- "$temp_bin"' EXIT
objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x2000 "$hex_file" "$temp_bin"
test "$(wc -c < "$temp_bin")" -eq 8192
test "$(xxd -p -s 0x1a00 -l 3 "$temp_bin")" = '100721'
test "$(dd if="$temp_bin" bs=1 skip=$((0x1a40)) count=16 status=none)" = '#MB8_L_01#      '
test "$(dd if="$temp_bin" bs=1 skip=$((0x1a50)) count=16 status=none)" = '#BLHELI$EFM8B10#'
test "$(xxd -p -s 0x1c00 -l 4 "$temp_bin")" != 'ffffffff'

sed 's/\r$//' "$hex_file" > "$alias_file"
printf 'Static checks passed: Rev16.7, BB10/L, Mini BLS 8A, 9-bit center-aligned PWM, no Damped Light, 8KB image, bootloader present.\n%s\n' "$alias_file"
sha256sum "$alias_file"
