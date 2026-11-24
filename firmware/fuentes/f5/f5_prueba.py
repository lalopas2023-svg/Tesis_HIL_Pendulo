#!/usr/bin/env python3
"""F5: comandos por UART con la ISR activa.

Requisito: el DSP ya cargado con control_hil_f5 y esperando 'S' (DSLite load).
No usa CoppeliaSim. Cierra el puerto al terminar.

Fases (cada una de DUR s):
  A  base          sin comandos
  B  D a 10 Hz     mide latencia de la respuesta
  C  D a 100 Hz    una por trama (estres)
  D  basura        bytes desconocidos a alta tasa (sin 'D', 'S', CR, LF) + D al final
  E  D final       lee isr_max_cycles, overruns, etc. de todo el recorrido

Criterios (PASA / FALLA por fila):
  - tasa de tramas de telemetria 100 +- 2 por segundo en cada fase
  - 0 lineas invalidas (ni de telemetria ni de diagnostico mal formadas)
  - B: cada D tiene respuesta; latencia mediana < 10 ms, maxima < 25 ms
  - cmd_diag final del DSP == total de D enviadas (ningun byte perdido)
  - D: cmd_unk del DSP == bytes de basura enviados
  - isr_max_cycles < 2000, isr_overruns = 0, adc_timeouts = 0, rx_err = 0
"""
import random
import re
import sys
import time

import serial

PUERTO = 'COM10'
BAUD = 115200
DUR = 10.0
TASA_ESPERADA = 100.0

PAT_TEL = re.compile(r'^-?\d+,-?\d+,-?\d+$')
PAT_DIAG = re.compile(r'^D(,-?\d+){9}$')
CAMPOS = ['isr_max', 'overruns', 'adc_to', 'rx_err', 'tx_frames',
          'cal_ok', 'B_milli', 'cmd_diag', 'cmd_unk']


class Lector:
    def __init__(self, ser):
        self.ser = ser
        self.buf = bytearray()

    def lineas(self):
        """Devuelve [(t_llegada, texto)] de las lineas completas disponibles."""
        out = []
        n = self.ser.in_waiting
        if n:
            d = self.ser.read(n)
            t = time.perf_counter()
            self.buf += d
            while True:
                i = self.buf.find(b'\n')
                if i < 0:
                    break
                raw = bytes(self.buf[:i + 1])
                del self.buf[:i + 1]
                out.append((t, raw.decode('ascii', errors='replace').strip()))
            if len(self.buf) > 512:
                self.buf.clear()
        return out


def clasifica(lin):
    if PAT_TEL.match(lin):
        return 'tel'
    if PAT_DIAG.match(lin):
        return 'diag'
    if lin == 'OK':
        return 'ok'
    return 'mala'


def fase(nombre, ser, lec, dur, hz_d=0.0, basura_hz=0.0):
    """Corre una fase. Devuelve diccionario de resultados."""
    r = dict(nombre=nombre, tel=0, diag=[], malas=[], enviadas_d=0, basura=0,
             lat=[], t_dur=0.0)
    cola_envios = []                      # tiempos de envio de D esperando respuesta
    t0 = time.perf_counter()
    prox_d = t0
    prox_b = t0
    per_d = 1.0 / hz_d if hz_d > 0 else None
    per_b = 1.0 / basura_hz if basura_hz > 0 else None
    while True:
        ahora = time.perf_counter()
        if ahora - t0 >= dur:
            break
        if per_d and ahora >= prox_d:
            ser.write(b'D')
            r['enviadas_d'] += 1
            cola_envios.append(ahora)
            prox_d += per_d
        if per_b and ahora >= prox_b:
            # basura: nunca 'D', 'S', CR ni LF
            b = random.choice([c for c in range(256) if c not in (0x44, 0x53, 0x0D, 0x0A)])
            ser.write(bytes([b]))
            r['basura'] += 1
            prox_b += per_b
        for t, lin in lec.lineas():
            k = clasifica(lin)
            if k == 'tel':
                r['tel'] += 1
            elif k == 'diag':
                v = [int(x) for x in lin.split(',')[1:]]
                r['diag'].append(dict(zip(CAMPOS, v)))
                if cola_envios and hz_d <= 20:      # latencia solo sin coalescencia
                    r['lat'].append((t - cola_envios.pop(0)) * 1000.0)
            elif k == 'mala':
                r['malas'].append(lin[:60])
        time.sleep(0.0005)
    r['t_dur'] = time.perf_counter() - t0
    return r


