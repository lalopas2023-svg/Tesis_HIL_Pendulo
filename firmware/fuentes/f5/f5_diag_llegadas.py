#!/usr/bin/env python3
"""F5 (diagnostico de la latencia): como llegan los bytes de la telemetria al PC.

Por que existe. En la primera corrida de f5_prueba.py la latencia comando-respuesta
dio mediana 11.4 ms y maxima 20.0 ms. El DSP contesta en menos de ~5 ms (ver el
analisis en la bitacora), asi que el resto debe venir del camino USB-serie del PC.
Sospechoso: el temporizador de latencia del chip FTDI de la sonda XDS100v2 (16 ms
por omision): el chip no entrega los bytes al PC hasta juntar un paquete o hasta
que vence ese temporizador.

Que mide. Solo escucha (no envia 'D'): registra el instante en que llega cada
bloque de bytes al programa y su tamano. Con temporizador de 16 ms los bloques
llegan cada ~16 ms con ~25 bytes; con 1 ms llegan casi byte a byte (2 o 3 bloques
por trama, cada 10 ms).

Requisito: el DSP ya cargado (DSLite load). Si esta esperando, se le manda 'S'.
No usa CoppeliaSim. Cierra el puerto al terminar.
"""
import sys
import time

PUERTO = 'COM10'
BAUD = 115200
DUR = 10.0          # s de captura
DESCARTE = 1.0      # s iniciales que se ignoran


def percentil(x, p):
    x = sorted(x)
    if not x:
        return 0.0
    k = (len(x) - 1) * p / 100.0
    i = int(k)
    j = min(i + 1, len(x) - 1)
    return x[i] + (x[j] - x[i]) * (k - i)


def analiza(reg):
    """reg = [(t_llegada_s, n_bytes)] ordenado. Devuelve un diccionario con los resultados."""
    if len(reg) < 3:
        return None
    t0, t1 = reg[0][0], reg[-1][0]
    dur = t1 - t0
    gaps = [(reg[i][0] - reg[i - 1][0]) * 1000.0 for i in range(1, len(reg))]
    tam = [n for _, n in reg]
    bins = [('< 2 ms', 0, 2), ('2 a 6 ms', 2, 6), ('6 a 12 ms', 6, 12),
            ('12 a 20 ms', 12, 20), ('>= 20 ms', 20, 1e9)]
    hist = []
    for nombre, a, b in bins:
        c = sum(1 for g in gaps if a <= g < b)
        hist.append((nombre, c, 100.0 * c / len(gaps)))
    r = dict(bloques=len(reg), dur=dur, bloques_s=len(reg) / dur if dur > 0 else 0.0,
             bytes_s=sum(tam) / dur if dur > 0 else 0.0,
             gap_med=percentil(gaps, 50), gap_p10=percentil(gaps, 10),
             gap_p90=percentil(gaps, 90), gap_max=max(gaps),
             tam_med=percentil(tam, 50), tam_max=max(tam), hist=hist)
    # lectura orientativa (no es un criterio de la prueba)
    if 13.0 <= r['gap_med'] <= 19.0 and r['tam_med'] >= 18:
        r['lectura'] = ('cadencia ~16 ms con bloques de ~%d bytes: COMPATIBLE con el temporizador '
                        'de latencia de 16 ms (valor por omision de FTDI)' % round(r['tam_med']))
    elif r['bloques_s'] >= 150 and r['tam_med'] <= 12:
        r['lectura'] = ('bloques pequenos y frecuentes (%.0f /s): COMPATIBLE con un temporizador '
                        'de latencia de ~1 ms' % r['bloques_s'])
    else:
        r['lectura'] = 'patron intermedio o no identificado: pegar la salida completa para analizarla'
    return r


def captura(ser, dur):
    """Escucha dur segundos. Devuelve [(t_llegada, n_bytes)]."""
    reg = []
    t_fin = time.perf_counter() + dur
    while time.perf_counter() < t_fin:
        b = ser.read(1)                       # bloquea hasta que llega el primer byte (o 50 ms)
        if not b:
            continue
        t = time.perf_counter()
        extra = ser.in_waiting
        if extra:
            ser.read(extra)
        reg.append((t, 1 + extra))
    return reg


def main():
    import serial
    ser = serial.Serial(PUERTO, BAUD, timeout=0.05)
    try:
        time.sleep(0.2)
        ser.reset_input_buffer()
        hay = False
        for _ in range(3):
            t_ini = time.time()
            while time.time() - t_ini < 1.5 and not hay:
                if ser.read(1):
                    hay = True
            if hay:
                break
            ser.write(b'S')
        if not hay:
            print('FALLA: no llega telemetria (cargar de nuevo con DSLite y repetir).')
            return 2
        print('Telemetria presente. Capturando %.0f s (se ignora el primer %.0f s)...' % (DUR, DESCARTE))
        captura(ser, DESCARTE)
        ser.reset_input_buffer()
        reg = captura(ser, DUR)
    finally:
        ser.close()

    r = analiza(reg)
    if r is None:
        print('FALLA: muy pocos bloques recibidos.')
        return 2
    print()
    print('Bloques recibidos: %d en %.2f s  (%.1f bloques/s, %.0f bytes/s)'
          % (r['bloques'], r['dur'], r['bloques_s'], r['bytes_s']))
    print('Tamano del bloque: mediana %.0f bytes, maximo %d bytes' % (r['tam_med'], r['tam_max']))
    print('Separacion entre bloques: mediana %.1f ms | p10 %.1f | p90 %.1f | maxima %.1f'
          % (r['gap_med'], r['gap_p10'], r['gap_p90'], r['gap_max']))
    print('Histograma de la separacion entre bloques:')
    for nombre, c, pct in r['hist']:
        print('  %-11s %5d  (%5.1f %%)' % (nombre, c, pct))
    print()
    print('Lectura orientativa:', r['lectura'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
