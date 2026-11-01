# ============================================================
# Middleware HIL: DSP (TMS320F28379D) <-> CoppeliaSim   (v2)
# Compatible con control_hil_v3.c  (USE_TRIGGER = 1)
#
# Protocolo (sin cambios):
#   Python -> DSP : el byte 'S' (arranca el control)
#   DSP -> Python : "OK\r\n"  y después, cada 10 ms, una línea
#                   "pos_mrad,vel_mrad_s,u_milli\r\n"  (3 enteros)
#
# ORDEN DE USO (importante):
#   1) En CCS: carga el programa con USE_TRIGGER = 1 y dale Resume.
#      El DSP queda esperando la 'S' (el LED/timer no hacen nada aún).
#   2) Abre CoppeliaSim con la escena (/pendulum_joint y /graph).
#   3) Cierra cualquier monitor serie que use el COM.
#   4) Ejecuta este script.
#   Para repetir una corrida: en CCS reinicia el programa (Restart / reset)
#   y vuelve a dar Resume ANTES de ejecutar el script otra vez.
#   El DSP solo atiende la 'S' una vez después de cada arranque.
# ============================================================

import csv
import os
import re
import shutil
import sys
import time
from datetime import datetime

import serial

# ----------------------------- CONFIGURACIÓN -----------------------------
PUERTO_COM = 'COM10'
BAUD_RATE = 115200          # el firmware v3 da 115 741 baud reales (+0.47 %)
DURACION_S = 20.0           # segundos de datos a grabar; None = hasta Ctrl+C
USAR_COPPELIA = '--sin-coppelia' not in sys.argv   # python script.py --sin-coppelia  = solo CSV
COPPELIA_CADA = 1           # actualizar CoppeliaSim cada N líneas (sube si va lento)
PASO_TIEMPO = 0.01          # el DSP manda una línea cada 10 ms
INTENTOS_TRIGGER = 3        # veces que se reintenta enviar 'S'
ESPERA_OK_S = 1.5           # segundos de espera de "OK" por intento

CARPETA_SALIDA = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'analisis'))
# Además del archivo con fecha, copiar la corrida a este nombre fijo (el que leen
# tus scripts de MATLAB). La corrida con fecha nunca se sobreescribe.
COPIAR_COMO_ULTIMO = True
NOMBRE_ULTIMO = 'datos_sin_perturbacion.csv'

PATRON_DATOS = re.compile(r'^-?\d+,-?\d+,-?\d+$')   # exactamente 3 enteros


class LectorLineas:
    """Lee el puerto sin quedarse bloqueado.

    ser.readline() no regresa mientras lleguen bytes sin '\\n' (por ejemplo si el
    DSP inunda la línea con basura): el programa parecía congelado y no imprimía
    nada. Este lector respeta el timeout aunque la línea nunca termine.
    """
    MAX_LINEA = 512

    def __init__(self, ser):
        self.ser = ser
        self.buf = bytearray()
        self.bytes_total = 0          # bytes crudos recibidos (cualquier cosa)

    def linea(self, timeout=0.5):
        t0 = time.time()
        while True:
            i = self.buf.find(b'\n')
            if i >= 0:
                raw = bytes(self.buf[:i + 1])
                del self.buf[:i + 1]
                return raw.decode('ascii', errors='ignore').strip()
            if len(self.buf) > self.MAX_LINEA:          # basura sin salto de línea
                raw = bytes(self.buf[:40])
                self.buf.clear()
                return '<sin salto de línea> ' + raw.decode('ascii', errors='replace')
            if time.time() - t0 >= timeout:
                return ''
            n = self.ser.in_waiting
            d = self.ser.read(n if n > 0 else 1)
            if d:
                self.bytes_total += len(d)
                self.buf += d