def mediana(x):
    x = sorted(x)
    n = len(x)
    return 0.0 if n == 0 else (x[n // 2] if n % 2 else 0.5 * (x[n // 2 - 1] + x[n // 2]))


def main():
    random.seed(5)
    ser = serial.Serial(PUERTO, BAUD, timeout=0.01)
    time.sleep(0.2)
    ser.reset_input_buffer()
    lec = Lector(ser)
    try:
        # --- arranque ---
        ok = False
        for intento in range(3):
            ser.write(b'S')
            t0 = time.time()
            while time.time() - t0 < 2.0 and not ok:
                for _, lin in lec.lineas():
                    if clasifica(lin) in ('ok', 'tel'):
                        ok = True
                time.sleep(0.01)
            if ok:
                break
        if not ok:
            print('FALLA: el DSP no respondio a S (OK ni datos).')
            return 2
        print('DSP arranco. Corriendo fases de %.0f s...' % DUR)
        time.sleep(1.0)
        ser.reset_input_buffer()
        lec.buf.clear()

        res = []
        res.append(fase('A base', ser, lec, DUR))
        res.append(fase('B D 10 Hz', ser, lec, DUR, hz_d=10))
        res.append(fase('C D 100 Hz', ser, lec, DUR, hz_d=100))
        res.append(fase('D basura 2 kHz', ser, lec, DUR, basura_hz=2000))
        # D final, una sola, tras la basura (tambien comprueba que el parser sigue vivo)
        ser.write(b'D')
        res[-1]['enviadas_d'] += 1
        t_fin = time.perf_counter()
        while time.perf_counter() - t_fin < 1.0:
            for _, lin in lec.lineas():
                k = clasifica(lin)
                if k == 'diag':
                    v = [int(x) for x in lin.split(',')[1:]]
                    res[-1]['diag'].append(dict(zip(CAMPOS, v)))
                elif k == 'mala':
                    res[-1]['malas'].append(lin[:60])
                elif k == 'tel':
                    res[-1]['tel'] += 1
            time.sleep(0.0005)
        res[-1]['t_dur'] += time.perf_counter() - t_fin   # el tiempo de esta espera tambien cuenta
    finally:
        ser.close()

    # ---------------------------- evaluacion ----------------------------
    filas = []
    ok_todo = True

    def check(cond, txt):
        nonlocal ok_todo
        ok_todo = ok_todo and cond
        filas.append(('PASA ' if cond else 'FALLA') + '  ' + txt)

    for r in res:
        tasa = r['tel'] / r['t_dur']
        check(abs(tasa - TASA_ESPERADA) <= 2.0,
              '%-16s tasa de tramas %.1f /s (esperado 100 +-2)' % (r['nombre'], tasa))
        check(len(r['malas']) == 0,
              '%-16s lineas invalidas: %d %s' % (r['nombre'], len(r['malas']),
                                                  r['malas'][:3]))
    B, C, Dd = res[1], res[2], res[3]
    check(len(B['diag']) >= B['enviadas_d'] - 1,
          'B: respuestas %d de %d D enviadas (se tolera 1 en vuelo al cerrar la fase)'
          % (len(B['diag']), B['enviadas_d']))
    if B['lat']:
        check(mediana(B['lat']) < 10.0 and max(B['lat']) < 25.0,
              'B: latencia mediana %.1f ms, maxima %.1f ms (limites 10 / 25)'
              % (mediana(B['lat']), max(B['lat'])))
    else:
        check(False, 'B: sin mediciones de latencia')
    check(len(C['diag']) > 0, 'C: %d respuestas a %d D (se agrupan si llegan varias antes de contestar)'
          % (len(C['diag']), C['enviadas_d']))
    if Dd['diag']:
        ult = Dd['diag'][-1]
        total_d = sum(r['enviadas_d'] for r in res)
        check(ult['cmd_diag'] == total_d,
              'D: comandos D contados por el DSP %d, enviados %d (ninguno perdido)'
              % (ult['cmd_diag'], total_d))
        check(ult['cmd_unk'] == Dd['basura'],
              'D: bytes desconocidos contados %d, enviados %d' % (ult['cmd_unk'], Dd['basura']))
        check(ult['isr_max'] < 2000, 'E: isr_max_cycles %d (< 2000)' % ult['isr_max'])
        check(ult['overruns'] == 0, 'E: isr_overruns %d' % ult['overruns'])
        check(ult['adc_to'] == 0, 'E: adc_timeouts %d' % ult['adc_to'])
        check(ult['rx_err'] == 0, 'E: rx_errors %d' % ult['rx_err'])
        print('Ultimo diagnostico: ', ult)
    else:
        check(False, 'E: sin diagnostico final')

    print()
    for f in filas:
        print(f)
    print()
    print('RESULTADO F5:', 'PASA' if ok_todo else 'FALLA')
    return 0 if ok_todo else 1


if __name__ == '__main__':
    sys.exit(main())
