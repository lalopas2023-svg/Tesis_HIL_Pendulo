# firmware/

Código del DSP TMS320F28379D (LAUNCHXL-F28379D), núcleo CPU1.

- **`fuentes/control_hil_v3.c`** — firmware vigente: planta + PID en la ISR de Timer0 (10 µs), lectura de ADC,
  DAC, SCI-A y telemetría `pos_mrad,vel_mrad_s,u_milli` cada 10 ms. Opciones al inicio del archivo:
  `USE_TRIGGER` (espera la 'S' de Python), `USE_ADCSETMODE`, `DIAG_TELE_ERR`, `ADC_CAL_M`/`ADC_CAL_B`.
- **`fuentes/diagnostico/diag_adc_dac.c`** — firmware de barrido DAC→ADC. **Ojo:** su divisor de baud
  (`BRR=26`) corresponde a SYSCLK = 100 MHz; con 200 MHz hay que cambiarlo a 53.
- **`Proyecto_Pendulo_CPU1/`** — proyecto de CCS (archivos `.project`, `.cproject`, linker, `F2837xD_CpuTimers.c`).
  **No contiene el `main`**: el proyecto original usaba el `main` de un ejemplo del SDK; copia
  `fuentes/control_hil_v3.c` al proyecto (y excluye de la compilación cualquier otro archivo con `main`).

## Requisitos de compilación que importan

- Símbolo predefinido **`_LAUNCHXL_F28379D`** a nivel *proyecto* (Properties → Build → C2000 Compiler → Predefined
  Symbols) y luego *Clean*. Sin él el reloj queda en 100 MHz y el baud real cambia; se verificó empíricamente
  (`sysclk_mhz_reg` 100 → 200 MHz).
- Incluir `F2837xD_Adc.c` (para `AdcSetMode`) además de los archivos habituales de device support del SDK.
- Variables útiles en la ventana *Watch*: `sysclk_mhz_reg`, `lspclk_mhz_reg`, `tx_frames`, `isr_max_cycles`,
  `isr_overruns`, `theta_mean_acc`, `offset_lsb`, `b_sugerido`.

## Calibración

`ADC_CAL_B` compensa el offset del lazo DAC→ADC (≈ 6.1 LSB con 200 MHz). El offset depende del reloj y de la
configuración del ADC: **repetir la calibración si cambia alguna** (ver bitácora, 2026-10-31/11-01).
