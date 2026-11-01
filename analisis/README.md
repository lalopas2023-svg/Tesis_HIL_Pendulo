# analisis/

Scripts y datos de las pruebas. Los `.m` leen el CSV por nombre: ejecútalos con esta carpeta como directorio actual.

## Scripts vigentes
- **`comparacion_sin_perturbacion.m`** (v2) — compara DSP vs simulación en MATLAB del mismo lazo (sin perturbación):
  RMS total/transitorio/permanente y figuras. Lee `datos_sin_perturbacion.csv`.
- **`analisis_barrido.m`** — ajusta ADC = m·DAC + b a partir del barrido de `diag_adc_dac.c`.
- **`sim_lazo.py`** — simulación en Python del lazo (apoyo a los análisis de sensibilidad).

## Scripts de sesiones anteriores
`Comapracion.m`, `Comapracion_matlab.m`, `analisi_ruido.m`, `analisis_multivariable.m`, `analisis_transitorio.m`
(nombres con erratas conservados tal cual; algunos esperan columnas como `Angulo_rad`).

## Datos
| Archivo | Qué es |
|---|---|
| `datos_sin_perturbacion.csv` | última corrida sin perturbación copiada por el middleware (versión del repo previa a esta sesión) |
| `datos_sin_perturbacion_20261101_011726.csv` | corrida de 20 s (2000 líneas) de 2026-11-01, previa a la corrección del arranque [v3-12] |
| `barrido_dac_adc.csv` | barrido DAC→ADC (65 puntos; hecho con SYSCLK = 100 MHz) |
| `datos_derivada_lenta.csv`, `datos_perturbacion_fija.csv`, `datos_tesis_pendulo.csv` | corridas del 2026-10-04/05 |

Columnas de los CSV del DSP: `Tiempo_s, Posicion_rad, Velocidad_rad_s, Control_u` (+ `Tiempo_reloj_s` en los nuevos).
La "velocidad" es la derivada filtrada del DSP, no la θ̇ real; la telemetría tiene resolución de 1 mrad.
