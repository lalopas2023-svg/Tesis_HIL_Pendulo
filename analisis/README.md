# analisis/

Scripts que comparan lo medido en el DSP con la simulación y miden la calidad de la captura. Los `.m` y los `.py` leen el
CSV por nombre: ejecútalos con **esta carpeta como directorio actual**.

## Scripts

| Archivo | Qué hace |
|---|---|
| `comparacion_sin_perturbacion.m` (v2) | Compara el DSP con la simulación en MATLAB del mismo lazo, sin perturbación: RMS total, del transitorio y del régimen permanente, y figuras. Lee `datos_sin_perturbacion.csv` (la última corrida que copia el middleware). |
| `comparacion_con_perturbacion.m` (v1) | Lo mismo con perturbación: le pasa a la simulación la `d(t)` real de la columna `Perturbacion`. Lee `datos_con_perturbacion.csv`. Su salida todavía no se ha reportado. |
| `analisis_perturbacion_python.py` | Perfil de `d(t)`, tramos de `d` casi constante y RMS de la corrida con perturbación. Lee `datos_con_perturbacion_potenciometro_20261103.csv`. |
| `analisis_barrido.m` | Ajusta `ADC = m·DAC + b` a partir del barrido de `firmware/fuentes/diagnostico/diag_adc_dac.c`. |
| `latencia_csv.py` | Mide el atraso de la lectura (*backlog*) de una corrida con las columnas de reloj: `python latencia_csv.py archivo.csv`. |
| `sim_lazo.py` | Simulación en Python del lazo del firmware (ADC y DAC de 12 bit, ruido y offset en LSB), de apoyo a los análisis de sensibilidad. |

## Datos versionados

| Archivo | Qué es |
|---|---|
| `barrido_dac_adc.csv` | Barrido DAC→ADC (65 puntos; SYSCLK = 100 MHz). Resultado: `0.9906·DAC + 1.24`. |
| `datos_sin_perturbacion_20261101_011726.csv` | Corrida de 20 s (2000 líneas) del 2026-11-01, **anterior** a la corrección del arranque; la bitácora (Prueba 13) la cita. |
| `datos_con_perturbacion_potenciometro_20261103.csv` | Corrida de 90 s (9000 muestras) con la perturbación movida a mano con el potenciómetro. |

Columnas de los CSV del DSP: `Tiempo_s, Posicion_rad, Velocidad_rad_s, Control_u` (+ `Perturbacion` con perturbación y
`Tiempo_reloj_s` en los nuevos). La "velocidad" es la derivada filtrada del DSP, no la θ̇ real; la telemetría tiene resolución de
1 mrad.

## Lo que no se versiona

El middleware escribe cada corrida como `datos_sin_perturbacion_AAAAMMDD_HHMMSS.csv` (o `datos_con_perturbacion_...`) y copia la
última sin fecha. Las corridas nuevas y esas copias están en `.gitignore`: para correr los scripts de comparación hay que capturar
primero, o apuntar el script a un CSV con fecha. Las capturas del 2026-10-31 al 11-05 que ya estaban versionadas se conservan.
Un CSV nuevo pasa a versionarse solo si respalda un resultado de la bitácora. Las corridas de la autocalibración (19 y 24 de
noviembre) están en la PC del laboratorio.

## Histórico

[`historico_octubre/`](historico_octubre/README.md): scripts, datos y figuras de las sesiones de octubre (firmware anterior a
la calibración). No se mantienen; sustentan las pruebas 2 a 5 de la bitácora.
