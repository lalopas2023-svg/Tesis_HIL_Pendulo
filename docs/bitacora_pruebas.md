# Bitácora de pruebas - Tesis HIL

## Sesión 2026-10-04

### Prueba 1: Verificación de baud rate (método: conteo de filas)
- **Fecha:** 2026-10-04
- **Objetivo:** Confirmar baud rate real del DSP
- **Método:** Conteo de filas del CSV en 33 s
- **Resultado:**
  - Baud rate real: **57,600 bps** (no 115,200 como decía el comentario)
  - Filas del CSV: 3342 en 33.4 s → 100 Hz exactos
  - Cero excepciones, cero bytes corruptos
- **Causa del error anterior:** Python leía a 115,200 (el doble). LSPCLK del DSP = 25 MHz, no 50 MHz. `SCILBAUD=53` da 57,600, no 115,200.
- **Acción:** Cambié `BAUD_RATE = 57600` en `hil_coppelia.py`
- **Evidencia:** `docs/capturas/baudrate_ok.png`
- **Conclusión:** Enlace serial verificado. Los datos a partir de ahora son fiables.

---

## Sesión 2026-10-08

### Prueba 1b: Verificación del baud rate con software de puerto serial
- **Fecha:** 2026-10-08
- **Objetivo:** Confirmar empíricamente el baud rate del SCI-A mediante
  una herramienta de monitoreo independiente del middleware propio.
- **Método intentado primero:** Medición con osciloscopio en los pines
  GPIO42/GPIO43 del LaunchPad (SCI-A).
- **Problema encontrado:** Los pines del SCI-A están ruteados al
  emulador XDS100v2 para el puerto COM virtual y NO están accesibles
  en los headers de expansión. Todas las mediciones dieron ruido.
- **Alternativa evaluada:** Migrar a SCI-B (GPIO18/GPIO19), pero requiere
  adaptador USB-a-Serial externo, que no está disponible.
- **Método finalmente aplicado:** Software de monitoreo de puerto serial
  (distinto al middleware `hil_coppelia.py`) en `COM10`.
- **Procedimiento:**
  1. Configuración a 115,200 bps → recepción de bytes sin sentido
  2. Configuración a 57,600 bps → tramas válidas:
     `"39,-8,224"`, `"39,54,207"`, `"39,62,202"`, `"39,-79,239"`, `"39,-19,226"`
  3. Todas las tramas con formato de 3 columnas separadas por comas
  4. Aparecieron dos advertencias:
     - "invalid number of channels": la app espera más columnas para graficar
     - "Port error: resource unavailable": conflicto con CCS en modo debug
- **Conclusión:** El SCI-A envía a 57,600 bps. Confirmado empíricamente
  por software independiente. El LSPCLK real es 25 MHz, no 50 MHz.
- **Nota:** La verificación con osciloscopio queda pendiente hasta adquirir
  un adaptador USB-Serial para migrar a SCI-B.

### Prueba 2: Diagnóstico de ruido del ADC
- **Fecha:** 2026-10-04
- **Objetivo:** Medir σ del ADC con entrada fija
- **Método:** Firmware de diagnóstico (solo lectura ADC), DAC fijo en 2048
- **Resultado:** σ = 1.610 mrad, pico a pico = 16 mrad, N = 6399
- **Conclusión:** Ruido real del ADC medido. Sirve para calcular
  amplificación en derivada.

### Prueba 3: Corrección de la derivada
- **Fecha:** 2026-10-04
- **Cambios en firmware:**
  1. Derivada a Ts=10 ms (cada 1000 ISRs) en lugar de Ts=10 µs
  2. Filtro IIR con α=0.386 (fc=10 Hz) dentro del bloque de 10 ms
  3. Eliminación de `theta_prev = theta_measured;`
- **Resultado:** Reducción de ruido de ~100× en velocidad.
- **Conclusión:** La derivada a Ts pequeño era la causa raíz del ruido.

