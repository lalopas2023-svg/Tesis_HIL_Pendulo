# ============================================================
# Middleware HIL: DSP <-> CoppeliaSim
# Con trigger de arranque para capturar transitorio completo
# ============================================================

import serial
import time
import csv
from coppeliasim_zmqremoteapi_client import RemoteAPIClient

# --- CONFIGURACIÓN SERIAL ---
PUERTO_COM = 'COM10'
BAUD_RATE = 115200

try:
    ser = serial.Serial(PUERTO_COM, BAUD_RATE, timeout=1)
    ser.reset_input_buffer()
    time.sleep(0.2)
    print(f"Conectado al DSP en {PUERTO_COM} a {BAUD_RATE} bps")
except Exception as e:
    print(f"Error al abrir el puerto: {e}")
    exit()

# --- CONEXIÓN A COPPELIASIM ---
print("Conectando a CoppeliaSim...")
client = RemoteAPIClient()
sim = client.getObject('sim')

joint_handle = sim.getObject('/pendulum_joint')
graph_handle = sim.getObject('/graph')

stream_id_pos = sim.addGraphStream(graph_handle, 'Posicion (rad)', 'rad', 0, [1, 0, 0])
stream_id_vel = sim.addGraphStream(graph_handle, 'Velocidad (rad/s)', 'rad/s', 0, [0, 1, 0])
stream_id_u   = sim.addGraphStream(graph_handle, 'Control (u)', 'u', 0, [0, 0, 1])

print("Iniciando simulación...")
sim.startSimulation()

# ============================================================
# TRIGGER: enviar 'S' al DSP para arrancar el control
# ============================================================
print("Esperando DSP...")
time.sleep(0.5)
ser.reset_input_buffer()

print("Enviando comando 'S' al DSP...")
ser.write(b'S')
ser.flush()

# Esperar confirmación
time.sleep(0.2)
confirmacion = ser.readline().decode('ascii', errors='ignore').strip()
print(f"Respuesta del DSP: '{confirmacion}'")

if confirmacion != "OK":
    print("ADVERTENCIA: El DSP no confirmó el arranque correctamente.")

print("Grabando desde el inicio del transitorio...")

# ============================================================
# BUCLE PRINCIPAL
# ============================================================
try:
    with open('../analisis/datos_sin_perturbacion.csv', mode='w', newline='') as archivo_csv:
        escritor_csv = csv.writer(archivo_csv)
        escritor_csv.writerow(['Tiempo_s', 'Posicion_rad', 'Velocidad_rad_s', 'Control_u'])
        
        tiempo_simulado = 0.0
        paso_tiempo = 0.01
        lineas_validas = 0
        lineas_invalidas = 0
        
        while True:
            if ser.in_waiting > 0:
                linea = ser.readline().decode('ascii', errors='ignore').strip()
                
                if linea and linea.count(',') == 2:
                    try:
                        valores = linea.split(',')
                        if len(valores) == 3:
                            pos_rad = int(valores[0]) / 1000.0
                            vel_rad_s = int(valores[1]) / 1000.0
                            u_control = int(valores[2]) / 1000.0
                            
                            sim.setJointPosition(joint_handle, pos_rad)
                            sim.setGraphStreamValue(graph_handle, stream_id_pos, pos_rad)
                            sim.setGraphStreamValue(graph_handle, stream_id_vel, vel_rad_s)
                            sim.setGraphStreamValue(graph_handle, stream_id_u, u_control)
                            
                            escritor_csv.writerow([round(tiempo_simulado, 4),
                                                    pos_rad, vel_rad_s, u_control])
                            tiempo_simulado += paso_tiempo
                            lineas_validas += 1
                        else:
                            lineas_invalidas += 1
                    except ValueError:
                        lineas_invalidas += 1

except KeyboardInterrupt:
    print("\nDeteniendo por el usuario...")

finally:
    sim.stopSimulation()
    ser.close()
    print(f"\nPuerto COM liberado.")
    print(f"Líneas válidas: {lineas_validas}")
    print(f"Líneas inválidas: {lineas_invalidas}")
    print(f"Duración: {tiempo_simulado:.2f} s")
    print(f"Archivo: ../analisis/datos_sin_perturbacion.csv")