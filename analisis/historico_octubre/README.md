# analisis/historico_octubre/

Material de las sesiones del 2026-10-04 al 10-12, hecho con el firmware previo a la calibración del offset (error en régimen
permanente ≈ 14 mrad). **No se mantiene**: está aquí porque sustenta las pruebas 2 a 5 de `docs/bitacora_pruebas.md`
(ruido del ADC, derivada, comparación DSP vs MATLAB). Los scripts conservan sus nombres originales, con erratas, y esperan
columnas como `Angulo_rad`. Se ejecutan con esta carpeta como directorio actual.

| Archivo | Qué es |
|---|---|
| `Comapracion.m` | Compara Ts = 10 µs frente a Ts = 10 ms sobre los mismos datos (`datos_derivada_lenta.csv`). |
| `Comapracion_matlab.m` | Primera comparación DSP vs MATLAB (`datos_perturbacion_fija.csv`). La reemplaza `../comparacion_sin_perturbacion.m` (v2). |
| `analisi_ruido.m`, `analisis_multivariable.m`, `analisis_transitorio.m` | Ruido, análisis multivariable y transitorio sobre `datos_tesis_pendulo.csv`. |
| `datos_derivada_lenta.csv`, `datos_perturbacion_fija.csv`, `datos_tesis_pendulo.csv` | Corridas del 2026-10-04/05. |
| `figuras/` | Figuras que produjeron esos scripts. |

Si ya no hacen falta, la carpeta completa se puede borrar sin afectar nada de lo vigente (todo queda en el historial de git).
