# Custom BB10/QFN20 layout X (development candidate)

This is a separate Bluejay v0.18.1 target for the reverse-engineered
EFM8BB10F8G ESC. It does not modify the original T layout. `CUSTOM_L_50`
is an output alias for internal layout `X_L_50_24` (24 kHz PWM, deadtime 50).

| Firmware phase | PCB phase | Low-side PWM | High-side COM | BEMF input |
| --- | --- | --- | --- | --- |
| A | C | P1.3 | P1.2 | P0.0 |
| B | B | P1.4 | P1.1 | P0.3 |
| C | A | P1.6 | P1.0 | P0.2 |

P0.1 is the comparator common reference. The DShot input is P0.5; this
Bluejay version does not support analog/servo PWM input. P1.5 and the other
unidentified pins are not assigned to PWM or other outputs. All six gate
control outputs are initialized low and use active-high control polarity.

For the extracted Silicon Labs Keil toolchain in the adjacent `KeilC51`
directory, run `tools/build_custom_l_50.sh`. The script uses the adjacent
Wine prefix when present. With a different installation, pass the path to
`C51/BIN` as its first argument and set `WINEPREFIX` to the prefix in which
the toolchain is licensed. A successful build checks the 8 KB image, BB10
tag, layout tag, and bootloader section.

The Keil evaluation linker has a 2 KB code limit. Silicon Labs provides a
free PK51 license for its 8-bit MCUs, but it must be activated for the
computer/Wine prefix before this target can be linked. See
<https://www.silabs.com/software-and-tools/keil-pk51> and
<https://www.silabs.com/documents/public/application-notes/an104.pdf>.

The licensed AX51/LX51/OHX51 toolchain built this target successfully. The
result is `build/hex/CUSTOM_L_50.hex`; SHA-256:
`bd25b1890cea0fbd4ef6fcaacd0c7329361ecc3b3514286249304271ddeb76a3`.
The linker reports 5120 bytes of code, zero errors, and one L30 DATA/IDATA
overlap warning. The unmodified T layout produces the same L30 warning.

This firmware is **not validated for flashing or motor use**. In particular,
the actual gate transition times, deadtime, boot/reset pin state, comparator
polarity, and BEMF zero-crossing order need bench verification with current
limiting and an oscilloscope. Bluejay uses complementary/damped PWM; do not
assume the chosen deadtime alone proves that shoot-through is impossible.
The default temperature protection is disabled because this board's sensor
calibration is unknown. No C2 flashing has been performed.

## CMPINV + deadtime 200 variant

`Layouts/Y.inc` is a separate copy of the custom pinout. It differs from X
only in `COMPARATOR_INVERT EQU 1`; on EFM8BB1, `Base.inc` implements this by
complementing the comparator result in software. It does not swap the BEMF
MUX inputs, phase mapping, six gate outputs, or throttle pin. X and its
previous `CUSTOM_L_50.hex` output remain unchanged.

Build the new variant with `bash tools/build_custom_l_200_cmpinv.sh`. The
output is `build/hex/CUSTOM_L_200_CMPINV_WITH_BOOTLOADER.hex`, with layout
tag `#Y_L_200#`. The BB10 bootloader is assembled into the same 8 KB HEX at
`0x1C00`; the build script compares the entire `0x1C00`–`0x1FFF` region with
the X build. The linked image uses 5121 bytes of code, including the
bootloader. The assembler reported
zero warnings/errors; the linker reported the same upstream L30 DATA/IDATA
overlap warning and zero errors. SHA-256:
`1be5b48fb003f10b1c8ba6d2dcdc8362e222e396046755bcca881d7f4a7e50c6`.

This build intentionally leaves the complementary PWM/Damped Light logic
unchanged. `Brake on Stop` is a separate setting. Bluejay v0.18.1's
`Braking Strength` limits the complementary PWM value, but setting it to zero
does not compile out the damp PWM channel or the `DEADTIME > 0` center-aligned
PWM path. The firmware subtracts `(DEADTIME + 1) / 2` from the BB1 damp PWM
register value, so 200 is a nominal timing parameter (about 4.1 µs using
the source's 20.4 ns/step comment), **not** an oscilloscope-verified
high/low MOSFET off-to-on gap. It should not be assumed that a UI setting
alone prevents shoot-through. Do not test with a propeller fitted; verify
all six gate waveforms and current-limited thermal behavior first. No C2
flashing has been performed for this variant.

## CMPINV + deadtime 90 + Braking Strength 0 default

`bash tools/build_custom_l_90_cmpinv_brake0.sh` generates
`build/hex/CUSTOM_L_90_CMPINV_BRAKE0_WITH_BOOTLOADER.hex`. The target is
`Y_L_90_24` with layout tag `#Y_L_90#`, BB10 MCU tag and the same bootloader
region. The script checks the 8 KB image and verifies that the emulated
EEPROM's `Braking Strength` default byte at `0x1A10` is `00`. SHA-256:
`b0554da9c8bc41ca56cc887a65a997f69e23224d9fb5c23562c93ac96081efca`.
Assembly completed with zero warnings/errors; the linker reported the same
single upstream L30 overlap warning and zero errors. Linked code is 5121
bytes including the bootloader.

Only the Y/BB10 custom target's default Braking Strength was changed; the
original X_L_50 image remains byte-identical. Defaults are applied when the
emulated EEPROM is initialized or reset. If flashing preserves an existing
EEPROM page with a valid `55 AA` signature, its old Braking Strength value
will override the compiled default. Read back the actual value after any
future flash; do not assume the firmware image changed the live setting.
The previous Y_L_200 image is superseded and is not recommended for this
BLHeliSuite, which rejected its three-digit deadtime tag. No C2 flashing
was performed for the Y_L_90 target.
