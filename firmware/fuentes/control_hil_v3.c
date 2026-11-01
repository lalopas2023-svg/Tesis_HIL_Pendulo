//###########################################################################
// control_hil_v3.c   (versión COMPLETA y corregida)
// HIL de péndulo invertido en el TMS320F28379D (LaunchPad), SIN perturbación,
// un solo núcleo (CPU1).
//
// Es tu firmware "con trigger" con cambios mínimos y justificados. PID, planta,
// derivada lenta, filtro y protocolo UART de 3 columnas quedan IGUAL, así que
// tu middleware de Python sigue siendo compatible.
//
// CAMBIOS respecto a tu versión (cada uno marcado con [v3-n] en el código):
//   [v3-1] AdcSetMode(): carga los trims de offset del ADC (advisory SPRZ412N).
//          Requiere que F2837xD_Adc.c esté en el proyecto.
//   [v3-2] SCILBAUD calculado con fórmula: 26 (tu 27 daba 111 607 baud, -3.1 %).
//   [v3-3] El DAC se REDONDEA (antes truncaba: sesgo de -0.5 LSB = -0.38 mrad).
//   [v3-4] Calibración por software del lazo DAC->ADC (ADC_CAL_B / ADC_CAL_M).
//          ADC_CAL_B es LO QUE CORRIGE tus 0.014 rad. Se ajusta con la PRUEBA 4.
//   [v3-5] La derivada lenta arranca con theta_prev_slow = primera medición.
//   [v3-6] Instrumentación (Watch): tiempo máximo de la ISR, overruns, timeouts
//          del ADC, offset_lsb, theta_mean_acc y b_sugerido (PRUEBA 4).
//   [v3-7] Telemetría redondeada y en 'long' (el 'int' de C28x es de 16 bits:
//          desbordaba si |valor| > 32.767).
//   [v3-8] Trigger 'S' robusto: máscara 0xFF, limpia errores de recepción,
//          reinicia TODO el estado (incluida la instrumentación), vacía la
//          bandera de envío y responde "OK\r\n". Opción USE_TRIGGER.
//   [v3-10] Telemetría SIN sprintf: formateador propio (fmt_long). Con sprintf la
//          tarjeta mandaba un flujo infinito de '9' y nunca terminaba la 1.ª trama
//          (tx_frames = 0 en Watch). Ver prueba con sniffer_serial.py.
//   [v3-11] Reloj: con el símbolo _LAUNCHXL_F28379D el SDK da SYSCLK=200 MHz; sin él,
//          100 MHz (tu medición: LSPCLK=25 MHz y 50 tramas/s). LSPCLK_HZ se elige solo.
//   [v3-12] DAC inicial = código de THETA_INIT (corrige el arranque falso de la derivada).
//   [v3-9] Retardos con DELAY_US() en lugar de bucles vacíos (el compilador
//          puede eliminar un bucle vacío y el tiempo cambia con la optimización).
//
// Quitado por innecesario: #define PI, buffer_serial[], el segundo TSS=1 y la
// escritura redundante de SCICTL2 y LOADMODE.
//###########################################################################

#include "F28x_Project.h"

// ========================= OPCIONES (edítalas aquí) =========================
#define USE_ADCSETMODE   1          // [v3-1] 1 = carga trims; 0 = como antes

// [DIAG] 0 = normal.  1 = la 3.a columna de la telemetría (que el CSV llama Control_u)
//        trae theta_medido - theta_real (error del lazo DAC->ADC, en rad).  SOLO para
//        diagnosticar el transitorio; vuelve a 0 para tomar datos normales.
#define DIAG_TELE_ERR    0

// [v3-8] 1 = espera el comando 'S' de Python antes de arrancar (modo normal con
//            middleware).
//        0 = arranca solo al cargar y correr (PRUEBA 4 con la vista Watch, sin Python).
#define USE_TRIGGER      1

// [v3-4] Calibración del lazo DAC->ADC:  adc_raw = ADC_CAL_M * codigo_dac + ADC_CAL_B
//        Se usa  theta_medido = (adc_raw - ADC_CAL_B)/ADC_CAL_M.
//        Identidad (1.0 / 0.0) = sin corrección. Con la PRUEBA 4 pon ADC_CAL_B
//        igual al valor de offset_lsb (≈ -17) y deja ADC_CAL_M = 1.0.
#define ADC_CAL_M        1.0f
#define ADC_CAL_B        6.1f

