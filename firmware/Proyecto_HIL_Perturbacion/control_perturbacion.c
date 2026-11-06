// ============================================================================
// control_perturbacion.c
// HIL de péndulo invertido en el TMS320F28379D (LaunchPad), CPU1, CON PERTURBACIÓN.
//
// BASE: tu firmware sin perturbación (v3, el que quedó con error estacionario de
// 0.2-0.5 mrad). PID, planta, derivada lenta, filtro, calibración del ADC,
// trigger 'S' e instrumentación son los MISMOS. Lo nuevo está marcado con [P-n]:
//
//   [P-1] MODO_PERT: 0 = sin perturbación | 1 = potenciómetro (ADCINA2, pin 29)
//                    | 2 = escalón programado (repetible, comparable con MATLAB).
//   [P-2] La perturbación d entra a la planta junto con u:  invJ*(u + d).
//   [P-3] El potenciómetro se lee cada 10 ms (no a 100 kHz), desfasado 5 ms de la
//         derivada y de la telemetría, para no alargar la ISR más pesada ni
//         tocar la calibración del canal theta (ADCINA0) en el 99.9 % de las muestras.
//   [P-4] SOC0 (theta) tiene prioridad alta y se espera a que terminen AMBAS
//         conversiones (ADCINT1 y ADCINT2) antes de leer resultados.
//   [P-5] Zona muerta central con continuidad + escala simétrica + filtro IIR.
//   [P-6] Telemetría de 4 columnas: pos_mrad, vel_mrad_s, u_milli, pert_milli.
//   [P-7] El acumulador de calibración (theta_mean_acc, b_sugerido) se reinicia si
//         hay perturbación activa: solo es válido con d = 0.
//   [CLK-1/2] Sin el simbolo _LAUNCHXL_F28379D ya NO compila (#error) y, ademas, si en
//          tiempo de ejecucion SYSCLK != 200 MHz el programa se detiene (clock_ok = 0).
//   [P-8] Contador u_sat_count: cuántas veces el control llegó a saturar.
// ============================================================================
#include "F28x_Project.h"

// ========================= OPCIONES (edítalas aquí) =========================
#define USE_ADCSETMODE   1          //  1 = carga trims del ADC
#define USE_TRIGGER      1          //  1 = espera la 'S' de Python

#define MODO_PERT        1          // 0 = sin perturbación | 1 = potenciómetro | 2 = escalón programado

// --- Perturbación (unidades de u: entra como invJ*(u + d)) -------------------
#define PERT_MAX         0.5f       // amplitud a fondo de escala del potenciómetro
#define PERT_ESCALON     0.1f       // [MODO 2] amplitud del escalón
#define PERT_T_ESCALON_S 2.0f       // [MODO 2] instante del escalón (s), resolución 10 ms

// --- Potenciómetro (MODO 1) ---------------------------------------------------
#define POT_CENTRO       2048       // código ADC con el que d = 0 (mídelo: pot_raw en Watch)
#define POT_RECORRIDO    2000       // códigos desde el centro hasta d = +-PERT_MAX
#define POT_ZONA_MUERTA  30         // códigos alrededor del centro en los que d = 0
#define POT_ALFA         0.386f     // IIR a 100 Hz de muestreo: fc = 10 Hz
#define ACQPS_POT        14         // ventana de adquisición del canal del potenciómetro
#define POT_CADA         1000       // se lee cada 1000 ISR = 10 ms
#define POT_FASE         500        // desfase respecto a la derivada/telemetría

#define ADC_CAL_M        1.0f
#define ADC_CAL_B        6.1f

