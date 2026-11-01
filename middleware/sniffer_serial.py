# Diagnóstico: muestra los bytes CRUDOS que manda el DSP después de la 'S'.
# Uso:  python sniffer_serial.py        (CoppeliaSim NO hace falta)
import time
import serial

PUERTO, BAUD = 'COM10', 115200
ser = serial.Serial(PUERTO, BAUD, timeout=0.2)
time.sleep(0.2)
ser.reset_input_buffer()
print("Enviando 'S'...")
ser.write(b'S'); ser.flush()
t0 = time.time()
total = 0
while time.time() - t0 < 6.0:
    d = ser.read(64)
    if d:
        total += len(d)
        print(f"{time.time()-t0:5.2f} s  {len(d):2d} bytes  {d!r}")
print(f"Total recibido: {total} bytes (esperado ~ 6 s x 100 líneas x ~8 bytes = ~4800)")
ser.close()
