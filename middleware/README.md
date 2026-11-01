# middleware/

Scripts de Python (requieren `pyserial`; para CoppeliaSim, `coppeliasim_zmqremoteapi_client`).

- **`hil_coppelia.py`** — puente DSP ↔ CoppeliaSim. Envía 'S', espera "OK", lee líneas `pos,vel,u` (mrad, mrad/s,
  milésimas), guarda el CSV y actualiza la escena. `--sin-coppelia` graba sólo el CSV. Configuración (COM, baud,
  duración) al inicio del archivo. Escribe `analisis/datos_sin_perturbacion_AAAAMMDD_HHMMSS.csv` y una copia
  `datos_sin_perturbacion.csv`.
- **`sniffer_serial.py`** — diagnóstico: manda 'S' y muestra los bytes crudos durante 6 s.
- **`test_serial.py`** — prueba antigua de baud rates (57600/115200/38400).
- **`datos_tesis_pendulo.csv`** — captura antigua (Tiempo_s, Angulo_rad).
- **`hil_coppelia_261004.py`** — archivo vacío (marcador de una versión anterior).

Uso: primero Resume en CCS, luego abrir CoppeliaSim con la escena `Escena_Pendulo`, cerrar otros programas que
usen el COM, y ejecutar. El DSP atiende la 'S' sólo una vez por arranque: para repetir, reinicia el programa en CCS.
