# Plataforma Hardware-in-the-Loop (HIL) de un péndulo invertido

Tesis de maestría en robótica (UTM) — Eduardo Pascual Pérez.

Plataforma HIL sobre una **TI LAUNCHXL-F28379D**: la planta (péndulo invertido linealizado) se simula en el
firmware dentro de una interrupción a 100 kHz; el ángulo sale por el DAC-B (pin 70), regresa por un jumper al
ADC-A0 (pin 30), pasa por un controlador PID y la telemetría se envía por UART (SCI-A) a un *middleware* en
Python que alimenta a CoppeliaSim y a los scripts de análisis en MATLAB.

```
 DSP (ISR 100 kHz): planta ──DAC-B──► jumper ──► ADC-A0 ──► PID ──► planta
        │ telemetría cada 10 ms (UART, 115200 baud)
        ▼
 Python (middleware/hil_coppelia.py) ──► CSV (analisis/) ──► MATLAB
        └────────────────────────────► CoppeliaSim (simulacion/)
```

## Estructura del repositorio

| Carpeta | Contenido |
|---|---|
| [`firmware/`](firmware/README.md) | Proyecto de CCS (CPU1) y fuentes vigentes del controlador (`fuentes/control_hil_v3.c`) |
| [`middleware/`](middleware/README.md) | Scripts de Python: puente DSP ↔ CoppeliaSim + CSV, y herramientas de diagnóstico serial |
| [`analisis/`](analisis/README.md) | Scripts de MATLAB/Python para analizar y comparar datos; CSV de las pruebas |
| [`simulacion/`](simulacion/README.md) | Escena de CoppeliaSim del péndulo |
| [`docs/`](docs/README.md) | Bitácora de pruebas, reportes (PDF) y capturas de evidencia |
| [`pruebas_respaldo/`](pruebas_respaldo/README.md) | Respaldos históricos de versiones anteriores del firmware y del middleware |

## Estado actual (2026-11-01)

- Modelo **sin perturbación, un solo núcleo**: converge a ≈ 0 rad (error en régimen permanente ≈ 0.2–0.5 mrad
  medido con el acumulador interno del firmware). Detalle y evidencias en `docs/bitacora_pruebas.md`.
- Abierto: el transitorio del DSP está más amortiguado que la simulación en MATLAB (causa sin confirmar).
- Pendiente: perturbación y división del controlador/planta en los dos núcleos.

## Cómo reproducir una corrida (resumen)

1. En CCS compila `firmware/fuentes/control_hil_v3.c` con el símbolo predefinido `_LAUNCHXL_F28379D`
   (a nivel **proyecto**) y `USE_TRIGGER 1`; carga y da *Resume*.
2. Abre CoppeliaSim con `simulacion/Escena_Pendulo.ttt` (o usa `--sin-coppelia`).
3. Ejecuta `python middleware/hil_coppelia.py` (cierra otros programas que usen el COM).
4. Analiza el CSV generado en `analisis/` con `comparacion_sin_perturbacion.m`.

Pasos detallados y advertencias: ver los README de cada carpeta.
