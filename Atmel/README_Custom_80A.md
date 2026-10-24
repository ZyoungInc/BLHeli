# Custom 80A (SuperSimple_40A-based, BLHeli 14.9)

This branch packages the earlier ATmega8L 80A ESC port without modifying the
upstream `SuperSimple_40A.inc` target. The branch name is a repository label;
it does **not** imply bidirectional DShot or bidirectional motor operation.

## Hardware mapping

| Function | ATmega8L pin |
| --- | --- |
| A low / EG2132 LIN | PD0 |
| A high / EG2132 HIN | PD1 |
| B low / EG2132 LIN | PD3 |
| B high / EG2132 HIN | PD4 |
| C low / EG2132 LIN | PD7 |
| C high / EG2132 HIN | PD5 |
| PWM receiver | PD2 / INT0 |
| A, B, C BEMF | ADC4, ADC3, ADC2 |
| Battery sense | ADC7; 47 kΩ / 4.7 kΩ (11:1) |

All six gate-driver inputs are configured as active high and initialized low.
The 16 MHz external clock is the board's existing hardware; this repository
does not set fuse bits. Operation of an ATmega8L at 16 MHz is outside its
nominal 8 MHz rating. `DAMPED_MODE_ENABLE` is 0, and the EEPROM default for
braking is off. Hardware dead-time and shoot-through safety require bench
verification before powering a motor.

The MAIN build uses the 11:1 ADC calibration (`ADC_LIMIT_L = 109`, derived
from 3.0 V per cell and a 2.56 V reference). The MULTI build does not provide
low-voltage cutoff; a flight controller or other external protection is
required. The ADC input reaches the 2.56 V reference at about 28.2 V, above
a fully charged 6S LiPo (25.2 V). This is a calculated limit, not a measured
voltage calibration.

The EEPROM layout tag remains `#SuperS_40A#` for compatibility with the stock
layout. BLHeliSuite recognition and parameter write-back are **not** claimed
to have been verified on this particular ESC.

## Build

On Ubuntu, install `avra` and `binutils` (for `objcopy`), then run:

```sh
bash Atmel/build_custom_80a.sh
```

The script leaves upstream assembly files untouched. It applies two AVRA
syntax-only workarounds to temporary copies of `BLHeli.asm` and
`BLHeliTxPgm.inc`, then builds MAIN and MULTI and combines MULTI with the
pinned 512-byte `BLHeli_m8_16_PD2.HEX` bootloader at byte address `0x1e00`.
It checks target symbols, EEPROM tags/defaults, code size, bootloader bytes,
and application bytes. Build output is under `Atmel/Output/Custom_80A/`.

Tracked releases:

- `Custom_80A_MAIN.hex` and `Custom_80A_MAIN.eep`
- `Custom_80A_MULTI.hex` and `Custom_80A_MULTI.eep`
- `Custom_80A_MULTI_WITH_BOOTLOADER.hex`

The combined HEX does **not** program EEPROM or fuse bits. In particular,
the bootloader requires a matching BOOTRST/BOOTSZ fuse configuration; do not
infer the correct fuse setting solely from the filename. There is no flashing
step in this build script. Backups, signature/fuse records, and temporary
build files are intentionally excluded from this branch.
