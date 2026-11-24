# middleware/

Scripts de Python que conectan el DSP con CoppeliaSim y registran los datos. Requieren `pyserial`; para CoppeliaSim,
`coppeliasim_zmqremoteapi_client` (2.0.4 en el laboratorio).

| Archivo | Qué hace |
|---|---|
| `hil_coppelia.py` | Puente DSP ↔ CoppeliaSim para el modelo **sin perturbación**. Envía `S`, espera `OK`, lee líneas `pos,vel,u` (mrad, mrad/s, milésimas), guarda el CSV y actualiza la escena. `--sin-coppelia` graba solo el CSV. Puerto, baud y duración se configuran al inicio del archivo (COM10, 115200, 20 s). Mide la tasa de tramas: ~100/s es el reloj correcto; ~50/s indica 100 MHz y aborta. |
| `hil_coppelia_perturbacion.py` | Lo mismo para el firmware **con perturbación** (`control_perturbacion.c`): trama de 4 columnas (`pos,vel,u,pert`). |
| `viz_coppelia.py` | Visualización en CoppeliaSim en un hilo aparte (lo importan los dos scripts anteriores). Siempre muestra la muestra más reciente. |
| `sniffer_serial.py` | Diagnóstico: manda `S` y muestra los bytes crudos durante 6 s (sirvió para encontrar la inundación de `9` por `sprintf`). |

## Uso

1. Carga el firmware en la tarjeta y da *Resume* (con `USE_TRIGGER 1` espera la `S`). También se puede cargar sin IDE
   (ver `docs/gui/Resultados_F2_2026-11-17.md`).
2. Abre CoppeliaSim con `simulacion/Escena_Pendulo.ttt`, o usa `--sin-coppelia`.
3. Cierra los programas que usen el COM y ejecuta el script.

El DSP atiende la `S` **una sola vez por arranque**: para repetir una corrida hay que reiniciar el programa en la tarjeta
(o cargarlo de nuevo).

## Datos que produce

Cada corrida se guarda en `analisis/` como `datos_sin_perturbacion_AAAAMMDD_HHMMSS.csv` (o `datos_con_perturbacion_...`) y la
última se copia sin fecha (`datos_sin_perturbacion.csv`). Columnas: `Tiempo_s, Posicion_rad, Velocidad_rad_s, Control_u`,
más `Perturbacion` en el caso con perturbación y `Tiempo_reloj_s` (reloj del PC, para medir latencia). Estos archivos están en
`.gitignore`; solo se versionan los que respaldan un resultado de la bitácora.

## Tiempo real

- El hilo principal solo lee el puerto y guarda el CSV completo (100 Hz); la visualización corre en otro hilo y siempre
  muestra la muestra más reciente (`VIZ_HZ`, `GRAFICA_HZ`). Si CoppeliaSim es lento se omiten cuadros, pero no se acumula
  retraso.
- Antes, 4–5 llamadas a CoppeliaSim por línea (~11 ms) superaban los 10 ms disponibles: en una corrida de 90 s la pantalla
  llegó a ir 7.9 s atrasada. Se mide en cualquier corrida con `analisis/latencia_csv.py`.
- Al terminar imprime el costo por actualización y la edad del dato al mostrarse.

## Advertencias

- Con la escena sin abrir (o sin `/pendulum_joint` y `/graph`) el script falla con `object does not exist`. CoppeliaSim no
  avisa al abrir una escena inexistente; ver `docs/gui/Resultados_F6_2026-11-19.md`.
- El puerto serie cambia de número al reconectar el USB: confirma el COM antes de ejecutar.