// [CLK-1] Si falta el simbolo _LAUNCHXL_F28379D el proyecto NO COMPILA. Antes solo habia un
//         #warning y la tarjeta arrancaba a 100 MHz sin que nadie lo notara. Este simbolo no
//         lo usa este archivo sino F2837xD_SysCtrl.c (InitSysCtrl): con el simbolo configura
//         el PLL a 200 MHz; sin el, a 100 MHz. Por eso se exige a nivel de PROYECTO.
#ifndef _LAUNCHXL_F28379D
  #error "Falta _LAUNCHXL_F28379D. Agregalo en Project > Properties > Build > C2000 Compiler > Predefined Symbols, en TODAS las configuraciones (CPU1_RAM_DEBUG, CPU1_FLASH_DEBUG, ...), y haz Clean Project. Sin el, SYSCLK=100 MHz y el HIL no funciona."
#endif
#define SYSCLK_MHZ       200.0f     // reloj que EXIGE el diseno (Ts = 10 us = 2000 ciclos)
#define LSPCLK_HZ        50000000UL // SYSCLK/4
#define BAUD             115200UL
#define BRR_VAL          ((LSPCLK_HZ + 4UL * BAUD) / (8UL * BAUD) - 1UL)   // = 53 con LSPCLK = 50 MHz

// ============================ PARÁMETROS (iguales) ==========================
#define THETA_INIT      0.1f
#define Ts              0.00001f
#define KP              5.0f
#define KI              2.0f
#define KD              0.15f
#define U_MIN           -10.0f
#define U_MAX           10.0f
#define THETA_MIN       -1.570796f
#define THETA_MAX       1.570796f
#define THETA_RANGE     (THETA_MAX - THETA_MIN)
#define THETA_PER_LSB   (THETA_RANGE / 4095.0f)
#define ADC_CAL_INV_M   (1.0f / ADC_CAL_M)

#define g               9.81f
#define L               0.3f
#define m               0.1f
#define b_fric          0.005f
#define J               (m * L * L)
#define invJ            (1.0f / J)
#define gL              (g / L)

// ============================ VARIABLES GLOBALES ============================
volatile float theta = THETA_INIT;
volatile float theta_dot = 0.0f;
volatile float theta_prev_slow = 0.0f;
volatile float theta_dot_meas = 0.0f;
volatile float theta_dot_filt = 0.0f;
volatile float u = 0.0f;
volatile float theta_measured = 0.0f;
volatile float error_integral = 0.0f;
volatile int   contador_derivada = 0;
volatile int   flag_enviar = 0;
volatile int   contador_serial = 0;
volatile int   primera_vez = 1;
volatile Uint16 dac_code = 2048;
volatile int   start_flag = 0;
volatile Uint32 rx_errors = 0;
volatile Uint16 clock_ok = 0;                 // [CLK-2] 1 = SYSCLK leido de los registros = 200 MHz
volatile Uint32 error_reloj = 0;                // [CLK-2] crece solo si el reloj es incorrecto (programa detenido)
volatile float  sysclk_mhz_reg = 0.0f;
volatile float  lspclk_mhz_reg = 0.0f;
volatile Uint32 main_iter = 0;
volatile Uint32 tx_frames = 0;

// [P-1..P-5] Perturbación
volatile float  pert_filt = 0.0f;       // d que ENTRA a la planta (lo que se envía en la 4.ª columna)
volatile float  pert_cmd = 0.0f;        // d pedida por el potenciómetro, sin filtrar
volatile Uint16 pot_raw = 0;            // código crudo del potenciómetro (Watch: mídelo en el centro)
volatile int    contador_pot = POT_FASE;
volatile int    pot_primera = 1;        // fuerza una lectura en la 1.ª ISR (inicializa el filtro)
volatile int    muestra_tras_pot = 0;   // la ISR anterior convirtió el potenciómetro
volatile Uint32 t10 = 0;                // tiempo en unidades de 10 ms (MODO 2)
volatile Uint32 u_sat_count = 0;        // [P-8]
// [DIAG-ARRANQUE] primeras lecturas del ADC tras la 'S' (mirar en Watch como arreglo)
volatile Uint16 adc_first[8] = {0,0,0,0,0,0,0,0};
volatile Uint16 n_first = 0;
volatile float  theta_prev_inicial = 0.0f;   // valor con el que arranca la derivada lenta

