"""Mide el atraso de la lectura (backlog) de una corrida a partir de las columnas de reloj.

Uso:  python latencia_csv.py datos_con_perturbacion_AAAAMMDD_HHMMSS.csv [...]

lag = Tiempo_reloj_s - (Tiempo_s + 0.01): cuánto después de lo que dice el DSP se procesó cada fila.
 - lag ~ constante y pequeño  -> lectura en tiempo real.
 - lag creciendo (deriva > 0)  -> el PC no alcanza a procesar 100 líneas/s: las líneas se acumulan
                                  en el buffer y la pantalla va cada vez más atrasada.
"""
import sys
import numpy as np
import pandas as pd

for f in sys.argv[1:]:
    d = pd.read_csv(f)
    tn = d.Tiempo_s.values + 0.01
    tr = d.Tiempo_reloj_s.values
    lag = tr - tn
    dt = np.diff(tr)
    deriva = np.polyfit(tn, lag, 1)[0]
    print(f"{f}: filas={len(d)} nominal={tn[-1]:.2f}s reloj={tr[-1]:.2f}s "
          f"lag_final={lag[-1]:+.2f}s lag_max={lag.max():+.2f}s deriva={deriva * 1e3:+.1f} ms/s "
          f"dt_med={np.median(dt) * 1e3:.1f} ms p95={np.percentile(dt, 95) * 1e3:.1f} ms")