def hacer_trigger(ser, lector):
    """Envía 'S' y espera 'OK'. Devuelve (ok, primera_linea_de_datos_o_None)."""
    for intento in range(1, INTENTOS_TRIGGER + 1):
        ser.reset_input_buffer()
        lector.buf.clear()
        print(f"Enviando 'S' al DSP (intento {intento}/{INTENTOS_TRIGGER})...")
        ser.write(b'S')
        ser.flush()
        t0 = time.time()
        while time.time() - t0 < ESPERA_OK_S:
            linea = lector.linea(0.2)
            if not linea:
                continue
            if 'OK' in linea:
                print("DSP confirmó el arranque (OK).")
                return True, None
            if PATRON_DATOS.match(linea):
                # El OK se perdió o se corrompió, pero el DSP ya está mandando datos.
                print("No llegó 'OK', pero ya llegan datos: el DSP arrancó.")
                return True, linea
    return False, None


def main():
    # Variables inicializadas ANTES de cualquier try (el finally las usa).
    ser = None
    sim = None
    client = None
    archivo_csv = None
    ruta_csv = None
    lineas_validas = 0
    lineas_ignoradas = 0
    tiempo_simulado = 0.0
    t_inicio = None
    max_pendiente = 0
    etapa = 'iniciando'
    lector = None
    bytes_total = 0
    ultima_cruda = ''
    t_latido = 0.0

    try:
        # --- Puerto serie ---------------------------------------------------
        try:
            ser = serial.Serial(PUERTO_COM, BAUD_RATE, timeout=0.05)
        except Exception as e:
            print(f"Error al abrir {PUERTO_COM}: {e}")
            print("Revisa: ¿otro programa usa el puerto? ¿es el COM correcto?")
            return 1
        time.sleep(0.2)
        ser.reset_input_buffer()
        lector = LectorLineas(ser)
        print(f"Conectado al DSP en {PUERTO_COM} a {BAUD_RATE} bps")

        # --- CoppeliaSim ----------------------------------------------------
        joint_handle = graph_handle = None
        id_pos = id_vel = id_u = None
        if USAR_COPPELIA:
            from coppeliasim_zmqremoteapi_client import RemoteAPIClient
            print("Conectando a CoppeliaSim...")
            client = RemoteAPIClient()
            sim = client.getObject('sim')
            joint_handle = sim.getObject('/pendulum_joint')
            graph_handle = sim.getObject('/graph')
            id_pos = sim.addGraphStream(graph_handle, 'Posicion (rad)', 'rad', 0, [1, 0, 0])
            id_vel = sim.addGraphStream(graph_handle, 'Velocidad (rad/s)', 'rad/s', 0, [0, 1, 0])
            id_u = sim.addGraphStream(graph_handle, 'Control (u)', 'u', 0, [0, 0, 1])
            print("Iniciando simulación...")
            sim.startSimulation()

        # --- Trigger --------------------------------------------------------
        ok, primera = hacer_trigger(ser, lector)
        if not ok:
            print("ERROR: el DSP no respondió a la 'S'. No se graba nada.")
            print("Revisa: ¿CCS está en Resume con USE_TRIGGER = 1? ¿Se reinició el")
            print("programa desde el último arranque? ¿Baud/puerto correctos?")
            return 2

        # --- Archivo de salida ---------------------------------------------
        os.makedirs(CARPETA_SALIDA, exist_ok=True)
        marca = datetime.now().strftime('%Y%m%d_%H%M%S')
        ruta_csv = os.path.join(CARPETA_SALIDA, f'datos_sin_perturbacion_{marca}.csv')
        archivo_csv = open(ruta_csv, mode='w', newline='')
        escritor = csv.writer(archivo_csv)
        escritor.writerow(['Tiempo_s', 'Posicion_rad', 'Velocidad_rad_s',
                           'Control_u', 'Tiempo_reloj_s'])
        print(f"Grabando en {ruta_csv}")
        print("Ctrl+C para detener antes." if DURACION_S is None
              else f"Grabando {DURACION_S:.0f} s (Ctrl+C para detener antes).")

        t_inicio = time.time()
        t_latido = t_inicio
        pendientes = [primera] if primera else []

        # --- Bucle principal -----------------------------------------------
        while True:
            if DURACION_S is not None and tiempo_simulado >= DURACION_S:
                break
            etapa = 'esperando una línea del DSP'
            if pendientes:
                linea = pendientes.pop(0)
            else:
                linea = lector.linea(0.5)    # espera hasta 0.5 s: no gasta CPU
                max_pendiente = max(max_pendiente, ser.in_waiting)
            bytes_total = lector.bytes_total
            if linea:
                ultima_cruda = linea
            # Latido cada 2 s: así sabes qué está pasando mientras corre.
            if time.time() - t_latido >= 2.0:
                t_latido = time.time()
                print(f"  [{t_latido - t_inicio:5.1f} s] válidas={lineas_validas} "
                      f"ignoradas={lineas_ignoradas} bytes={bytes_total} "
                      f"última='{ultima_cruda[:30]}'")
            if not linea:
                continue
            if not PATRON_DATOS.match(linea):
                lineas_ignoradas += 1        # ruido, "OK" repetido, línea cortada
                continue

            p, v, c = linea.split(',')
            pos_rad = int(p) / 1000.0
            vel_rad_s = int(v) / 1000.0
            u_control = int(c) / 1000.0

            # Primero se GUARDA en el CSV; si CoppeliaSim se traba, los datos ya están.
            escritor.writerow([round(tiempo_simulado, 4), pos_rad, vel_rad_s,
                               u_control, round(time.time() - t_inicio, 4)])
            archivo_csv.flush()
            tiempo_simulado += PASO_TIEMPO
            lineas_validas += 1

            if USAR_COPPELIA and ((lineas_validas - 1) % COPPELIA_CADA == 0):
                etapa = 'enviando datos a CoppeliaSim (si se queda aquí, CoppeliaSim se trabó)'
                sim.setJointPosition(joint_handle, pos_rad)
                sim.setGraphStreamValue(graph_handle, id_pos, pos_rad)
                sim.setGraphStreamValue(graph_handle, id_vel, vel_rad_s)
                sim.setGraphStreamValue(graph_handle, id_u, u_control)

    except KeyboardInterrupt:
        print(f"\nDeteniendo por el usuario. Estaba: {etapa}")

    finally:
        if archivo_csv is not None:
            archivo_csv.close()
        if sim is not None:
            try:
                sim.stopSimulation()
            except Exception as e:
                print(f"(aviso) no se pudo detener CoppeliaSim: {e}")
        if ser is not None:
            ser.close()
            print("Puerto COM liberado.")

        if ruta_csv is not None and lineas_validas > 0:
            reloj = (time.time() - t_inicio) if t_inicio else 0.0
            print(f"Líneas válidas:    {lineas_validas}")
            print(f"Líneas ignoradas:  {lineas_ignoradas}")
            print(f"Duración (por conteo de líneas): {tiempo_simulado:.2f} s")
            print(f"Duración (reloj real):           {reloj:.2f} s")
            print(f"Máx. bytes acumulados en el buffer serie: {max_pendiente}")
            if abs(reloj - tiempo_simulado) > 0.05 * max(reloj, 1.0):
                print("AVISO: el reloj y el conteo difieren más de 5 %. Si el reloj es "
                      "mayor, se perdieron líneas o Python/CoppeliaSim va lento "
                      "(sube COPPELIA_CADA o pon USAR_COPPELIA = False).")
            print(f"Archivo: {ruta_csv}")
            if COPIAR_COMO_ULTIMO:
                destino = os.path.join(CARPETA_SALIDA, NOMBRE_ULTIMO)
                try:
                    shutil.copyfile(ruta_csv, destino)
                    print(f"Copia para MATLAB: {destino}")
                except Exception as e:
                    print(f"(aviso) no se pudo copiar a {destino}: {e}")
        elif ruta_csv is not None:
            print("No se recibió ningún dato válido: el archivo quedó vacío.")
            print(f"Líneas ignoradas: {lineas_ignoradas}, bytes recibidos: {bytes_total}, "
                  f"última línea cruda: '{ultima_cruda}'")
    return 0


if __name__ == '__main__':
    sys.exit(main())