### Prueba 4: Sistema completo con perturbaciones
- **Fecha:** 2026-10-04
- **Resultado:** Posición converge y rechaza perturbaciones. Velocidad
  interpretable. Control no satura.
- **Conclusión:** El lazo HIL funciona correctamente.

### Prueba 5: Comparación DSP vs MATLAB
- **Fecha:** 2026-10-05
- **Método:** Simulación del mismo lazo cerrado en MATLAB con Ts=10 µs,
  misma derivada lenta, mismas ganancias.
- **Resultados:**
  - Sin offset: error RMS posición = 0.00303 rad
  - Con offset modelado (-0.014 rad): error RMS = 0.00087 rad (< 2 LSB)
- **Conclusión:** El algoritmo del DSP coincide con el modelo teórico
  a nivel de cuantización del ADC. La discrepancia era hardware.

### Pendientes para la siguiente sesión
- [ ] Caracterización formal del DAC por barrido (0 a 4095)
- [ ] Caracterización del ADC con fuente calibrada
- [ ] Depuración del protocolo UART extendido a 4 columnas
---

## Sesión 2026-10-31 / 2026-11-01

**Objetivo de la sesión:** que el modelo *sin perturbación, un solo núcleo (CPU1)* converja a 0 rad
(el ángulo real se estabilizaba en ≈ 0.014 rad) y dejar la cadena DSP → UART → Python → CoppeliaSim/MATLAB
robusta y verificada. **Resultado: logrado** (media del ángulo en régimen permanente ≈ +0.02 mrad en la
captura comparada contra MATLAB). La discrepancia de amortiguamiento en el transitorio queda **abierta** (ver pendientes).

**Configuración de referencia (firmware `firmware/fuentes/control_hil_v3.c`):** F28379D LaunchPad, CPU1;
ISR de Timer0 cada 10 µs (PRD = 1999 a 200 MHz → 100 kHz); planta linealizada integrada en la ISR (Euler-Cromer);
DAC-B (pin 70) → jumper → ADC-A0 (pin 30); PID con KP = 5, KI = 2, KD = 0.15; derivada cada 1000 ISR (10 ms)
con filtro IIR α = 0.386; telemetría cada 10 ms por SCI-A, 3 columnas `pos_mrad,vel_mrad_s,u_milli`
(la "velocidad" enviada es `theta_dot_filt`, no la θ̇ verdadera; resolución 1 mrad por redondeo a entero).
1 LSB del ADC = π/4095 = 0.7672 mrad.

### Prueba 6: Barrido DAC → ADC (caracterización del lazo de hardware)
- **Fecha:** 2026-10-31
- **Objetivo:** medir cuánto se desvía el ADC respecto al código enviado al DAC (el error de ≈ 0.014 rad sospechado como offset del lazo).
- **Método:** firmware `diag_adc_dac.c`: 65 códigos de DAC de 0 a 4095 en pasos de 64; para cada uno, media y desviación del ADC. Datos copiados de la ventana Watch de CCS (`124b20f8-attachment.txt` en la conversación; reducidos a `analisis/barrido_dac_adc.csv`). Ajuste con `analisis/analisis_barrido.m` / `numpy.polyfit`.
- **Resultado (reloj del sistema de 100 MHz, ver Prueba 8):**
  - Ajuste lineal ADC = 0.9906·DAC + 1.24 LSB.
  - Diferencia ADC − DAC: +3.6 LSB en código 0; −7.1 LSB en 1024; **−17.6 LSB en 2048**; −29.8 LSB en 3072; −34.5 LSB en 4032.
  - Residuos respecto de la recta: −7.2 a +5.8 LSB (no es una recta perfecta: escalones de 1–4 LSB).
  - σ del ADC por punto: 0.62 a 1.11 LSB (0.48 a 0.85 mrad).
