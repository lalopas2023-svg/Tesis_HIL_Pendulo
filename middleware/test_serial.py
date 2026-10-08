import serial, time

for baud in [57600, 115200, 38400]:
    print(f"\n=== Probando {baud} ===")
    try:
        ser = serial.Serial('COM10', baud, timeout=2)
        ser.reset_input_buffer()
        time.sleep(0.5)
        print(f"Bytes: {ser.in_waiting}")
        if ser.in_waiting > 0:
            raw = ser.read(min(200, ser.in_waiting))
            print(f"Datos: {raw[:100]!r}")
            print(f"Comas: {raw.count(b',')}")
            print(f"Saltos de línea: {raw.count(b'\\n')}")
        ser.close()
    except Exception as e:
        print(f"Error: {e}")