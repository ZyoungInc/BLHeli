# Mini BLS 8A experimental BLHeli_S 16.7 target

This branch adds an independent `MB8` target for a traced EFM8BB10F8G
QFN20 ESC. It is derived from the BLHeli_S `T` layout. The stock layouts
remain unchanged. The branch does not claim that a physical ESC has been
flashed or that gate timing has been measured.

The mapping assumes the three P-MOS high sides are driven through three
NPN transistors and the three N-MOS low sides are driven directly by MCU
GPIO. Both MCU-side control signals are assumed active high. This matches
the previously traced PCB, but must be checked again if the Mini BLS 8A
board is a different revision. No output should be connected to P1.5.

| Firmware phase | PCB phase | Low-side PWM | High-side GPIO | BEMF |
| --- | --- | --- | --- | --- |
| A | C | P1.3 | P1.2 | P0.0 |
| B | B | P1.4 | P1.1 | P0.3 |
| C | A | P1.6 | P1.0 | P0.2 |

P0.1 is the comparator common reference and P0.5 is the input/bootloader
signal. The comparator output is inverted for this low-side-PWM mapping.
The 9-bit center-aligned PCA PWM runs at about 23.9 kHz with a nominal
24.5 MHz system clock. Only CEX0 is routed, to the selected low-side pin;
CEX1 is disabled and high sides remain commutation GPIOs. Thus there is no
complementary PWM or Damped Light during motor running. The stock
`Brake on Stop` default is zero, but retained EEPROM settings may enable
that separate feature. `FETON_DELAY=1` is only a selector for the existing
nonzero-driver code path; it does **not** define a measured deadtime.

Build with an activated Keil C51 toolchain and Wine:

```bash
cd 'BLHeli_S SiLabs'
bash ./build_mini_bls_8a.sh
```

Set `WINEPREFIX` and `KEIL_BIN_DIR` if the tools are installed elsewhere.
The reproducible output is `build/hex/MiniBLS8A_L_24K_NODAMP.hex`.
A normalized Intel HEX copy is committed at
`firmware/MiniBLS8A_L_24K_NODAMP.hex`, SHA-256
`a7e93fa1d8298c7ea2149d35ddbe3e9c303f8e19a00a2233efde802ab75addbe`.
The image contains BLHeli_S 16.7 default EEPROM data and its bootloader;
the layout tag is `#MB8_L_01#`. The code occupies 5789 bytes. Assembly
reported zero warnings/errors; the linker reported the same upstream
`L30 MEMORY SPACE OVERLAP` warning and zero errors. Aside from the layout
tag, the image is byte-identical to the previously built XN non-damped
candidate. Custom BLHeliSuite recognition and parameter writes have not
been tested.

Safety: this firmware does not add a fixed commutation blanking interval.
Normal six-step operation floats each phase for one step between its high-
and low-side drive states, but the old high-side GPIO is cleared only about
two CPU clocks before the new high-side GPIO is set. That instruction
spacing is not a measured P-MOS off-to-on interval. PWM routing between
low sides also has no separately verified all-off interval. Before any
powered motor test, remove the propeller and check all six gate waveforms,
phase current and switching temperature with suitable equipment and
current limiting. Do not assume disabling Damped Light alone makes this
power stage safe.
