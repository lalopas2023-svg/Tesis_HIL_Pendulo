//###########################################################################
// diag_adc_dac.c
// FIRMWARE DE DIAGNÓSTICO del lazo  DAC-B (pin 70)  ->  cable  ->  ADC-A0 (pin 30)
//
// Objetivo: medir, SIN controlador ni planta, qué lee el ADC cuando el DAC
// escribe un código conocido. La diferencia (ADC - DAC) es el "offset del lazo"
// en LSB. Con tu mapeo 1 LSB = 0.767 mrad, así que 18 LSB = 0.014 rad.
//
// Qué hace, en orden:
//   1) Configura reloj, ADC-A, DAC-B y SCI-A (UART) igual que tu firmware.
//   2) PRUEBAS 1 y 2: mide 4 puntos fijos (FIX): media y desv. estándar.
//   3) PRUEBA 3: barrido del DAC de 0 a 4095 (65 puntos, SW).
//   4) Se detiene en ESTOP0 para que leas las variables en la vista Watch.
//      Pulsa Resume (F8) y el programa vuelca las tablas por UART cada 3 s.
//
// Cómo usarlo: ver la guía. Cambia USE_ADCSETMODE (0 o 1), recompila y repite.
//###########################################################################

#include "F28x_Project.h"
#include <stdio.h>
#include <math.h>

// ========================= OPCIONES (edítalas aquí) =========================
#define USE_ADCSETMODE   0          // 0 = igual que tu firmware actual
                                    // 1 = llama a AdcSetMode() (carga trims)
#define HALT_AFTER_MEASURE 1        // 1 = ESTOP0 antes de volcar por UART
#define N_AVG            10000L     // muestras promediadas en cada punto

// --- UART: 115200 baudios con LSPCLK = 25 MHz (medido por ti: SCILBAUD=53
//     daba 57 600 baud, o sea LSPCLK = 25 MHz y no 50 MHz) ------------------
#define LSPCLK_HZ        25000000UL
#define BAUD             115200UL
// BRR = redondeo( LSPCLK / (8*BAUD) ) - 1  = redondeo(27.13) - 1 = 26
// (el +4*BAUD dentro del paréntesis hace el redondeo con enteros)
#define BRR_VAL          ((LSPCLK_HZ + 4UL * BAUD) / (8UL * BAUD) - 1UL)

// ============================ RESULTADOS (Watch) ============================
#define N_FIX 4
#define N_SW  65
const Uint16 fix_code[N_FIX] = {1950, 2048, 2066, 2150};
volatile float  fix_mean[N_FIX];
volatile float  fix_sd[N_FIX];
volatile Uint16 sw_code[N_SW];
volatile float  sw_mean[N_SW];
volatile float  sw_sd[N_SW];
volatile Uint32 adc_timeouts = 0;   // debe quedar en 0

// ------------------------------ prototipos ---------------------------------
static Uint16 read_adc(void);
static void   measure(Uint16 code, volatile float *mean, volatile float *sd);
static void   scia_gpio_init(void);
static void   scia_init(void);
static void   scia_puts(const char *s);
static void   dump_tables(void);

// ============================================================================
void main(void)
{
    int i;

    // --- 1) Reloj del sistema (PLL a 200 MHz) y pines por defecto -----------
    InitSysCtrl();
    InitGpio();
    DINT;                       // sin interrupciones en este programa

    // --- 2) UART (SCI-A, GPIO42 TX / GPIO43 RX en el LaunchPad) -------------
    scia_gpio_init();
    scia_init();

    // --- 3) ADC-A -----------------------------------------------------------
    EALLOW;
    CpuSysRegs.PCLKCR13.bit.ADC_A = 1;      // reloj del módulo ADC-A
    AdcaRegs.ADCCTL2.bit.PRESCALE = 6;      // ADCCLK = SYSCLK/4 = 50 MHz
#if USE_ADCSETMODE
    // TI (SPRZ412N, advisory "ADC Offset Trim in Different Modes"): al
    // arrancar NO se cargan los trims de offset/linealidad; AdcSetMode() los
    // carga. Requiere F2837xD_Adc.c en el proyecto.
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);
    EALLOW;     // por si AdcSetMode() cerró la protección de registros (EDIS)
#endif
    AdcaRegs.ADCCTL1.bit.INTPULSEPOS = 1;   // flag de fin de conversión al final
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;      // energiza el ADC
    EDIS;
    DELAY_US(1000);                         // el ADC necesita >= 500 us

    EALLOW;
    AdcaRegs.ADCSOC0CTL.bit.CHSEL = 0;      // SOC0 convierte ADCINA0 (pin 30)
    AdcaRegs.ADCSOC0CTL.bit.ACQPS = 14;     // ventana de muestreo: 15 ciclos
    AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 0;    // disparo por software (forzado)
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0;  // INT1 se activa al terminar SOC0
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;
    EDIS;

    // --- 4) DAC-B -----------------------------------------------------------
    EALLOW;
    CpuSysRegs.PCLKCR16.bit.DAC_B = 1;      // reloj del DAC-B
    DacbRegs.DACCTL.bit.DACREFSEL = 1;      // referencia = VREFHI del ADC
    DacbRegs.DACCTL.bit.LOADMODE = 0;       // el valor se carga al escribir
    DacbRegs.DACOUTEN.bit.DACOUTEN = 1;     // habilita la salida (pin 70)
    DacbRegs.DACVALS.all = 2048;
    EDIS;
    DELAY_US(1000);                         // el buffer del DAC tarda en energizar

    // --- 5) PRUEBAS 1 y 2: puntos fijos -------------------------------------
    for (i = 0; i < N_FIX; i++)
        measure(fix_code[i], &fix_mean[i], &fix_sd[i]);

    // --- 6) PRUEBA 3: barrido 0..4095 en pasos de 64 (último punto 4095) ----
    for (i = 0; i < N_SW; i++) {
        sw_code[i] = (i == (N_SW - 1)) ? 4095 : (Uint16)(i * 64);
        measure(sw_code[i], &sw_mean[i], &sw_sd[i]);
    }