// [v3-2] UART.  LSPCLK depende del reloj del sistema que configura InitSysCtrl():
//   - Con el símbolo predefinido _LAUNCHXL_F28379D (cristal de 10 MHz, PLL x40/2):
//     SYSCLK = 200 MHz y LSPCLK = SYSCLK/4 = 50 MHz.
//   - SIN ese símbolo el SDK usa PLL para otro cristal: SYSCLK = 100 MHz,
//     LSPCLK = 25 MHz (es lo que mediste) y el Timer0 va a la mitad de velocidad.
#ifdef _LAUNCHXL_F28379D
  #define LSPCLK_HZ      50000000UL
#else
  #warning "Falta _LAUNCHXL_F28379D en Predefined Symbols: el reloj podria quedar en 100 MHz (verifica sysclk_mhz_reg en Watch)"
  #define LSPCLK_HZ      25000000UL
#endif
#define BAUD             115200UL
#define BRR_VAL          ((LSPCLK_HZ + 4UL * BAUD) / (8UL * BAUD) - 1UL)   // = 26 (25 MHz) o 53 (50 MHz)

// ============================ PARÁMETROS (igual que antes) ==================
#define THETA_INIT      0.1f            // condición inicial (rad)
#define Ts              0.00001f        // 10 us: paso de la planta y del PID
#define KP              5.0f
#define KI              2.0f
#define KD              0.15f
#define U_MIN           -10.0f
#define U_MAX           10.0f
#define THETA_MIN       -1.570796f      // -pi/2 <-> código ADC/DAC 0
#define THETA_MAX       1.570796f       // +pi/2 <-> código ADC/DAC 4095
#define THETA_RANGE     (THETA_MAX - THETA_MIN)
#define THETA_PER_LSB   (THETA_RANGE / 4095.0f)        // 0.7672 mrad por LSB
#define ADC_CAL_INV_M   (1.0f / ADC_CAL_M)

#define g               9.81f
#define L               0.3f
#define m               0.1f
#define b_fric          0.005f
#define J               (m * L * L)
#define invJ            (1.0f / J)
#define gL              (g / L)

// ============================ VARIABLES GLOBALES ============================
volatile float theta = THETA_INIT;            // estado REAL de la planta (rad)
volatile float theta_dot = 0.0f;
volatile float theta_prev_slow = 0.0f;
volatile float theta_dot_meas = 0.0f;
volatile float theta_dot_filt = 0.0f;
volatile float u = 0.0f;
volatile float theta_measured = 0.0f;   // lo que "ve" el controlador (vía ADC)
volatile float error_integral = 0.0f;
volatile int   contador_derivada = 0;
volatile int   flag_enviar = 0;
volatile int   contador_serial = 0;
volatile int   primera_vez = 1;         // [v3-5]
volatile Uint16 dac_code = 2048;        // último código escrito al DAC
volatile int   start_flag = 0;          // [v3-8] 1 = ya arrancó (también se puede poner a mano en Watch)
volatile Uint32 rx_errors = 0;          // [v3-8] errores de recepción SCI (informativo)
volatile float  sysclk_mhz_reg = 0.0f;  // [DIAG] SYSCLK leído de los registros (supone cristal de 10 MHz)
volatile float  lspclk_mhz_reg = 0.0f;  // [DIAG] LSPCLK leído de los registros
volatile Uint32 main_iter = 0;          // [DIAG] vueltas del bucle principal (debe crecer)
volatile Uint32 tx_frames = 0;          // [DIAG] tramas enviadas por UART (≈100 por segundo)

// [v3-6] Instrumentación (se leen en la vista Watch)
volatile Uint32 isr_max_cycles = 0;     // ciclos máximos de la ISR (límite: 2000)
volatile Uint32 isr_overruns   = 0;     // veces que la ISR duró más de 10 us
volatile Uint32 adc_timeouts   = 0;     // debe ser 0
#define N_ACC           1000000UL       // 10 s a 100 kHz
#define N_SETTLE        200000UL        // se ignoran los primeros 2 s
volatile Uint32 isr_count = 0;
volatile Uint32 sum_adc = 0, sum_dac = 0, n_acc = 0;
volatile float  sum_theta = 0.0f;
volatile Uint32 n_theta = 0;
volatile int    acc_on = 1;
volatile float  offset_lsb = 0.0f;      // RESULTADO: media(ADC - DAC) en LSB
volatile float  theta_mean_acc = 0.0f;  // RESULTADO: media de theta REAL (rad)
volatile float  b_sugerido = 0.0f;      // RESULTADO: valor sugerido para ADC_CAL_B

__interrupt void cpu_timer0_isr(void);

