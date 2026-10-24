#!/usr/bin/env bash
set -Eeuo pipefail

src_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
out_dir="$src_dir/Output/Custom_80A"
boot_hex="$src_dir/BLHeli_m8_16_PD2.HEX"
boot_sha256='09bd88dee15478e03eea639176af648313d14c3a4584131a0f6d3b90b6280bf5'

for tool_name in avra objcopy sha256sum sed mktemp tee wc cmp awk xxd; do
    command -v "$tool_name" >/dev/null || {
        printf 'Missing build tool: %s\n' "$tool_name" >&2
        exit 1
    }
done

if [[ "$(sha256sum "$boot_hex" | sed 's/ .*//')" != "$boot_sha256" ]]; then
    printf 'Bootloader file changed; refusing to combine.\n' >&2
    exit 1
fi

mkdir -p -- "$out_dir"
build_tmp="$(mktemp -d)"
trap 'rm -rf -- "$build_tmp"' EXIT

# Keep upstream source unchanged. AVRA 1.4.2 needs two syntax-only fixes:
# a whitespace-free label and an explicit ZL register pair for ADIW.
sed -e 's/"SuperSimple_40A.inc"/"Custom_80A.inc"/g' \
    -e 's/^pwm_afet[[:space:]]*:/pwm_afet:/' \
    "$src_dir/BLHeli.asm" > "$build_tmp/BLHeli_Custom_80A.asm"
sed -e 's/adiw[[:space:]]*Z,[[:space:]]*1/adiw ZL, 1/g' \
    "$src_dir/BLHeliTxPgm.inc" > "$build_tmp/BLHeliTxPgm.inc"

check_symbol() {
    local map_file="$1" symbol_name="$2" expected_value="$3" actual_value
    actual_value="$(awk -v key="$symbol_name" '$1 == key {print $4}' "$map_file")"
    if [[ "$actual_value" != "$expected_value" ]]; then
        printf 'Unexpected %s in %s: got %s, expected %s\n' \
            "$symbol_name" "$map_file" "$actual_value" "$expected_value" >&2
        exit 1
    fi
}

for mode_name in MAIN MULTI; do
    firmware_hex="$out_dir/Custom_80A_${mode_name}.hex"
    eeprom_hex="$out_dir/Custom_80A_${mode_name}.eep"
    avra -D "SUPERSIMPLE_40A_${mode_name}" -I "$build_tmp" -I "$src_dir" -fI \
        -o "$firmware_hex" -e "$eeprom_hex" \
        -l "$out_dir/Custom_80A_${mode_name}.lst" \
        -m "$out_dir/Custom_80A_${mode_name}.map" \
        "$build_tmp/BLHeli_Custom_80A.asm" \
        2>&1 | tee "$out_dir/Custom_80A_${mode_name}.log"
    objcopy -I ihex -O binary "$firmware_hex" "$build_tmp/${mode_name}.bin"
    objcopy -I ihex -O binary --gap-fill 0xff \
        "$eeprom_hex" "$build_tmp/${mode_name}.eep.bin"
    firmware_bytes="$(wc -c < "$build_tmp/${mode_name}.bin")"
    if ((firmware_bytes >= 7680)); then
        printf '%s occupies %s bytes and overlaps the 0x1e00 bootloader.\n' \
            "$mode_name" "$firmware_bytes" >&2
        exit 1
    fi
    printf '%s application: %s bytes (bootloader starts at 0x1e00)\n' \
        "$mode_name" "$firmware_bytes"

    firmware_map="$out_dir/Custom_80A_${mode_name}.map"
    for symbol_spec in \
        'AnFET:0' 'ApFET:1' 'BnFET:3' 'BpFET:4' 'CnFET:7' 'CpFET:5' \
        'Rcp_In:2' 'Mux_A:4' 'Mux_B:3' 'Mux_C:2' 'Volt_Ip:7' \
        'INIT_PD:0' 'DIR_PD:187' 'DAMPED_MODE_ENABLE:0' 'ADC_LIMIT_L:109' \
        'Eep_ESC_Layout:64' 'Eep_ESC_MCU:80'; do
        check_symbol "$firmware_map" "${symbol_spec%%:*}" "${symbol_spec##*:}"
    done
    eeprom_binary="$build_tmp/${mode_name}.eep.bin"
    if [[ "$(xxd -p -s 0x40 -l 16 "$eeprom_binary")" != '235375706572535f3430412320202020' ||
          "$(xxd -p -s 0x50 -l 16 "$eeprom_binary")" != '23424c48454c4923416d384c23202020' ||
          "$(xxd -p -s 0x27 -l 1 "$eeprom_binary")" != '00' ]]; then
        printf '%s EEPROM tag or brake-off default mismatch.\n' "$mode_name" >&2
        exit 1
    fi
    expected_pwm='02'
    if [[ "$mode_name" == MULTI ]]; then expected_pwm='01'; fi
    if [[ "$(xxd -p -s 0x0a -l 1 "$eeprom_binary")" != "$expected_pwm" ]]; then
        printf '%s EEPROM PWM mode is not non-damped.\n' "$mode_name" >&2
        exit 1
    fi
done

# The stock bootloader HEX has a 0x0000 FFFF placeholder. Keep the application
# reset vector instead, and append only the pinned bootloader records at 0x1e00.
combined_hex="$out_dir/Custom_80A_MULTI_WITH_BOOTLOADER.hex"
sed '$d' "$out_dir/Custom_80A_MULTI.hex" > "$combined_hex"
sed -n '3,34p' "$boot_hex" >> "$combined_hex"
printf ':00000001FF\n' >> "$combined_hex"
objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x2000 \
    "$combined_hex" "$out_dir/Custom_80A_MULTI_WITH_BOOTLOADER.bin"
if [[ "$(wc -c < "$out_dir/Custom_80A_MULTI_WITH_BOOTLOADER.bin")" != 8192 ]]; then
    printf 'Combined flash is not exactly 8192 bytes.\n' >&2
    exit 1
fi
cmp -n "$(wc -c < "$build_tmp/MULTI.bin")" \
    "$build_tmp/MULTI.bin" "$out_dir/Custom_80A_MULTI_WITH_BOOTLOADER.bin"
objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x2000 \
    "$boot_hex" "$build_tmp/bootloader.bin"
cmp -i 7680:7680 -n 512 "$build_tmp/bootloader.bin" \
    "$out_dir/Custom_80A_MULTI_WITH_BOOTLOADER.bin"
if [[ "$(xxd -p -s 0x1e00 -l 3 "$out_dir/Custom_80A_MULTI_WITH_BOOTLOADER.bin")" != 'f894aa' ]]; then
    printf 'Bootloader entry signature is missing.\n' >&2
    exit 1
fi

printf 'Built: %s\n' "$out_dir/Custom_80A_MAIN.hex"
printf 'Built: %s\n' "$out_dir/Custom_80A_MULTI.hex"
printf 'Built: %s\n' "$combined_hex"
