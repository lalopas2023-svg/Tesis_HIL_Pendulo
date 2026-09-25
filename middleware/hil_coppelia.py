import serial
import time
import csv
from coppeliasim_zmqremoteapi_client import RemoteAPIClient

# --- 1. CONFIGURACIÓN DEL PUERTO SERIAL ---
PUERTO_COM = 'COM10' 
BAUD_RATE = 57600

try:
    ser = serial.Serial(PUERTO_COM, BAUD_RATE, timeout=0)
    print(f"Conectado al DSP en {PUERTO_COM}")
except Exception as e:
    print(f"Error al abrir el puerto: {e}")
    exit()

# --- 2. CONFIGURACIÓN DE ZEROMQ CON COPPELIASIM ---
print("Conectando a CoppeliaSim...")
client = RemoteAPIClient()
sim = client.getObject('sim')

joint_handle = sim.getObject('/pendulum_joint')
graph_handle = sim.getObject('/graph')

# NUEVO: Inicialización de los tres canales para la gráfica de CoppeliaSim
stream_id_pos = sim.addGraphStream(graph_handle, 'Posicion (rad)', 'rad', 0, [1, 0, 0])      # Línea Roja
stream_id_vel = sim.addGraphStream(graph_handle, 'Velocidad (rad/s)', 'rad/s', 0, [0, 1, 0]) # Línea Verde
stream_id_u = sim.addGraphStream(graph_handle, 'Control (u)', 'u', 0, [0, 0, 1])             # Línea Azul

print("Iniciando simulación HIL...")
sim.startSimulation()

# --- 3. BUCLE DE CONTROL EN TIEMPO REAL Y GUARDADO ---
try:
    with open('../analisis/datos_tesis_pendulo.csv', mode='w', newline='') as archivo_csv:
        escritor_csv = csv.writer(archivo_csv)
        escritor_csv.writerow(['Tiempo_s', 'Posicion_rad', 'Velocidad_rad_s', 'Control_u'])
        
        print("Grabando múltiples variables en CSV...")
        
        # Tiempo determinista basado en el hardware (10 ms por ciclo)
        tiempo_simulado = 0.0
        paso_tiempo = 0.01  
        
        while True:
            if ser.in_waiting > 0:
                linea = ser.readline().decode('utf-8').strip()
                
                if linea:
                    try:
                        valores = linea.split(',')
                        
                        if len(valores) == 3:
                            # Des-escalar dividiendo entre 1000
                            pos_rad = int(valores[0]) / 1000.0
                            vel_rad_s = int(valores[1]) / 1000.0
                            u_control = int(valores[2]) / 1000.0
                            
                            # Inyección cinemática (mover el modelo 3D)
                            sim.setJointPosition(joint_handle, pos_rad)
                            
                            # NUEVO: Dibujar las tres variables en la gráfica flotante
                            sim.setGraphStreamValue(graph_handle, stream_id_pos, pos_rad)
                            #sim.setGraphStreamValue(graph_handle, stream_id_vel, vel_rad_s)
                            sim.setGraphStreamValue(graph_handle, stream_id_u, u_control)
                            
                            # Escribir la fila completa en el CSV
                            escritor_csv.writerow([round(tiempo_simulado, 4), pos_rad, vel_rad_s, u_control])
                            
                            # Avanzar el tiempo exactamente 0.01s
                            tiempo_simulado += paso_tiempo
                            
                    except ValueError:
                        # Ignorar tramas corruptas sin detener la simulación
                        pass

except KeyboardInterrupt:
    print("\nDeteniendo simulación por el usuario...")

finally:
    # --- 4. CIERRE LIMPIO ---
    sim.stopSimulation()
    ser.close()
    print("Puerto COM liberado, simulación terminada y archivo guardado exitosamente.")