// Instrumentación
volatile Uint32 isr_max_cycles = 0;
volatile Uint32 isr_overruns   = 0;
volatile Uint32 adc_timeouts   = 0;
#define N_ACC           1000000UL
#define N_SETTLE        200000UL
volatile Uint32 isr_count = 0;
volatile Uint32 sum_adc = 0, sum_dac = 0, n_acc = 0, n_off = 0;
volatile float  sum_theta = 0.0f;
volatile Uint32 n_theta = 0;
volatile int    acc_on = 1;
volatile float  offset_lsb = 0.0f;
volatile float  theta_mean_acc = 0.0f;
volatile float  b_sugerido = 0.0f;

__interrupt void cpu_timer0_isr(void);

static long to_milli(float x)
{
    float y = x * 1000.0f;
    if (!(y == y))        return 0L;
    if (y >  999999.0f)   y =  999999.0f;
    if (y < -999999.0f)   y = -999999.0f;
    return (long)(y + ((y >= 0.0f) ? 0.5f : -0.5f));
}

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

// [P-5] Código del potenciómetro -> perturbación (sin filtrar).
//   x = código - centro; dentro de la zona muerta d = 0; fuera, se resta la zona
//   muerta (así d crece desde 0 sin saltos) y se escala para que llegue a +-1 en
//   +-POT_RECORRIDO; se satura en +-1 y se multiplica por PERT_MAX.
static float pot_a_pert(Uint16 raw)
{
    float x = (float)((int)raw - POT_CENTRO);
    if      (x >  (float)POT_ZONA_MUERTA) x -= (float)POT_ZONA_MUERTA;
    else if (x < -(float)POT_ZONA_MUERTA) x += (float)POT_ZONA_MUERTA;
    else                                  x  = 0.0f;
    x = x / (float)(POT_RECORRIDO - POT_ZONA_MUERTA);
    if (x >  1.0f) x =  1.0f;
    if (x < -1.0f) x = -1.0f;
    return x * PERT_MAX;
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
    SciaRegs.SCIHBAUD.all = (Uint16)(BRR_VAL >> 8);
    SciaRegs.SCILBAUD.all = (Uint16)(BRR_VAL & 0xFF);
    SciaRegs.SCICTL1.all  = 0x0023;
}

static int SciReadByte(void)
{
    if (SciaRegs.SCIRXST.bit.RXERROR) {
        rx_errors++;
        SciaRegs.SCICTL1.bit.SWRESET = 0;
        SciaRegs.SCICTL1.bit.SWRESET = 1;
        return -1;
    }
    if (SciaRegs.SCIRXST.bit.RXRDY == 1)
        return (int)(SciaRegs.SCIRXBUF.all & 0x00FF);
    return -1;
}

static void SciSendString(const char *msg)
{
    while (*msg) {
        while (SciaRegs.SCICTL2.bit.TXRDY == 0) {}
        SciaRegs.SCITXBUF.all = (Uint16)(*msg++);
    }
}

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
    sum_adc = 0;  sum_dac = 0;  n_acc = 0;  n_off = 0;  sum_theta = 0.0f;  n_theta = 0;
    main_iter = 0;  tx_frames = 0;
    acc_on = 1;  offset_lsb = 0.0f;  theta_mean_acc = 0.0f;  b_sugerido = 0.0f;
    // [P] estado de la perturbación
    pert_filt = 0.0f;  pert_cmd = 0.0f;  pot_raw = 0;
    contador_pot = POT_FASE;  pot_primera = 1;  muestra_tras_pot = 0;
    t10 = 0;  u_sat_count = 0;
    { int i; for (i = 0; i < 8; i++) adc_first[i] = 0; }  n_first = 0;  theta_prev_inicial = 0.0f;
    // El DAC arranca en el código de THETA_INIT (corrección [v3-12]).
    dac_code = (Uint16)(((THETA_INIT - THETA_MIN) / THETA_RANGE) * 4095.0f + 0.5f);
    DacbRegs.DACVALS.all = dac_code;
    DELAY_US(200);
}