// [v3-7] mrad redondeado y SATURADO a +-999999 (evita desbordes y NaN).
//        Devuelve 'long' (32 bits): el 'int' de C28x es de 16 bits.
static long to_milli(float x)
{
    float y = x * 1000.0f;
    if (!(y == y))        return 0L;             // NaN
    if (y >  999999.0f)   y =  999999.0f;
    if (y < -999999.0f)   y = -999999.0f;
    return (long)(y + ((y >= 0.0f) ? 0.5f : -0.5f));
}

// [v3-10] Convierte un entero a texto SIN usar sprintf.
//         Escribe en p y devuelve cuántos caracteres escribió (máx. 8 con |v|<=999999).
//         Usa restas de potencias de 10: no hay divisiones ni librería de printf.
static int fmt_long(char *p, long v)
{
    static const unsigned long pot[7] = {1000000UL, 100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL};
    unsigned long a;
    int n = 0, k, started = 0;
    if (v < 0) { p[n++] = '-'; a = (unsigned long)(-v); }
    else       { a = (unsigned long)v; }
    for (k = 0; k < 7; k++) {
        int d = 0;
        while (a >= pot[k]) { a -= pot[k]; d++; }
        if (d != 0 || started || k == 6) { p[n++] = (char)('0' + d); started = 1; }
    }
    return n;
}

// ------------------------------- UART SCI-A --------------------------------
void InitSciaGpio_LaunchPad(void)
{
    EALLOW;
    GpioCtrlRegs.GPBPUD.bit.GPIO43   = 0;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO43 = 3;
    GpioCtrlRegs.GPBGMUX1.bit.GPIO43 = 3;
    GpioCtrlRegs.GPBMUX1.bit.GPIO43  = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO42   = 0;
    GpioCtrlRegs.GPBGMUX1.bit.GPIO42 = 3;
    GpioCtrlRegs.GPBMUX1.bit.GPIO42  = 3;
    EDIS;
}

void InitScia_Config(void)
{
    EALLOW;
    CpuSysRegs.PCLKCR7.bit.SCI_A = 1;
    EDIS;
    SciaRegs.SCICCR.all   = 0x0007;
    SciaRegs.SCICTL1.all  = 0x0003;
    SciaRegs.SCICTL2.all  = 0x0003;
    SciaRegs.SCIHBAUD.all = (Uint16)(BRR_VAL >> 8);     // [v3-2]
    SciaRegs.SCILBAUD.all = (Uint16)(BRR_VAL & 0xFF);   // [v3-2]
    SciaRegs.SCICTL1.all  = 0x0023;
}

// Lee un byte si hay uno; si no, devuelve -1. [v3-8]
static int SciReadByte(void)
{
    if (SciaRegs.SCIRXST.bit.RXERROR) {          // paridad/overrun/framing/break
        rx_errors++;
        SciaRegs.SCICTL1.bit.SWRESET = 0;        // reinicia el receptor
        SciaRegs.SCICTL1.bit.SWRESET = 1;
        return -1;
    }
    if (SciaRegs.SCIRXST.bit.RXRDY == 1)
        return (int)(SciaRegs.SCIRXBUF.all & 0x00FF);   // solo 8 bits de datos
    return -1;
}

static void SciSendString(const char *msg)
{
    while (*msg) {
        while (SciaRegs.SCICTL2.bit.TXRDY == 0) {}
        SciaRegs.SCITXBUF.all = (Uint16)(*msg++);
    }
}

// Pone TODO el estado en la condición inicial (nada queda de una corrida previa). [v3-8]
static void ResetEstado(void)
{
    theta = THETA_INIT;
    theta_dot = 0.0f;
    theta_prev_slow = THETA_INIT;
    theta_dot_meas = 0.0f;
    theta_dot_filt = 0.0f;
    error_integral = 0.0f;
    u = 0.0f;
    contador_derivada = 0;
    contador_serial = 0;
    flag_enviar = 0;
    primera_vez = 1;
    isr_count = 0;  isr_max_cycles = 0;  isr_overruns = 0;  adc_timeouts = 0;
    sum_adc = 0;  sum_dac = 0;  n_acc = 0;  sum_theta = 0.0f;  n_theta = 0;
    main_iter = 0;  tx_frames = 0;
    acc_on = 1;  offset_lsb = 0.0f;  theta_mean_acc = 0.0f;  b_sugerido = 0.0f;
    // [v3-12] El DAC debe arrancar en el código de THETA_INIT (no en 2048 = ángulo 0).
    //         Antes, la 1.ª lectura del ADC veía "0 rad" aunque la planta empezaba en 0.1 rad:
    //         la derivada lenta daba un salto falso (+2.96 rad/s) y el transitorio no
    //         coincidía con MATLAB. Se espera a que la salida del DAC se estabilice.
    dac_code = (Uint16)(((THETA_INIT - THETA_MIN) / THETA_RANGE) * 4095.0f + 0.5f);
    DacbRegs.DACVALS.all = dac_code;
    DELAY_US(200);
}