- **Conclusión:** el lazo DAC→ADC tiene un offset de ≈ −17.6 LSB en la zona de trabajo (código ≈ 2048–2178). Ese offset de ≈ 13.5 mrad es el que producía el ángulo "real" de ≈ 0.014 rad. Con la ecuación de θ medido, el estado estacionario es θ_real = (B − off)·LSB, donde off = media(ADC − DAC) en el punto de operación.
- **Limitación:** la calibración es válida sólo para esta configuración de reloj/ADC y cerca del punto de equilibrio (la no linealidad hace que el offset cambie 1–4 LSB en otros puntos).

### Prueba 7: Corrección de signo y punto de equilibrio (error mío, corregido)
- **Fecha:** 2026-10-31
- **Qué pasó:** en el análisis inicial afirmé θ_real = −13 mrad con B = 0 y un código de equilibrio ≈ 2030. Al revisar la ecuación del firmware, el signo correcto es **+13.6 mrad** con B = 0 y el código de equilibrio es **≈ 2065**.
- **Acción:** se rehizo el cálculo; todas las conclusiones posteriores usan el signo corregido.
- **Conclusión:** se introduce la constante de calibración `ADC_CAL_B` (offset en LSB) y `ADC_CAL_M` (ganancia, se deja en 1.0). Fórmula: `θ_medido = THETA_MIN + ((adc − ADC_CAL_B)/ADC_CAL_M)·LSB`. El firmware calcula en línea `b_sugerido = ADC_CAL_B − theta_mean_acc/LSB` como estimador absoluto del offset.

### Prueba 8: Reloj del sistema a 100 MHz en lugar de 200 MHz
- **Fecha:** 2026-10-31
- **Síntoma:** variables de diagnóstico del firmware (`sysclk_mhz_reg`, `lspclk_mhz_reg`, calculadas de `ClkCfgRegs`) mostraban **SYSCLK = 100 MHz y LSPCLK = 25 MHz**; el ISR medía hasta 1578 ciclos.
- **Causa:** el proyecto de CCS **no tenía definido el símbolo predefinido `_LAUNCHXL_F28379D`**. Un hilo de TI E2E indica que este símbolo corrige el comportamiento de reloj lento en el LaunchPad; aquí se verificó **empíricamente**.
- **Primer intento fallido:** se agregó el símbolo en las propiedades *del archivo* (Properties del `.c`), lo cual no tiene efecto sobre el resto de archivos. Se repitió a nivel *proyecto* (Properties → Build → C2000 Compiler → Predefined Symbols) y se hizo **Clean** antes de compilar.
- **Resultado:** `sysclk_mhz_reg` 100 → **200**, `lspclk_mhz_reg` 25 → **50**. Con 200 MHz la ISR cumple en tiempo real: `isr_max_cycles` 1378–1578 de 2000 ciclos disponibles, `isr_overruns = 0`. Periodo medio de las tramas: **10.004 ms** (antes el "100 kHz" real era 50 kHz).
- **Consecuencia sobre el baud:** BRR = (LSPCLK + 4·BAUD)/(8·BAUD) − 1 → 26 (25 MHz) o 53 (50 MHz); ambos dan +0.47 % de error (≈ 115 741 baud). El firmware v3 elige el valor según el símbolo y emite `#warning` si falta.
- **Consecuencia sobre el offset:** el offset del lazo DAC→ADC **cambió con el reloj**: −17.6 LSB a 100 MHz y **+6.6 LSB a 200 MHz** (`b_sugerido` ≈ 6.6). La causa **no está verificada**; hipótesis: ventana de adquisición/reloj del ADC. No se afirma cuántos ns dura la ventana porque las fuentes consultadas discrepan en si `ACQPS` cuenta ciclos de SYSCLK o de ADCCLK. Esto implica que **la calibración debe repetirse si se cambia el reloj o la configuración del ADC**.

