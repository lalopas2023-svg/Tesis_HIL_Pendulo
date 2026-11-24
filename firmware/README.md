# firmware/

Código del DSP TMS320F28379D (LAUNCHXL-F28379D), núcleo CPU1.

## Fuentes (`fuentes/`)

Cada carpeta es una variante del mismo controlador (planta + PID en la ISR de Timer0 a 100 kHz, lectura del ADC, DAC y
telemetría por SCI-A cada 10 ms). Van de la más antigua a la más reciente:

| Archivo | Qué es | Estado |
|---|---|---|
| `sin_perturbacion/control_hil_sin_perturbacion.c` | **Base probada.** Sin perturbación. Es idéntico al `main` del proyecto de CCS (mismo SHA-256). Trama `pos_mrad,vel_mrad_s,u_milli`. | Vigente |
| `con_perturbacion/control_perturbacion.c` | La base más una perturbación `d(t)` por potenciómetro (ADCINA2) o escalón. Trama de 4 columnas (`...,pert_milli`). Se usa con `middleware/hil_coppelia_perturbacion.py`. Es el mismo archivo que el `main` de `Proyecto_HIL_Perturbacion/`. | Probada |
| `autocal/control_hil_autocal.c` | La base más **autocalibración v2**: al arrancar mide el offset del lazo DAC→ADC y lo usa como `B`. | Validada (18 corridas, 3 arranques, ±0.4 mrad). Espera la decisión del director para entrar al firmware definitivo |
| `f5/control_hil_f5.c` y `f5_prueba.py` | La autocalibración más el comando `D` por UART con la ISR activa. `f5_prueba.py` es el script que lo prueba (4 fases, criterios en su cabecera). | Preparada; sin resultados en la tarjeta |
| `diagnostico/diag_adc_dac.c` | Barrido DAC→ADC sin controlador ni planta (mide el offset del lazo). Lo analiza `analisis/analisis_barrido.m`. | Herramienta. **Ojo:** su divisor de baud (`BRR = 26`) solo vale a SYSCLK = 100 MHz; con 200 MHz hay que poner 53 |

### Opciones al inicio de los archivos
`USE_TRIGGER` (espera la `S` de Python para empezar), `USE_ADCSETMODE`, `ADC_CAL_M`/`ADC_CAL_B` (calibración fija) y,
en `autocal` y `f5`, `USE_AUTOCAL`.

### Autocalibración v2 (`autocal/` y `f5/`)
Con el DAC en el código 2048 toma 5000 lecturas del ADC, descarta las que están a más de 12 LSB de `2048 + 6.1`
(`CAL_OUT_LSB`) y promedia el resto: ese promedio es `B`. Lo acepta si `|B − 6.1| ≤ 10` y se descartaron a lo más 250
lecturas (`CAL_OUT_MAX`); si no, usa `ADC_CAL_B = 6.1` y deja `cal_ok = 0`. Variables para el *Watch*: `b_auto`,
`adc_cal_b`, `cal_ok`, `cal_min`, `cal_max`, `cal_out`, `cal_runs`. Presupuesto de la ISR: 2000 ciclos; máximo medido
1385 a 1578, sin *overruns*. La primera versión (criterio de rango `max − min ≤ 24`) se rechazó porque las lecturas atípicas
la dejaban sin calibrar; la historia está en la bitácora (adenda del 2026-11-19 (9)).

### Comando `D` (`f5/`)
Entre tramas de telemetría, el DSP atiende `D` y responde
`D,isr_max,overruns,adc_to,rx_err,tx_frames,cal_ok,B_milli,cmd_diag,cmd_unk`. Ignora `S`, CR y LF; cualquier otro byte
suma a `cmd_unk`. Permite leer los contadores sin el IDE (la operación `memory` de DSLite ejecuta el GEL `OnTargetConnect` al
conectar, que inicializa la RAM y reinicia, y no sirve para eso; ver `docs/gui/Resultados_F2_2026-11-17.md`).

## Proyecto de CCS (`Proyecto_Pendulo_CPU1/`)

Contiene solo lo propio del proyecto: `.project`, `.cproject`, `.ccsproject`, el linker de flash
(`2837x_FLASH_lnk_cpu1.cmd`), `F2837xD_Adc.c`, `F2837xD_CpuTimers.c` y `targetConfigs/TMS320F28379D.ccxml` (sonda **XDS100v2**).
Las configuraciones son `CPU1_RAM_DEBUG`, `CPU1_FLASH_DEBUG` y `CPU1_FLASH_STANDALONE`; hasta ahora solo se ha compilado
`CPU1_RAM_DEBUG`.