// ============================================================================
void main(void)
{
    InitSysCtrl();

    // [DIAG] ¿A qué frecuencia quedó realmente el reloj? (lectura de registros del PLL)
    {
        Uint16 imult = (Uint16)(ClkCfgRegs.SYSPLLMULT.all & 0x7F);
        Uint16 fmult = (Uint16)((ClkCfgRegs.SYSPLLMULT.all >> 8) & 0x03);
        Uint16 divs  = (Uint16)(ClkCfgRegs.SYSCLKDIVSEL.all & 0x3F);
        Uint16 lsp   = (Uint16)(ClkCfgRegs.LOSPCP.all & 0x07);
        float  sys   = 10.0f * ((float)imult + 0.25f * (float)fmult) / (divs ? 2.0f * (float)divs : 1.0f);
        sysclk_mhz_reg = sys;
        lspclk_mhz_reg = sys / (lsp ? 2.0f * (float)lsp : 1.0f);
    }

    InitGpio();
    DINT;
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

    InitSciaGpio_LaunchPad();
    InitScia_Config();

    // ------------------------------ ADC-A -----------------------------------
    EALLOW;
    CpuSysRegs.PCLKCR13.bit.ADC_A = 1;
    AdcaRegs.ADCCTL2.bit.PRESCALE = 6;
#if USE_ADCSETMODE
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);   // [v3-1]
    EALLOW;     // por si AdcSetMode() cerró la protección de registros (EDIS)
#endif
    AdcaRegs.ADCCTL1.bit.INTPULSEPOS = 1;
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;
    EDIS;
    DELAY_US(1000);

    EALLOW;
    AdcaRegs.ADCSOC0CTL.bit.CHSEL = 0;      // ADCINA0 (pin 30) -> theta
    AdcaRegs.ADCSOC0CTL.bit.ACQPS = 14;
    AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 0;
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0;
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;
    EDIS;

    // ------------------------------ DAC-B -----------------------------------
    EALLOW;
    CpuSysRegs.PCLKCR16.bit.DAC_B = 1;
    DacbRegs.DACCTL.bit.DACREFSEL = 1;
    DacbRegs.DACCTL.bit.LOADMODE = 0;
    DacbRegs.DACOUTEN.bit.DACOUTEN = 1;
    DacbRegs.DACVALS.all = 2048;
    EDIS;
    DELAY_US(1000);

    // ---------------------- Timer 0: ISR cada 10 us -------------------------
    InitCpuTimers();
    ConfigCpuTimer(&CpuTimer0, 200, 10);    // 200 MHz, 10 us = 2000 ciclos

    EALLOW;
    PieVectTable.TIMER0_INT = &cpu_timer0_isr;
    EDIS;

    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
    IER |= M_INT1;
    EINT;
    ERTM;
    CpuTimer0Regs.TCR.bit.TSS = 1;          // el timer queda detenido hasta arrancar

    // ---------------- Arranque: trigger 'S' o inmediato [v3-8] ------------------
#if USE_TRIGGER
    while (start_flag == 0) {
        int c = SciReadByte();
        if (c == 'S') break;                // también sale si pones start_flag=1 en Watch
    }
#endif
    ResetEstado();
    CpuTimer0Regs.TCR.bit.TRB = 1;          // recarga el contador
    CpuTimer0Regs.TCR.bit.TSS = 0;          // arranca el timer
    start_flag = 1;
#if USE_TRIGGER
    SciSendString("OK\r\n");               // confirmación a Python
#endif

    // ------------------- Bucle principal: telemetría UART -------------------
    for (;;) {
        main_iter++;                                       // [DIAG]
        if (flag_enviar == 1) {
            char trama[32];
            int  len = 0, i;
            len += fmt_long(trama + len, to_milli(theta));            // mrad
            trama[len++] = ',';
            len += fmt_long(trama + len, to_milli(theta_dot_filt));   // mrad/s
            trama[len++] = ',';
#if DIAG_TELE_ERR
            len += fmt_long(trama + len, to_milli(theta_measured - theta));   // [DIAG] error del lazo
#else
            len += fmt_long(trama + len, to_milli(u));                // milí-unidades
#endif
            trama[len++] = '\r';
            trama[len++] = '\n';
            for (i = 0; i < len; i++) {
                while (SciaRegs.SCICTL2.bit.TXRDY == 0) {}
                SciaRegs.SCITXBUF.all = (Uint16)trama[i];
            }
            flag_enviar = 0;
            tx_frames++;                                   // [DIAG]
        }
    }
}