### Prueba 9: Telemetría inundada de '9' (`sprintf %ld`) — `tx_frames = 0`
- **Fecha:** 2026-10-31
- **Síntoma:** el sniffer (`sniffer_serial.py`, manda 'S' y muestra bytes crudos 6 s) recibía un flujo interminable de caracteres '9' y la variable `tx_frames` del Watch seguía en 0; la primera trama nunca terminaba.
- **Causa:** `sprintf` con `%ld` en el C28x (`int` de 16 bits) generaba una cadena sin fin.
- **Corrección:** formateador propio `fmt_long` (resta potencias de 10; verificado contra `printf` para ±999999 en PC) y `to_milli` saturado a ±999999 con NaN → 0. Sin `sprintf` en el firmware.
- **Resultado:** sniffer con tramas válidas; en una captura el Watch mostró `tx_frames = 1842`; con 200 MHz, 6528 bytes en 6 s.

### Prueba 10: Middleware Python (`middleware/hil_coppelia.py`) sin datos
- **Fecha:** 2026-10-31
- **Síntomas:** CSV vacío; "Deteniendo por el usuario… No se recibió ningún dato válido"; la primera corrida falló con `object /pendulum_joint does not exist`.
- **Causas:** (1) `readline()` de pyserial no regresa mientras lleguen bytes sin `\n` (la inundación de '9'), por lo que el script parecía congelado; (2) en CoppeliaSim la pestaña de escena activa no era `Escena_Pendulo`; (3) se ejecutó sin la opción `--sin-coppelia` cuando se quería sólo grabar el CSV.
- **Correcciones (v2):** clase `LectorLineas` con *timeout* real; `hacer_trigger` con 3 intentos y aceptación de una línea de datos si se perdió el "OK"; filtro `^-?\d+,-?\d+,-?\d+$`; el CSV se escribe y vacía **antes** de llamar a CoppeliaSim; latido cada 2 s; resumen final que compara conteo de líneas contra reloj real; archivo con fecha (`datos_sin_perturbacion_AAAAMMDD_HHMMSS.csv`) más copia fija `datos_sin_perturbacion.csv`; columnas `Tiempo_s, Posicion_rad, Velocidad_rad_s, Control_u, Tiempo_reloj_s`.
- **Pruebas:** con puerto serie simulado (flujo normal e inundación). En hardware: captura `datos_sin_perturbacion_20261101_011726.csv` con 2000 líneas válidas (20 s).
- **Nota de Windows:** las marcas de tiempo del reloj llegan en ráfagas de ≈ 16 ms (media 10.004 ms, correcta); por eso el eje de tiempo se construye por **conteo de líneas** (`Tiempo_s`).
- **Procedimiento:** el DSP atiende la 'S' una sola vez tras cada arranque; para repetir hay que reiniciar el programa en CCS y dar Resume antes de ejecutar Python.
- **Archivo `datos_sin_perturbacion_20261031_235904.csv`:** corrida fallida (sólo encabezado), no usar.

### Prueba 11: Artefacto de arranque (primera muestra del ADC)
- **Fecha:** 2026-11-01
- **Síntoma:** en las gráficas viejas de MATLAB aparecía un pico inicial (hasta ≈ 0.15 rad) y una velocidad inicial falsa de ≈ +2.96 rad/s.
- **Causa:** `ResetEstado()` inicializaba el DAC en 2048 (ángulo 0), así que la primera lectura del ADC daba 0 rad aunque la condición inicial era `THETA_INIT`; la derivada lenta interpretaba el salto como velocidad.
- **Corrección [v3-12]:** el DAC se inicializa con el código de `THETA_INIT` (2178) y se espera `DELAY_US(200)` antes de arrancar.
- **Conclusión:** el pico inicial viejo era un artefacto del arranque, no dinámica del péndulo.

### Prueba 12: Repeticiones de la calibración (`ADC_CAL_B = 6.1`)
- **Fecha:** 2026-11-01
- **Método:** con `ADC_CAL_B = 6.1`, 200 MHz, se dejó que el firmware acumulara N_SETTLE = 200 000 + N_ACC = 1 000 000 muestras y se leyó `theta_mean_acc` en Watch (resolución fina; la telemetría sólo da 1 mrad).
- **Resultados de `theta_mean_acc`:** 0.187 mrad (con B = 6.6), y en tres repeticiones con B = 6.1: 0.537, 0.440 y 0.459 mrad. En esas mediciones el máximo de ISR fue 1578 ciclos y 0 *overruns*.
- **Conclusión:** el ángulo en régimen permanente quedó en 0.2–0.5 mrad (antes ≈ 14 mrad). Falta la prueba de repetibilidad pedida: 3 reinicios + 1 apagado/encendido completo.

