"""Simulación fiel del lazo HIL del firmware (ISR a 100 kHz).

Modela: ADC 12 bit con ruido, DAC 12 bit con truncado/redondeo y un offset
de lazo L (en LSB) entre lo que escribe el DAC y lo que lee el ADC.
Uso:  python3 -I sim_lazo.py
"""
import math, random

Ts = 1e-5
KP, KI, KD = 5.0, 2.0, 0.15
UMIN, UMAX = -10.0, 10.0
TMIN, TMAX = -1.570796, 1.570796
RANGE = TMAX - TMIN
g, L_, m, b = 9.81, 0.3, 0.1, 0.005
J = m * L_ * L_
invJ, gL = 1.0 / J, g / L_
LSB_RAD = RANGE / 4095.0
SIGMA_LSB = 1.61e-3 / LSB_RAD          # ruido medido del ADC: 1.61 mrad


def corre(offset_lsb, redondeo_dac=False, cal_b=0.0, seconds=12.0, seed=1):
    rnd = random.Random(seed)
    theta, theta_dot = 0.1, 0.0
    prev_slow = 0.0
    dot_meas = dot_filt = 0.0
    integ = 0.0
    cnt = 0
    n = int(seconds / Ts)
    acc_adc = acc_dac = 0.0
    acc_theta = 0.0
    n_acc = 0
    t_acc0 = int(2.0 / Ts)             # promediar desde t = 2 s
    dac_code = 2048
    for k in range(n):
        # ---- ADC: lee el voltaje que dejó el DAC en el ciclo anterior ----
        adc = dac_code + offset_lsb + rnd.gauss(0.0, SIGMA_LSB)
        adc_raw = min(4095, max(0, int(adc + 0.5)))
        adc_corr = adc_raw - cal_b
        theta_m = TMIN + (adc_corr / 4095.0) * RANGE
        # ---- PID ----
        err = -theta_m
        integ += err * Ts
        integ = max(-2.0, min(2.0, integ))
        cnt += 1
        if cnt >= 1000:
            dot_meas = (theta_m - prev_slow) / 0.01
            prev_slow = theta_m
            cnt = 0
            dot_filt = 0.614 * dot_filt + 0.386 * dot_meas
        u = KP * err - KD * dot_filt + KI * integ
        u = max(UMIN, min(UMAX, u))
        # ---- planta Euler-Cromer ----
        td_new = theta_dot + Ts * (gL * theta - b * invJ * theta_dot + invJ * u)
        th_new = theta + Ts * td_new
        th_new = max(TMIN, min(TMAX, th_new))
        theta, theta_dot = th_new, td_new
        # ---- DAC ----
        frac = (theta - TMIN) / RANGE
        frac = max(0.0, min(1.0, frac))
        x = frac * 4095.0 + (0.5 if redondeo_dac else 0.0)
        dac_code = int(x)
        if k >= t_acc0:
            acc_adc += adc_raw
            acc_dac += dac_code
            acc_theta += theta
            n_acc += 1
    return acc_theta / n_acc, (acc_adc - acc_dac) / n_acc, theta_dot


if __name__ == "__main__":
    print(f"LSB = {LSB_RAD*1e3:.4f} mrad; sigma ADC = {SIGMA_LSB:.2f} LSB")
    print(f"código DAC para theta=0.014: {(0.014-TMIN)/RANGE*4095:.2f}")
    print()
    print("offset_lazo[LSB]  trunc/round   theta_medio[rad]  (adc-dac)_medio[LSB]")
    for off in (0, -10, -16, -17, -18, -19, -20):
        th, d, _ = corre(off)
        print(f"{off:>10}          trunc        {th:10.5f}        {d:8.2f}")
    print()
    for off in (0, -18):
        th, d, _ = corre(off, redondeo_dac=True)
        print(f"{off:>10}          round        {th:10.5f}        {d:8.2f}")
    print()
    print("Con calibración por software (cal_b = offset del lazo, redondeo DAC):")
    for off, cb in ((-18, -18.0), (-18, -17.0), (-18, -19.0), (-18, -18.25)):
        th, d, _ = corre(off, redondeo_dac=True, cal_b=cb)
        print(f"  offset={off} cal_b={cb:>6}: theta_medio = {th*1e3:7.3f} mrad")