// ============================================================================
// ISR del Timer 0 (cada 10 us): lee ADC -> PID -> avanza la planta -> escribe DAC
__interrupt void cpu_timer0_isr(void)
{
    // [v3-6] marca de tiempo de entrada (el timer cuenta HACIA ABAJO 1999..0)
    Uint32 t_start = CpuTimer0Regs.TIM.all;

    // --- 1) Lee el ADC (convierte lo que el DAC dejó en el pin) -------------
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
    AdcaRegs.ADCSOCFRC1.bit.SOC0 = 1;
    volatile int timeout = 1000;
    while ((AdcaRegs.ADCINTFLG.bit.ADCINT1 == 0) && (timeout > 0))
        timeout--;
    if (timeout == 0) adc_timeouts++;       // [v3-6]
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;

    // --- 2) Código -> ángulo medido, con calibración [v3-4] -----------------
    Uint16 adc_raw = AdcaResultRegs.ADCRESULT0;
    theta_measured = THETA_MIN +
        (((float)adc_raw - ADC_CAL_B) * ADC_CAL_INV_M) * THETA_PER_LSB;

    // [v3-5] primera pasada: evita el salto falso de la derivada
    if (primera_vez) {
        theta_prev_slow = theta_measured;
        primera_vez = 0;
    }

    // --- 3) PID --------------------------------------------------------------
    float error = 0.0f - theta_measured;

    error_integral += error * Ts;
    if (error_integral >  2.0f) error_integral =  2.0f;     // anti-windup
    if (error_integral < -2.0f) error_integral = -2.0f;

    contador_derivada++;
    if (contador_derivada >= 1000) {                        // cada 10 ms
        theta_dot_meas  = (theta_measured - theta_prev_slow) / 0.01f;
        theta_prev_slow = theta_measured;
        contador_derivada = 0;
        theta_dot_filt  = (1.0f - 0.386f) * theta_dot_filt + 0.386f * theta_dot_meas;
        if (acc_on && isr_count >= N_SETTLE) {              // [v3-6]
            sum_theta += theta;
            n_theta++;
        }
    }

    u = (KP * error) - (KD * theta_dot_filt) + (KI * error_integral);
    if (u > U_MAX) u = U_MAX;
    if (u < U_MIN) u = U_MIN;

    // --- 4) Planta (Euler-Cromer) --------------------------------------------
    float theta_dot_new = theta_dot + Ts * (gL * theta - (b_fric * invJ * theta_dot) + invJ * u);
    float theta_new     = theta + Ts * theta_dot_new;
    if (theta_new > THETA_MAX) theta_new = THETA_MAX;
    if (theta_new < THETA_MIN) theta_new = THETA_MIN;
    theta     = theta_new;
    theta_dot = theta_dot_new;

    // --- 5) Ángulo -> código DAC (lazo de vuelta) ----------------------------
    float theta_frac = (theta - THETA_MIN) / THETA_RANGE;
    if (theta_frac < 0.0f) theta_frac = 0.0f;
    if (theta_frac > 1.0f) theta_frac = 1.0f;
    dac_code = (Uint16)(theta_frac * 4095.0f + 0.5f);       // [v3-3] redondeo
    DacbRegs.DACVALS.all = dac_code;

    // --- 6) [v3-6] Acumuladores de la PRUEBA 4 --------------------------------
    if (isr_count < N_SETTLE) {
        isr_count++;
    } else if (acc_on) {
        sum_adc += adc_raw;
        sum_dac += dac_code;
        if (++n_acc >= N_ACC) {
            acc_on = 0;
            offset_lsb     = (float)((long)(sum_adc - sum_dac)) / (float)N_ACC;
            theta_mean_acc = sum_theta / (float)n_theta;
            // theta_real = (B - offset)*LSB  =>  B_nuevo = B_actual - theta_real/LSB
            b_sugerido = ADC_CAL_B - theta_mean_acc / THETA_PER_LSB;
        }
    }

    // --- 7) Bandera de envío UART cada 10 ms ---------------------------------
    contador_serial++;
    if (contador_serial >= 1000) {
        flag_enviar = 1;
        contador_serial = 0;
    }

    // [v3-6] duración de la ISR en ciclos de CPU
    Uint32 t_end = CpuTimer0Regs.TIM.all;
    if (t_end > t_start) {
        isr_overruns++;                     // el timer recargó: duró > 10 us
    } else {
        Uint32 cyc = t_start - t_end;
        if (cyc > isr_max_cycles) isr_max_cycles = cyc;
    }

    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}