### Prueba 13: Comparación DSP vs MATLAB (v2)
- **Fecha:** 2026-11-01
- **Script:** `analisis/comparacion_sin_perturbacion.m` (v2). Condiciones iniciales θ = 0.1, θ̇ = 0; la "velocidad" simulada es `theta_dot_filt`; tiempo `t = (1:N)·0.01`; redondeo igual al del DSP.
- **Errores de mi versión anterior del script del usuario corregidos:** condición inicial tomada de la columna de velocidad, comparación contra θ̇ en vez de la filtrada, y desfase de 10 ms.
- **Resultado (salida de MATLAB v2 que reportó el usuario; corresponde a una captura posterior a [v3-12] que *no* se subió al repo):**
  - Régimen permanente (t ≥ 2 s): media DSP = **+0.018 mrad**, MATLAB = −0.026 mrad.
  - RMS total: posición 0.00100 rad, velocidad 0.0474 rad/s, control 0.0099.
  - Transitorio (≤ 0.6 s): posición 0.00557 rad, velocidad 0.2204, control 0.0458.
  - Permanente (≥ 2 s): posición 0.00024, velocidad 0.0285, control 0.0061.
- **Nota:** la captura `datos_sin_perturbacion_20261101_011726.csv` (anterior a [v3-12], 2000 líneas) da para t ≥ 2 s una media de +0.65 mrad y σ = 0.48 mrad; no es la que produjo las cifras de arriba.
- **Conclusión:** el permanente coincide con el modelo (diferencias del orden de la resolución de 1 mrad de la telemetría). El **transitorio no coincide**: el DSP está más amortiguado que MATLAB (primer valle ≈ −25 mrad frente a −32 mrad).
- **Descartado por simulación:** error de ganancia de medición o retardo de medición no explican la diferencia. Aproximan el resultado Kp × 0.8 o fricción × 4, pero **sin causa confirmada**.

### Verificaciones que no se pudieron hacer / hallazgos que se conservan como abiertos
- La figura del usuario con pico rojo de 0.15 rad y etiqueta "ω" era una ventana vieja de MATLAB (con el artefacto de arranque); las figuras v2 son las correctas.
- Se retiró una afirmación mía anterior de "convergencia lenta" y se descartó `AdcSetMode` como causa principal del offset.
- Error de enlace intermedio `undefined symbol AdcSetMode`: se resolvió agregando `F2837xD_Adc.c` al proyecto.
- Discrepancia de ruido: el reporte del 2026-10-04 dio σ = 1.61 mrad y media −1.5404 rad (que implicaría un código ADC ≈ 40, incompatible con un lazo cerrado con DAC en 2048); las mediciones de hoy dan σ = 0.62–1.11 LSB (0.48–0.85 mrad). No se ha reconciliado.

### Pendientes para la siguiente sesión
- [ ] Diagnosticar el amortiguamiento del transitorio: compilar con `DIAG_TELE_ERR 1` (3ª columna = θ_medido − θ) y comparar.
- [ ] Repetibilidad de la calibración: 3 reinicios + 1 apagado/encendido; anotar `theta_mean_acc` y `b_sugerido`.
- [ ] Probar `ACQPS` 14 vs 99 y comparar `offset_lsb` (causa del cambio de offset con el reloj).
- [ ] Regenerar `diag_adc_dac.c` para 200 MHz (el BRR está fijo en 26, válido sólo para 100 MHz) y repetir el barrido.
- [ ] Agregar al repo el CSV final que usó MATLAB v2 (el de esta sesión que no quedó subido).
- [ ] Reconciliar el σ de la Prueba 2 con las mediciones nuevas.
- [ ] Añadir perturbación y separar el controlador y la planta en los dos núcleos.