// ============================================================================
void main(void)
{
    InitSysCtrl();

    {
        Uint16 imult = (Uint16)(ClkCfgRegs.SYSPLLMULT.all & 0x7F);
        Uint16 fmult = (Uint16)((ClkCfgRegs.SYSPLLMULT.all >> 8) & 0x03);
        Uint16 divs  = (Uint16)(ClkCfgRegs.SYSCLKDIVSEL.all & 0x3F);
        Uint16 lsp   = (Uint16)(ClkCfgRegs.LOSPCP.all & 0x07);
        float  sys   = 10.0f * ((float)imult + 0.25f * (float)fmult) / (divs ? 2.0f * (float)divs : 1.0f);
        sysclk_mhz_reg = sys;
        lspclk_mhz_reg = sys / (lsp ? 2.0f * (float)lsp : 1.0f);
    }

    // [CLK-2] Guarda en tiempo de ejecucion: si el reloj no es 200 MHz, NO se arranca nada.
    //         (cubre casos que el #error no ve: otro cristal, PLL mal configurado, etc.)
    //         El programa queda detenido aqui; Python no recibira "OK" y avisara.
    clock_ok = (sysclk_mhz_reg > (SYSCLK_MHZ - 1.0f) && sysclk_mhz_reg < (SYSCLK_MHZ + 1.0f)) ? 1 : 0;
    if (!clock_ok) {
        DINT;
        for (;;) { error_reloj++; }
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
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);
    EALLOW;
#endif
    AdcaRegs.ADCCTL1.bit.INTPULSEPOS = 1;
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;
    EDIS;
    DELAY_US(1000);

    EALLOW;
    AdcaRegs.ADCSOC0CTL.bit.CHSEL = 0;      // ADCINA0 (pin 30) -> theta   (sin cambios)
    AdcaRegs.ADCSOC0CTL.bit.ACQPS = 14;
    AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 0;
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0;  // ADCINT1 = fin de SOC0
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;
#if MODO_PERT == 1
    AdcaRegs.ADCSOC1CTL.bit.CHSEL = 2;      // ADCINA2 (pin 29) -> potenciómetro
    AdcaRegs.ADCSOC1CTL.bit.ACQPS = ACQPS_POT;
    AdcaRegs.ADCSOC1CTL.bit.TRIGSEL = 0;    // solo por software
    AdcaRegs.ADCINTSEL1N2.bit.INT2SEL = 1;  // ADCINT2 = fin de SOC1
    AdcaRegs.ADCINTSEL1N2.bit.INT2E = 1;
    AdcaRegs.ADCSOCPRICTL.bit.SOCPRIORITY = 1;   // [P-4] SOC0 siempre primero
#endif
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
    ConfigCpuTimer(&CpuTimer0, SYSCLK_MHZ, 10);

    EALLOW;
    PieVectTable.TIMER0_INT = &cpu_timer0_isr;
    EDIS;

    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
    IER |= M_INT1;
    EINT;
    ERTM;
    CpuTimer0Regs.TCR.bit.TSS = 1;

#if USE_TRIGGER
    while (start_flag == 0) {
        int c = SciReadByte();
        if (c == 'S') break;
    }
#endif
    ResetEstado();
    CpuTimer0Regs.TCR.bit.TRB = 1;
    CpuTimer0Regs.TCR.bit.TSS = 0;
    start_flag = 1;
#if USE_TRIGGER
    SciSendString("OK\r\n");
#endif

    // ------------------- Bucle principal: telemetría UART -------------------
    for (;;) {
        main_iter++;
        if (flag_enviar == 1) {
            char trama[48];                      // [P-6] 4 columnas: caben hasta 37 caracteres
            int  len = 0, i;
            len += fmt_long(trama + len, to_milli(theta));            // mrad
            trama[len++] = ',';
            len += fmt_long(trama + len, to_milli(theta_dot_filt));   // mrad/s
            trama[len++] = ',';
            len += fmt_long(trama + len, to_milli(u));                // milí-unidades
            trama[len++] = ',';
            len += fmt_long(trama + len, to_milli(pert_filt));        // [P-6] perturbación aplicada (milí)
            trama[len++] = '\r';
            trama[len++] = '\n';
            for (i = 0; i < len; i++) {
                while (SciaRegs.SCICTL2.bit.TXRDY == 0) {}
                SciaRegs.SCITXBUF.all = (Uint16)trama[i];
            }
            flag_enviar = 0;
            tx_frames++;
        }
    }
}