#if HALT_AFTER_MEASURE
    asm(" ESTOP0");     // aquí lee fix_mean[], fix_sd[], sw_mean[] en Watch
#endif

    // --- 7) Volcado por UART, repetido para que puedas abrir la terminal
    //        cuando quieras -------------------------------------------------
    for (;;) {
        dump_tables();
        DELAY_US(3000000L);
    }
}

// ============================================================================
// Una conversión del ADC por software. Devuelve el código de 12 bits (0..4095).
static Uint16 read_adc(void)
{
    volatile Uint32 timeout = 20000UL;

    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;  // limpia flag viejo
    AdcaRegs.ADCSOCFRC1.bit.SOC0 = 1;       // fuerza el inicio de SOC0
    while (AdcaRegs.ADCINTFLG.bit.ADCINT1 == 0 && timeout > 0)
        timeout--;                          // espera fin de conversión
    if (timeout == 0) adc_timeouts++;       // (no debería pasar)
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
    return AdcaResultRegs.ADCRESULT0;
}

// Escribe 'code' en el DAC, espera, toma N_AVG muestras del ADC y calcula
// media y desviación estándar con el algoritmo de Welford (estable en float).
static void measure(Uint16 code, volatile float *mean, volatile float *sd)
{
    float m = 0.0f, M2 = 0.0f;
    long  k;

    DacbRegs.DACVALS.all = code;
    DELAY_US(1000);                         // asentamiento del buffer del DAC
    for (k = 1; k <= N_AVG; k++) {
        float x = (float)read_adc();
        float d = x - m;
        m  += d / (float)k;
        M2 += d * (x - m);
    }
    *mean = m;
    *sd   = sqrtf(M2 / (float)(N_AVG - 1));
}

// ------------------------------- UART --------------------------------------
static void scia_gpio_init(void)
{
    EALLOW;
    GpioCtrlRegs.GPBPUD.bit.GPIO43   = 0;   // RX con pull-up
    GpioCtrlRegs.GPBQSEL1.bit.GPIO43 = 3;   // RX asíncrono
    GpioCtrlRegs.GPBGMUX1.bit.GPIO43 = 3;   // función SCIRXDA
    GpioCtrlRegs.GPBMUX1.bit.GPIO43  = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO42   = 0;
    GpioCtrlRegs.GPBGMUX1.bit.GPIO42 = 3;   // función SCITXDA
    GpioCtrlRegs.GPBMUX1.bit.GPIO42  = 3;
    EDIS;
}

static void scia_init(void)
{
    EALLOW;
    CpuSysRegs.PCLKCR7.bit.SCI_A = 1;       // reloj del SCI-A
    EDIS;
    SciaRegs.SCICCR.all   = 0x0007;         // 8 bits, 1 stop, sin paridad
    SciaRegs.SCICTL1.all  = 0x0003;         // habilita TX y RX (aún en reset)
    SciaRegs.SCICTL2.all  = 0x0003;
    SciaRegs.SCIHBAUD.all = (Uint16)(BRR_VAL >> 8);     // byte alto de BRR
    SciaRegs.SCILBAUD.all = (Uint16)(BRR_VAL & 0xFF);   // byte bajo de BRR
    SciaRegs.SCICTL1.all  = 0x0023;         // sale de reset
}

static void scia_puts(const char *s)
{
    while (*s) {
        while (SciaRegs.SCICTL2.bit.TXRDY == 0) {}
        SciaRegs.SCITXBUF.all = *s++;
    }
}

// Formato (media y sd multiplicadas por 100, para no usar %f en la C28x):
//   FIX,<codigo_dac>,<media_adc*100>,<sd_adc*100>
//   SW,<codigo_dac>,<media_adc*100>,<sd_adc*100>
static void dump_tables(void)
{
    char buf[64];
    int  i;

    sprintf(buf, "# diag USE_ADCSETMODE=%d N_AVG=%ld timeouts=%ld\r\n",
            (int)USE_ADCSETMODE, (long)N_AVG, (long)adc_timeouts);
    scia_puts(buf);
    for (i = 0; i < N_FIX; i++) {
        sprintf(buf, "FIX,%u,%ld,%ld\r\n", fix_code[i],
                (long)(fix_mean[i] * 100.0f), (long)(fix_sd[i] * 100.0f));
        scia_puts(buf);
    }
    for (i = 0; i < N_SW; i++) {
        sprintf(buf, "SW,%u,%ld,%ld\r\n", sw_code[i],
                (long)(sw_mean[i] * 100.0f), (long)(sw_sd[i] * 100.0f));
        scia_puts(buf);
    }
    scia_puts("# END\r\n");
}
