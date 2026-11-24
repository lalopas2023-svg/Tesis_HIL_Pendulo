# Plataforma Hardware-in-the-Loop (HIL) de un péndulo invertido

Proyecto de maestría en robótica (UTM) — Eduardo Pascual Pérez.

Plataforma HIL sobre una **TI LAUNCHXL-F28379D**: la planta (péndulo invertido linealizado) se simula en el
firmware dentro de una interrupción a 100 kHz; el ángulo sale por el DAC-B (pin 70), regresa por un cable al
ADC-A0 (pin 30), pasa por un controlador PID y la telemetría se envía por UART (SCI-A) a un *middleware* en
Python que alimenta a CoppeliaSim y a los scripts de análisis en MATLAB y Python.

```
 DSP (ISR 100 kHz): planta ──DAC-B──► cable ──► ADC-A0 ──► PID ──► planta
        │ telemetría cada 10 ms (UART, 115200 baud nominales)
        ▼
 Python (middleware/hil_coppelia.py) ──► CSV (analisis/) ──► MATLAB / Python
        └────────────────────────────► CoppeliaSim (simulacion/)
```

## Dónde está cada cosa

| Carpeta | Contenido |
|---|---|
| [`firmware/`](firmware/README.md) | Proyecto de CCS (CPU1) y las fuentes del controlador: base probada, autocalibración, prueba de comandos UART, variante con perturbación y diagnóstico del lazo |
| [`middleware/`](middleware/README.md) | Scripts de Python: puente DSP ↔ CoppeliaSim con registro en CSV, visualización y diagnóstico serial |
| [`analisis/`](analisis/README.md) | Scripts de MATLAB y Python que comparan el DSP con la simulación y miden latencia; CSV que respaldan los resultados |
| [`simulacion/`](simulacion/README.md) | Escena de CoppeliaSim del péndulo |
| [`docs/`](docs/README.md) | Bitácora de pruebas, reportes individuales (LaTeX y PDF), capturas de evidencia y la propuesta de interfaz gráfica con sus pruebas de factibilidad |

Para leer el proyecto en orden: este README, [`docs/bitacora_pruebas.md`](docs/bitacora_pruebas.md) (qué se probó, con qué
resultado y qué sigue abierto) y, si interesa la interfaz gráfica, [`docs/gui/README.md`](docs/gui/README.md).

## Estado actual (2026-11-24)

- **Modelo sin perturbación, un solo núcleo:** implementado y probado. Con la autocalibración del offset del ADC el error
  en régimen permanente queda dentro de ±0.4 mrad (18 corridas en 3 arranques). La autocalibración está validada, pero su
  inclusión en el firmware definitivo espera la decisión del director.
- **Con perturbación** (potenciómetro o escalón): implementado y capturado en corridas de 90 s (2026-11-03 y 11-05).
- **Abierto:** el transitorio del DSP está más amortiguado que la simulación en MATLAB (causa sin confirmar) y no se
  sabe por qué el offset del lazo cambió entre el 9 y el 17 de noviembre (la autocalibración lo compensa, pero no lo explica).
- **Pendiente:** dividir controlador y planta en los dos núcleos (prueba F4, diferida).
- **Interfaz gráfica y generación de configuración:** es una propuesta en evaluación. Las pruebas de factibilidad F1
  (compilar sin IDE), F2 (cargar sin IDE) y F6 (CoppeliaSim por línea de comandos) están aprobadas; F5 (comandos por UART
  con la ISR activa) está preparada y sin ejecutar en la tarjeta; F3 (flash frente a RAM) está pendiente. Detalle en [`docs/gui/`](docs/gui/README.md).

## Cómo reproducir una corrida (resumen)

1. **Compilar.** Abre `firmware/Proyecto_Pendulo_CPU1` en CCS 20.5.1 con la configuración `CPU1_RAM_DEBUG`. El proyecto
   **no contiene el `main`**: enlaza `ipc_gpio_toggle_cpu01.c` del ejemplo de C2000Ware 5.2.0.00, y ese archivo es el
   firmware (el `main` probado es idéntico a `firmware/fuentes/sin_perturbacion/control_hil_sin_perturbacion.c`). Para probar
   otra versión de `firmware/fuentes/` hazlo en una copia del proyecto (ver [`firmware/README.md`](firmware/README.md)); editar
   el archivo enlazado modifica el ejemplo del SDK. El símbolo `_LAUNCHXL_F28379D` debe estar definido en el proyecto, o la
   compilación falla con `#error` (con él, SYSCLK = 200 MHz).
2. **Cargar y arrancar.** Carga el programa y da *Resume*. Con `USE_TRIGGER 1` el DSP espera la `S` de Python.
3. **CoppeliaSim.** Abre `simulacion/Escena_Pendulo.ttt` (o usa `--sin-coppelia` en el paso 4).
4. **Capturar.** Cierra otros programas que usen el puerto COM y ejecuta `python middleware/hil_coppelia.py`. Escribe el
   CSV de la corrida en `analisis/`.
5. **Analizar.** `analisis/comparacion_sin_perturbacion.m` (sin perturbación) o `comparacion_con_perturbacion.m`.

La compilación y la carga también se pueden hacer sin el IDE (`ccs-serverc` y `DSLite`); los comandos y sus resultados están
en [`docs/gui/Resultados_F1_2026-11-17.md`](docs/gui/Resultados_F1_2026-11-17.md) y [`Resultados_F2_2026-11-17.md`](docs/gui/Resultados_F2_2026-11-17.md).

## Entorno de referencia

CCS 20.5.1.00012, compilador `ti-cgt-c2000` 25.11.0.LTS, C2000Ware 5.2.0.00, DSLite 20.5.0.4063, sonda XDS100v2
(integrada en la LaunchPad), Windows 11, CoppeliaSim Edu y `coppeliasim_zmqremoteapi_client` 2.0.4. Puerto serie de la
LaunchPad: COM10 (puede cambiar al reconectar el USB).

## Qué no se versiona

Los CSV nuevos que genera el middleware en cada corrida (`analisis/datos_*_AAAAMMDD_HHMMSS.csv` y sus copias sin fecha), las
salidas de compilación (`*.out`, `*.map`, carpetas `*DEBUG/`) y la copia de trabajo `firmware/_factibilidad/` están en
`.gitignore`. Solo se versionan los CSV que respaldan un resultado de la bitácora.