- **No contiene el `main`.** El `.project` enlaza `ipc_gpio_toggle_cpu01.c` del ejemplo de C2000Ware 5.2.0.00 y ese archivo
  *es* el firmware. Los archivos de soporte del SDK (`F2837xD_*.c`) también están enlazados, con rutas que parten de la
  variable `COM_TI_C2000WARE_INSTALL_DIR`. En el laboratorio hay dos versiones de C2000Ware; el proyecto exige la 5.2.0.00.
- Cambiar el archivo enlazado modifica el ejemplo del SDK. Para probar otra fuente de `fuentes/` se trabajó en una copia
  aislada (`firmware/_factibilidad/`, ignorada por git) en la que se quitó el enlace del `main`; el procedimiento está en
  [`docs/gui/registro_preparacion_2026-11-12.md`](../docs/gui/registro_preparacion_2026-11-12.md).
- Los archivos propios del IDE (Theia, `.clangd`, `.settings/`) y la configuración de otro dispositivo (F28377D) se
  retiraron del repositorio el 2026-11-24: se regeneran o no se usan (siguen en `.gitignore`).

## Proyecto con perturbación (`Proyecto_HIL_Perturbacion/`)

Proyecto de CCS de la variante con perturbación. A diferencia de `Proyecto_Pendulo_CPU1`, su `main` (`control_perturbacion.c`) y
`F2837xD_Adc.c` están dentro de la carpeta, y la configuración de compilación es `CPU1_RAM_DEBUG`. Los requisitos de la sección
siguiente (símbolo `_LAUNCHXL_F28379D`, reloj de 200 MHz) valen igual. Antes se llamaba `Proyecto_Pendulo_Perturbacion`.

## Requisitos de compilación que importan

- Símbolo predefinido **`_LAUNCHXL_F28379D`** a nivel *proyecto* y en **todas** las configuraciones (Properties → Build →
  C2000 Compiler → Predefined Symbols), y luego *Clean*. Sin él el reloj queda en 100 MHz y el baud real cambia (se verificó:
  `sysclk_mhz_reg` pasó de 100 a 200 MHz).
- Incluir `F2837xD_Adc.c` (para `AdcSetMode`) además de los archivos habituales del SDK.
- Variables útiles en la ventana *Watch*: `sysclk_mhz_reg`, `lspclk_mhz_reg`, `tx_frames`, `isr_max_cycles`,
  `isr_overruns`, `theta_mean_acc`, `offset_lsb`, `b_sugerido` y las de la autocalibración.
- El baud real es 115 741 (BRR = 53 con LSPCLK = 50 MHz), +0.47 % del nominal de 115200: coherente con el middleware.

## Calibración

`B` (`ADC_CAL_B`) compensa el offset del lazo DAC→ADC: entre ≈ 5.5 y 6.6 LSB a 200 MHz según la sesión (−17.6 LSB a 100 MHz); el valor fijo del firmware es 6.1. El offset depende del
reloj y de la configuración del ADC, y además se desplazó entre sesiones (9 y 17 de noviembre) sin causa identificada. Por eso
existe la autocalibración, que lo mide en cada arranque. Si se usa calibración fija, repítela cuando cambie algo del montaje.

## Reloj del DSP: 200 MHz obligatorio (protecciones)

El HIL exige SYSCLK = 200 MHz (Ts = 10 µs = 2000 ciclos). El reloj lo fija `InitSysCtrl()` (C2000Ware) según el símbolo
`_LAUNCHXL_F28379D`; sin él el PLL queda a 100 MHz. Protecciones:

1. **Compilación:** si falta el símbolo, `#error` (no un `#warning`).
2. **Ejecución:** tras `InitSysCtrl()` se lee el PLL; si no son 200 MHz, `clock_ok = 0` y el programa se detiene.
3. **Python:** el middleware mide la tasa de tramas; ~50/s (en vez de 100/s) indica reloj a 100 MHz, aborta y no copia el CSV.

Lista de comprobación antes de capturar: compila sin `#error`; en *Watch* `sysclk_mhz_reg = 200` y `clock_ok = 1`; Python
imprime "Reloj del DSP correcto: ~100 tramas/s".