// ============================================================================
__interrupt void cpu_timer0_isr(void)
{
    Uint32 t_start = CpuTimer0Regs.TIM.all;

    // --- 0) ¿Esta ISR lee también el potenciómetro? [P-3] --------------------
    int leer_pot = 0;
    int tras_pot = muestra_tras_pot;        // ¿la conversión anterior fue del potenciómetro?
#if MODO_PERT != 0
    if (++contador_pot >= POT_CADA) {       // cada 10 ms (desfasado 5 ms de la derivada)
        contador_pot = 0;
        t10++;                              // reloj de 10 ms (lo usa el MODO 2)
        leer_pot = 1;
    }
    if (pot_primera) leer_pot = 1;          // 1.ª ISR: inicializa el filtro
#endif
#if MODO_PERT != 1
    leer_pot = 0;                           // sin potenciómetro no se convierte el canal A2
#endif

    // --- 1) Lee el ADC --------------------------------------------------------
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
#if MODO_PERT == 1
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT2 = 1;
    if (leer_pot) AdcaRegs.ADCSOCFRC1.all = 0x0003;     // SOC0 (theta) y SOC1 (pot)
    else          AdcaRegs.ADCSOCFRC1.all = 0x0001;     // solo SOC0
    volatile int timeout = 3000;
    while ((AdcaRegs.ADCINTFLG.bit.ADCINT1 == 0) && (timeout > 0)) timeout--;
    if (leer_pot) {                                      // [P-4] esperar también el fin de SOC1
        while ((AdcaRegs.ADCINTFLG.bit.ADCINT2 == 0) && (timeout > 0)) timeout--;
    }
    if (timeout == 0) adc_timeouts++;
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT2 = 1;
#else
    AdcaRegs.ADCSOCFRC1.bit.SOC0 = 1;
    volatile int timeout = 1000;
    while ((AdcaRegs.ADCINTFLG.bit.ADCINT1 == 0) && (timeout > 0)) timeout--;
    if (timeout == 0) adc_timeouts++;
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
#endif

    // --- 2) Código -> ángulo medido (calibrado) ------------------------------
    Uint16 adc_raw = AdcaResultRegs.ADCRESULT0;
    theta_measured = THETA_MIN +
        (((float)adc_raw - ADC_CAL_B) * ADC_CAL_INV_M) * THETA_PER_LSB;

    if (n_first < 8) adc_first[n_first++] = adc_raw;       // [DIAG-ARRANQUE]

    if (primera_vez) {
        theta_prev_slow = theta_measured;
        theta_prev_inicial = theta_measured;                // [DIAG-ARRANQUE]
        primera_vez = 0;
    }

    // --- 2b) Perturbación [P-1][P-5] -----------------------------------------
#if MODO_PERT == 1
    if (leer_pot) {
        pot_raw  = AdcaResultRegs.ADCRESULT1;
        pert_cmd = pot_a_pert(pot_raw);
        if (pot_primera) { pert_filt = pert_cmd; pot_primera = 0; }      // sin rampa inicial
        else             { pert_filt = (1.0f - POT_ALFA) * pert_filt + POT_ALFA * pert_cmd; }
    }
#elif MODO_PERT == 2
    if (leer_pot == 0 && contador_pot == 0) {            // tick de 10 ms (t10 ya incrementó)
        pert_cmd  = ((float)t10 >= PERT_T_ESCALON_S * 100.0f) ? PERT_ESCALON : 0.0f;
        pert_filt = pert_cmd;                            // el escalón no se filtra
    }
    pot_primera = 0;
#else
    pert_filt = 0.0f;
    pot_primera = 0;
#endif
    muestra_tras_pot = leer_pot;

    // --- 3) PID (igual que antes) --------------------------------------------
    float error = 0.0f - theta_measured;

    error_integral += error * Ts;
    if (error_integral >  2.0f) error_integral =  2.0f;
    if (error_integral < -2.0f) error_integral = -2.0f;

    contador_derivada++;
    if (contador_derivada >= 1000) {
        theta_dot_meas  = (theta_measured - theta_prev_slow) / 0.01f;
        theta_prev_slow = theta_measured;
        contador_derivada = 0;
        theta_dot_filt  = (1.0f - 0.386f) * theta_dot_filt + 0.386f * theta_dot_meas;
        if (acc_on && isr_count >= N_SETTLE) {
            sum_theta += theta;
            n_theta++;
        }
    }

    u = (KP * error) - (KD * theta_dot_filt) + (KI * error_integral);
    if (u > U_MAX) { u = U_MAX; u_sat_count++; }            // [P-8]
    if (u < U_MIN) { u = U_MIN; u_sat_count++; }

    // --- 4) Planta (Euler-Cromer) con perturbación [P-2] ----------------------
    float theta_dot_new = theta_dot + Ts * (gL * theta - (b_fric * invJ * theta_dot)
                                            + invJ * (u + pert_filt));
    float theta_new     = theta + Ts * theta_dot_new;
    if (theta_new > THETA_MAX) theta_new = THETA_MAX;
    if (theta_new < THETA_MIN) theta_new = THETA_MIN;
    theta     = theta_new;
    theta_dot = theta_dot_new;

    // --- 5) Ángulo -> DAC -----------------------------------------------------
    float theta_frac = (theta - THETA_MIN) / THETA_RANGE;
    if (theta_frac < 0.0f) theta_frac = 0.0f;
    if (theta_frac > 1.0f) theta_frac = 1.0f;
    dac_code = (Uint16)(theta_frac * 4095.0f + 0.5f);
    DacbRegs.DACVALS.all = dac_code;

    // --- 6) Acumuladores de calibración (solo válidos con d = 0) [P-7] --------
    {
        float ap = (pert_filt < 0.0f) ? -pert_filt : pert_filt;
        if (acc_on && ap > 0.001f) {                         // hay perturbación: reinicia la ventana
            isr_count = 0;  n_acc = 0;  n_off = 0;
            sum_adc = 0;  sum_dac = 0;  sum_theta = 0.0f;  n_theta = 0;
        }
    }
    if (isr_count < N_SETTLE) {
        isr_count++;
    } else if (acc_on) {
        if (!tras_pot) {                                     // excluye la muestra posterior a convertir el pot
            sum_adc += adc_raw;
            sum_dac += dac_code;
            n_off++;
        }
        if (++n_acc >= N_ACC) {
            acc_on = 0;
            offset_lsb     = (n_off > 0UL) ? (float)((long)(sum_adc - sum_dac)) / (float)n_off : 0.0f;
            theta_mean_acc = (n_theta > 0UL) ? sum_theta / (float)n_theta : 0.0f;
            b_sugerido = ADC_CAL_B - theta_mean_acc / THETA_PER_LSB;
        }
    }

    // --- 7) Bandera de envío UART cada 10 ms ---------------------------------
    contador_serial++;
    if (contador_serial >= 1000) {
        flag_enviar = 1;
        contador_serial = 0;
    }

    Uint32 t_end = CpuTimer0Regs.TIM.all;
    if (t_end > t_start) {
        isr_overruns++;
    } else {
        Uint32 cyc = t_start - t_end;
        if (cyc > isr_max_cycles) isr_max_cycles = cyc;
    }

    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
