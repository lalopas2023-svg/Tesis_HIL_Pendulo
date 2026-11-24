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
- **Evidencia:** `docs/capturas/baudrate_ok.png` (la imagen nunca se subió al repositorio; verificado el 2026-11-24)
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

### Pendientes de esta sesión (estado revisado el 2026-11-24)
- Caracterización formal del DAC por barrido (0 a 4095): **hecha** (Prueba 6, sesión 2026-10-31/11-01).
- Caracterización del ADC con fuente calibrada: **retirada**. El lazo DAC→ADC ya está caracterizado (Prueba 6) y su offset se mide y compensa en cada arranque con la autocalibración (adendas del 2026-11-19 y 2026-11-24).
- Protocolo UART extendido a 4 columnas: **hecho** (`firmware/fuentes/con_perturbacion/control_perturbacion.c` con `middleware/hil_coppelia_perturbacion.py`).
---

## Sesión 2026-10-31 / 2026-11-01

**Objetivo de la sesión:** que el modelo *sin perturbación, un solo núcleo (CPU1)* converja a 0 rad
(el ángulo real se estabilizaba en ≈ 0.014 rad) y dejar la cadena DSP → UART → Python → CoppeliaSim/MATLAB
robusta y verificada. **Resultado: logrado** (media del ángulo en régimen permanente ≈ +0.02 mrad en la
captura comparada contra MATLAB). La discrepancia de amortiguamiento en el transitorio queda **abierta** (ver pendientes).

**Configuración de referencia (firmware de esa sesión: `control_hil_v3.c`, retirado el 2026-11-24; su lógica es la de `firmware/fuentes/sin_perturbacion/control_hil_sin_perturbacion.c`):** F28379D LaunchPad, CPU1;
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

### Pendientes de esta sesión (estado revisado el 2026-11-24)
- Diagnosticar el amortiguamiento del transitorio con `DIAG_TELE_ERR 1` (3ª columna = θ_medido − θ): **retirada como prueba programada**. La opción solo existía en `control_hil_v3.c`, que se retiró del repositorio (su lógica es idéntica a `sin_perturbacion/control_hil_sin_perturbacion.c`, salvo esa opción). **La discrepancia del transitorio sigue abierta como hallazgo** (arriba, Prueba 13); si se quiere diagnosticarla, hay que portar la opción a la versión vigente (el original está en el respaldo, ver la entrada del 2026-11-24).
- Repetibilidad de la calibración (3 reinicios + 1 apagado): **hecha de otra forma**. La autocalibración v2 mide `B` en cada arranque y se validó con 18 corridas en 3 arranques, todas dentro de ±0.4 mrad (adendas del 2026-11-19 y 2026-11-24).
- Probar `ACQPS` 14 frente a 99: **retirada**. Con la autocalibración ya no hace falta conocer la causa del cambio de offset con el reloj para operar. La causa sigue sin determinarse.
- Regenerar `diag_adc_dac.c` para 200 MHz y repetir el barrido: **retirada** por la misma razón. La advertencia del baud (BRR = 26 solo vale a 100 MHz) queda en `firmware/README.md`.
- Agregar al repositorio el CSV final de MATLAB v2: **sigue abierto** (el dato no está subido; las corridas recientes están en la PC, `analisis/datos_sin_perturbacion_2026*.csv`).
- Reconciliar la σ de la Prueba 2 con las mediciones nuevas: **sigue abierto**.
- Añadir perturbación: **hecho** (`fuentes/con_perturbacion/`). Separar controlador y planta en los dos núcleos: etapa futura (prueba F4, diferida).

---

## Notas de laboratorio rescatadas (2026-11-03 a 2026-11-11)

> Las entradas de esas fechas trataban sobre el documento de tesis y se movieron a un archivo privado (ver "Depuración del repositorio, 2026-11-24" al final). Aquí quedan solo los hechos de laboratorio que contenían.

- **Procedencia de los datos.**
  - La copia `datos_sin_perturbacion.csv` que estaba publicada en `analisis/` (retirada el 2026-11-24: el middleware la sobrescribe en cada corrida) era **anterior a la calibración**: reproducida con la simulación da un RMS de posición ≈ 13.6 mrad, por el offset de 14 mrad. El CSV de la corrida corregida que produjo las cifras de la Prueba 13 no está en el repositorio.
  - Con perturbación: `analisis/datos_con_perturbacion_potenciometro_20261103.csv` (9000 muestras, 90 s). RMS de 1.25 mrad en total y 1.07 mrad desde los 2 s, calculados con `analisis/analisis_perturbacion_python.py` (simulación en Python; el `.m` equivalente, `comparacion_con_perturbacion.m`, existe pero no se ha reportado su salida).
  - `isr_max_cycles` leído en las capturas del 2026-11-01: 1578 a 100 MHz (captura `03`) y 1378 y 1578 a 200 MHz (capturas `06` a `09`). Medidas posteriores con la autocalibración: 1385 a 1578 de 2000 a 200 MHz, sin *overruns*.
- **Cálculos del 2026-11-10.** Polos del péndulo +5.447 y −6.003 s⁻¹ (|λ|Δt ≈ 6×10⁻⁵); BRR = 53 da 115 741 baudios (+0.47 % respecto de 115 200); ajuste del barrido: 0.9906·DAC + 1.24 (reproducido del CSV).
- **Hechos del código** (firmware sin y con perturbación): baud nominal 115200 (BRR calculado); PID y planta a 100 kHz dentro de la ISR; derivada cada 1000 ISR con α = 0.386; límites ±2 (integral) y ±10 (control); perturbación por potenciómetro (ADCINA2, centro 2048, zona muerta ±30, ±2000 códigos ↔ ±0.5) o por escalón (0.1 a los 2 s).
- **Hallazgo abierto.** En la corrida con perturbación la primera velocidad filtrada es ≈ −2 rad/s (el modelo da ≈ −0.1). No está explicada; podría tener el mismo origen que el transitorio más amortiguado de la Prueba 13.

---

## Sesión 2026-11-11 — Reporte independiente de diseño de la GUI

### 1. Qué se pidió y qué se decidió
El director comentó que, por el enfoque escolar, la GUI debe facilitar todo el proceso de pruebas (hoy: cargar desde CCS, abrir Python y CoppeliaSim, cada uno con su configuración). Decisiones del tesista:
- Alcance **B**: orquestador + control en línea (parámetros por UART sin recompilar, gráficas y CSV integrados). Abrir CoppeliaSim es un paso **opcional por modelo** (clave `visualizacion.coppelia` del descriptor de planta).
- Entorno: CCS 20.5.1, Windows 11, **sin UniFlash instalado**.
- **No se modifica la tesis** hasta tener el visto bueno del director; por ahora el diseño vive en un reporte independiente.

### 2. Archivos nuevos (nada se movió ni se borró)
- `docs/gui/Reporte_diseno_GUI.tex` y `.pdf` (17 págs.): problema, requisitos RG1–RG10, alcance de automatización investigado, alternativas A/B/C, arquitectura (flujo y procesos, figuras TikZ), decisiones justificadas (lenguaje, compilación, carga, parámetros, generador, YAML, puerto serie, CoppeliaSim, empaquetado), protocolo de comandos UART propuesto, riesgos, pruebas de factibilidad F1–F6, etapas G0–G5, validación, impacto en la tesis y preguntas para el director.
- Compilación: `pdflatex` ×2 con `TEXINPUTS` del babel-spanish local; 0 errores.

### 3. Hechos verificados en documentación (fuentes en el reporte)
- CCS 20.x compila por línea de comandos con `ccs-server-cli ... -application com.ti.ccs.apps.buildProject -ccs.projects <p> -ccs.configuration <cfg>` (carpeta `ccs/eclipse`).
- `dslite` (el programa de UniFlash en CLI) **viene incluido con CCS** (`ccs/ccs_base/DebugServer/bin`, según TI en un hilo de E2E con otro dispositivo: verificar en 20.5.1). Sintaxis: `dslite flash -c <ccxml> -f -v -u <out>`; `-r` reinicio; `-n` núcleo (0 = CPU1); código de salida 0 = éxito; existe modo `load` (RAM).
- DSS está documentado como **heredado (legacy)** en CCS 20.5.1 (sustituido por CCS Scripting) → se descartó para el ajuste de parámetros; se eligió UART.
- Limitaciones: UniFlash programa un núcleo a la vez; en hilos de TI, CPU2 por CLI falló en versiones viejas y la carga a RAM en doble núcleo quedó sin resolver.

### 4. No verificado (pruebas F1–F6 pendientes en el laboratorio)
Extensión de `ccs-server-cli` en Windows y uso con el IDE abierto; existencia de `dslite.exe` en CCS 20.5.1; flash vs RAM; CPU2 por CLI; costo en ciclos de recibir comandos con la ISR activa; tiempos de compilación/carga (no se afirma ninguna cifra); licencias PySide6/pyqtgraph.

### 5. Cómo restaurar / qué falta
- El documento de tesis **no cambió** en esta sesión. Cuando el director apruebe: reescribir 4.7, 5.6, cronograma, guía y, si procede, objetivos/delimitaciones (lista en la sección "Impacto en la tesis" del reporte).
- Decisión de diseño clave a recordar: el generador escribe **solo archivos fuente** sobre un proyecto CCS plantilla fijo (para conservar símbolos predefinidos como `_LAUNCHXL_F28379D`, rutas y linker).

### Adenda 2026-11-11 (2): Reporte de configuración de modelos

**Qué se pidió:** valorar qué valores del modelo puede modificar el usuario desde la GUI (Ts, baudios, K, perturbación, etc.), considerando 3 modelos precargados (péndulo, robot, motor CD) y la opción de agregar modelos nuevos con código generado desde cero; revisar factibilidad y estructurar.

**Archivos nuevos (nada movido ni borrado; la tesis no cambió):** `docs/gui/Reporte_configuracion_modelos.tex/.pdf` (14 págs.).

**Conclusiones del reporte (para poder reconstruir el razonamiento):**
- Factible con alcance acotado. Cota dura: presupuesto de ciclos de la ISR (Ts=10 µs = 2000 ciclos @200 MHz; en un núcleo Ts solo puede subir) y 3 salidas DAC de 12 bits en el chip (datasheet F2837xD; canales accesibles en LaunchPad: pendiente).
- Hechos calculados del firmware sin perturbación (entonces `control_hil_v3.c`): α=ωT/(1+ωT) con fc=10 Hz y T=10 ms da 0.386 (coincide con el código); derivada y telemetría cada 1000 ISR; BRR=round(LSPCLK/(8·baud))−1. Con LSPCLK=50 MHz el error de baud es +0.01 % (9600), −0.45 % (57600), +0.47 % (115200 y 230400), −3.12 % (460800 y 921600) → solo se ofrecerían los de error ≤2 % (criterio propuesto, sin referencia aún).
- Ámbitos de parámetro: en línea (comando W), de sesión (al reiniciar con Z), de generación (recompila), bloqueado (reloj, ACQPS, pines).
- Decisión estructural: los 3 modelos precargados son **descriptores** en el mismo formato que los modelos nuevos; un solo camino descriptor → validación/simulación previa → generador → proyecto C. Tabla de parámetros generada = única fuente de verdad PC/firmware; valores por UART como entero×10^exp.
- Modelos nuevos en 3 niveles: lineal (A,B,E,C), no lineal por expresiones (lista blanca, traducción a C, nunca `eval`), código a mano. Controladores propios fuera de la v1 (catálogo: P/PI/PID y realimentación de estados).
- Pruebas F7–F13 y etapas M1–M6 propuestas; preguntas abiertas para el director en la sección final (¿K = PID o estados?, definición de motor y robot, Ts editable o preestablecido, tolerancia de baud).

**No verificado:** porcentajes de uso de la ISR (69–79 %) y su reloj; opciones de compilación de la TMU y ciclos de sin/cos; canales DAC/ADC accesibles; soporte de 230400 baudios del adaptador USB; costo en ciclos de parámetros variables; descriptores de motor y robot (valores del ejemplo YAML son ilustrativos).

### Adenda 2026-11-11 (3): Reporte de configuración de modelos v0.2

**Qué cambió y por qué:** el tesista respondió las preguntas del v0.1 y pidió contemplar hardware. La v0.1 se conservó sin cambios hasta el 2026-11-24, cuando se retiró del repositorio (ver el respaldo en la entrada final); la vigente es `docs/gui/Reporte_configuracion_modelos.tex/.pdf` (22 págs.). El documento de tesis sigue sin cambios.

**Decisiones del tesista incorporadas:**
1. Las "K" son ambas: ganancias PID y vector de realimentación de estados (catálogo de controladores incluye los dos).
2. Motor de CD y robot: sin especificar; se prevén como pestañas en estado *borrador* (gris, con lista de campos que faltan) y se agregó una **ficha de definición** de modelo con el péndulo como referencia.
3. Ts: **tres valores preestablecidos** por modelo (no se escribe libremente). Péndulo: 10 µs sería el más rápido; los otros dos por medir (F8/F9). En modelos nuevos la herramienta calcula los tres a partir de Ts,min y Ts,max.
4. Interfaz: una pestaña por modelo + pestaña "+ Nuevo modelo"; las pestañas se **generan del descriptor** (no se programan a mano); el péndulo es la implementación de referencia. Boceto en la Figura 1 del reporte.
5. Parámetros físicos editables (péndulo: L, m, b; g solo en modo avanzado; J, g/L, 1/J derivados de solo lectura). Cada parámetro del descriptor declara `editable`, rango y ámbito (por defecto *sesión*, opcionalmente *en línea* con marca del instante en el CSV). En modelos nuevos el autor decide cuáles son editables; los no editables se generan como constantes (menos ciclos). Valores actuales tomados de `control_hil_v3.c` (g=9.81, L=0.3, m=0.1, b=0.005, θ0=0.1).
6. Hardware: caja con conexiones fijas y periféricos activables desde la GUI; nueva sección con principio "conexión fija, uso configurable", **perfil de hardware** (`caja_v1.yaml`, ilustrativo), recursos candidatos (potenciómetros, interruptores, pulsadores, LED, puntos de prueba para osciloscopio, puentes DAC→ADC, acceso USB/reinicio) y criterios eléctricos/mecánicos. Se decide tras la etapa M3; se añadieron etapas H0 (inventario y pines) y H1 (caja).

**Hecho verificado en código en esta adenda:** en `control_perturbacion.c` el potenciómetro (ADCINA2) se lee dentro de la ISR solo en una fracción de los pasos (variable `leer_pot`) y la perturbación se elige al compilar (`MODO_PERT` 0/1/2, archivos distintos); el reporte propone pasarla a selección en línea (a medir en F8).

**Pendiente/no verificado:** valores de los 3 Ts; rangos de parámetros editables; niveles del ADC (0–3.3 V, verificar hoja de datos); tabla de pines libres de la LaunchPad; si hay conector de expansión; ficha del motor y del robot.

### Adenda 2026-11-11 (4): portada, formato y redacción de los reportes de la GUI

**Qué se pidió y qué se hizo (ambos reportes: `Reporte_diseno_GUI` y `Reporte_configuracion_modelos`):**
- Portada institucional (UTM) con el código dado por el tesista, adaptando el título al reporte, con subtítulo del tipo de reporte/versión y la línea "Proyecto de tesis: …". Logo: `docs/gui/Imagenes/UTM.png` (el mismo logo que usa la portada del documento de tesis). Se redujeron espacios verticales (`\vspace` de 0.5 a 0.2 cm) para que la portada quepa en una página. Fecha con `\today`.
- **Se eliminó el resumen (abstract)** y el bloque `\title/\author/\date/\maketitle`; el reporte es una propuesta formal pero no al nivel de la tesis.
- **El índice va en su propia hoja** (`\clearpage` antes y después de `\tableofcontents`).
- **Redacción impersonal**, sin referencias al autor como tercera persona: se sustituyó "decisión/propuesta/indicación del tesista" por formas como "decisión adoptada", "se propone", "se acordó", "se confirmó", "indicación de diseño". Búsqueda posterior: 0 apariciones de "tesista" en los dos `.tex`.

**Cómo restaurar:** las versiones previas (sin portada, con resumen y `\maketitle`) se retiraron del repositorio el 2026-11-24 (`Reporte_diseno_GUI_v0.1_sin_portada.tex`, `Reporte_configuracion_modelos_v0.2_sin_portada.tex` y el v0.1 del reporte de modelos `.tex/.pdf`); se recuperan del respaldo (ver la entrada final). Para compilar los `.tex` vigentes hace falta la carpeta `Imagenes/UTM.png` junto al archivo.

**Resultado:** reporte de GUI 17 págs.; reporte de modelos 23 págs.; 0 errores de compilación. La numeración de secciones y las referencias cruzadas no cambiaron.

## Sesión 2026-11-12 a 2026-11-24 — Pruebas de factibilidad de la GUI (F1–F6) y autocalibración del offset

### Adenda 2026-11-12: Reporte de diseño de la GUI v0.2
- Se actualizó `docs/gui/Reporte_diseno_GUI.tex` (v0.1 → v0.2) para ser consistente con las decisiones del 11-nov.
- Cambios: subtítulo a Borrador 0.2; sección "Cambios respecto a la versión 0.1"; requisitos RG11–RG14 (pestañas por modelo, K y Ts preestablecido, parámetros físicos editables, periféricos fijos); la subsección "Pantallas" (cinco pestañas por flujo de trabajo) se sustituyó por "Estructura de la interfaz" (una pestaña por modelo + "Nuevo modelo") y "Hardware de apoyo"; etapa G4 reformulada y referencia cruzada a M1–M6/H0–H1; tres preguntas nuevas para el director.
- Para restaurar la estructura anterior: `Reporte_diseno_GUI_v0.1.tex`, retirado el 2026-11-24 (ver el respaldo en la entrada final).

### Adenda 2026-11-12 (2): Guía de verificación de factibilidad F1–F6
- Se agregó `docs/gui/Guia_verificacion_factibilidad.md`: procedimiento paso a paso (para qué, por qué, comandos, criterio y qué hacer si falla) de las pruebas del capítulo 10 del reporte de la GUI, con preparación P0 y tabla de registro.
- Evidencia revisada: guía CCS 20.5.1 (importProject/buildProject), hilos de TI (sintaxis de CCS 20, -n 0/-n 2 y CLA habilitados, CPU2 sin arrancar con dslite), manual de CoppeliaSim (-h, -s, -q, ZeroMQ).
- No se pudieron consultar las páginas de dslite/UniFlash desde el entorno (URL fuera de lista permitida); por eso la guía incluye el paso P3 (`--help`) y marca esas banderas como "por verificar".
- Ajustes propuestos al capítulo 10 (pendientes de aplicar al reporte): criterio de F1 menos estricto, F2 con verificación de CLA en el ccxml, F4 no bloqueante, orden F1, F2, F5, F6, F3, F4.

### Adenda 2026-11-12 (3): Preparación P0 de las pruebas de factibilidad
- Se hicieron en la PC del laboratorio (`lap-pascual`) las copias de la guía: `firmware\_factibilidad\` con `proyecto\Proyecto_Pendulo_CPU1` (copia aislada), `linea_base\` (.out/.map/SHA-256), `ws_cli\` y `logs\`. Copias verificadas por SHA-256. No se modificó el proyecto original; en la copia solo se quitó del `.project` el enlace al `main` del SDK (queda `.project.orig`) y el `main` pasó a ser archivo local.
- Herramientas localizadas: CCS 20.5.1.00012 (`C:\ti\ccs2051`), `ccs-server-cli.bat`, `DSLite.exe` en `ccs_base\DebugServer\bin`, compilador C2000 25.11.0.LTS.
- Hallazgos: (H1) el `main` compilado es idéntico a `fuentes\sin_perturbacion\control_hil_sin_perturbacion.c`, no `control_hil_v3.c`; (H2) dependencia de `COM_TI_C2000WARE_INSTALL_DIR` con dos versiones de C2000Ware instaladas; (H3) el `#error` por falta de `_LAUNCHXL_F28379D` autoverifica el reloj; (H4) la sonda es XDS100v2, no XDS110.
- Registro completo: `docs/gui/registro_preparacion_2026-11-12.md`. Guía actualizada a borrador 0.2.
- Aclaración (2026-11-12): el usuario confirmó que el modelo sin perturbación usado en las pruebas es `firmware\Proyecto_Pendulo_CPU1` (original, no la v3). Hallazgo H1 resuelto; queda por corregir la redacción del README de `firmware` (no modificado).

### Adenda 2026-11-17: F1, primeros resultados
- Pasos 0 y 1 ejecutados en `lap-pascual`: rutas verificadas (True ×3) y `ccs-server-cli.bat -help` arranca (log `logs\f1_help.log`).
- Hallazgo: en CCS 20.5.1 los identificadores de aplicación difieren de la documentación general y de los foros (`com.ti.ccs.initialize`, `com.ti.ccs.apps.projectImport`, `com.ti.ccs.apps.projectBuild`; nombres cortos `initialize`, `importProject`, `buildProject`). `-noSplash` no figura en el uso. Guía actualizada. Siguiente: `-ccs.help` de cada aplicación para fijar las opciones reales.

### Adenda 2026-11-17 (2): F1, compilación por línea de comandos
- Resultado: la compilación con `ccs-serverc.exe` funciona (0 errores, 2 advertencias menores). Tabla de símbolos idéntica a la línea base (280/280); C2000Ware 5.2.0.00, compilador 25.11.0.LTS, `--define=_LAUNCHXL_F28379D` presente.
- Causa de los fallos previos: sintaxis de PowerShell 5.1 con argumentos `-ccs.xxx` sin comillas (hipótesis consistente con todos los datos; falta prueba directa). Ver `docs/gui/Resultados_F1_2026-11-17.md`.
- Hecho: se desensambló `BaseHeadlessApp`/`ProjectImportApp` de CCS (solo lectura, copia en `_factibilidad\debug_apps` del equipo) para confirmar que la ayuda aparece cuando `-ccs.location` llega vacío.
- Pendiente de F1: verificación del lanzador con comillas, modificación de fuente, CCS abierto, prueba de error.

### Adenda 2026-11-17 (3): F1 aprobada
- Lanzador oficial con opciones entre comillas: import y build correctos (exit 0). Confirma que la causa de los fallos previos era el manejo de `-ccs.xxx` sin comillas en PowerShell 5.1.
- Modificar `ADC_CAL_B` en la copia cambió el hash del `.out`; un error de sintaxis devolvió exit=1 con mensaje `#29`. La copia se restauró desde `.bak` y recompiló limpia.
- Solo se tocó la copia aislada (`_factibilidad`); el proyecto original y la línea base no se modificaron. Quedan archivos `.bak` en la copia.
- Pendiente: paso 7 (CCS abierto, opcional). Siguiente: F2.

### Adenda 2026-11-17 (4): F2 parcial, carga con DSLite
- DSLite carga el `.out` compilado por línea de comandos a CPU1 (exit 0, 17.8 s) y el programa corre: el middleware recibe `OK`, 2000 líneas válidas, 100 tramas/s, igual que con carga desde el IDE.
- La guía tenía banderas supuestas (`--help`, `-r`) que no existen en esta versión; corregidas en `Resultados_F2_2026-11-17.md`.
- Pendiente: lectura de `isr_max_cycles` con `DSLite memory`, 10 repeticiones, pruebas negativas.

### Adenda 2026-11-17 (5): F2, repetibilidad, `memory` y pruebas negativas
- 10 de 10 cargas con DSLite correctas (7.5-8.4 s). Tres corridas del middleware tras carga con DSLite: `OK`, 2000/0, 100 tramas/s.
- `DSLite memory` interfiere con el programa en ejecución (lecturas en cero y el DSP deja de responder a `S` hasta una nueva carga). Descartada para consulta en vivo; los ciclos de ISR no se midieron sin IDE.
- Negativas: `.out` o `.ccxml` inexistentes devuelven exit=1 con mensaje legible.
- Pendiente de F2: tarjeta desconectada y CCS abierto. Detalle en `docs/gui/Resultados_F2_2026-11-17.md`.

### Adenda 2026-11-17 (6): F2 cerrada
- Tarjeta desconectada: exit=1 en 1.1 s, error -151 de FTDI. CCS con sesión abierta: exit=1 en 3.3 s con el **mismo** mensaje; la GUI no puede distinguirlos solo por el texto. `identifyProbe` devuelve exit=0 aun sin tarjeta (inútil como chequeo de presencia).
- Recuperación correcta tras ambos casos (carga en 6 s, exit 0).
- F2 aprobada con la limitación de los ciclos de ISR (no medidos sin IDE). Detalle en `docs/gui/Resultados_F2_2026-11-17.md`.

### Adenda 2026-11-19: F6 parcial, CoppeliaSim por línea de comandos
- Lanzar con la escena como argumento (con ventana y con `-h`) deja el servidor ZeroMQ respondiendo; el cliente lee el péndulo y arranca/detiene la simulación. Con `-h` el proceso no cierra solo. Con escena inexistente no hay error al lanzar, solo al buscar `/pendulum_joint` (error 1189).
- Pendiente: ciclo completo con DSLite y middleware, confirmar cierre de procesos. Detalle en `docs/gui/Resultados_F6_2026-11-19.md`.

### Adenda 2026-11-19 (2): F6 cerrada y desviación de posición observada
- F6 aprobada: ciclo completo con CoppeliaSim lanzado por línea de comandos, carga con DSLite y middleware (2000 líneas, 0 ignoradas, 0 procesos restantes). Detalle en `docs/gui/Resultados_F6_2026-11-19.md`.
- **Observación del usuario (sin resolver):** en la comparación DSP vs MATLAB con la corrida del 2026-11-19 (`analisis/datos_sin_perturbacion_20261119_085527.csv`, no está en el repositorio), la media de posición en régimen permanente del DSP fue +2.695 mrad (desv. 0.460 mrad) contra -0.026 mrad de MATLAB; RMS de posición en permanente 0.00276 rad. En la corrida de referencia posterior a la calibración se reportó +0.018 mrad (RMS 0.00024 rad), y `theta_mean_acc` midió 0.44-0.54 mrad con `ADC_CAL_B = 6.1`. Diferencia: ≈3.5 LSB (0.7672 mrad por LSB). Causa **no determinada**; se plantea una prueba A/B (firmware compilado por IDE frente a compilado por línea de comandos, alternados, misma sesión). La diferencia de amortiguamiento en el transitorio ya estaba registrada como pendiente y no es nueva.

### Adenda 2026-11-19 (3): prueba A/B de la desviación de posición
- **Método:** 3 rondas alternando carga con DSLite del firmware compilado por línea de comandos (`headless`) y del firmware compilado por el IDE (`IDE_base`, línea base del 2026-11-09), misma sesión, `hil_coppelia.py --sin-coppelia`, 20 s por corrida; media de la posición para t ≥ 2 s (1800 muestras, telemetría redondeada a 1 mrad).
- **Resultados (media en mrad, desv. est. entre paréntesis):** headless 0.874 (0.332), 0.885 (0.319), 0.933 (0.325); IDE_base 0.917 (0.303), 0.928 (0.260), 0.874 (0.345). Diferencia entre firmwares ≈ 0.01 mrad, menor que la dispersión entre corridas (≈ 0.05 mrad). Tabla en `logs\f_offset_AB.csv` del equipo.
- **Conclusión:** el binario compilado por línea de comandos **no** es la causa de la desviación. La media de +2.695 mrad observada antes en la comparación con MATLAB **no se reprodujo** (las seis corridas dan ≈ 0.9 mrad ≈ 1.2 LSB). Causa de aquella lectura: no determinada. Falta comparar con las corridas anteriores por el mismo método (todos los CSV con fecha de `analisis/`).

### Adenda 2026-11-19 (4): línea de tiempo del offset y lectura del GEL
- **Media de posición (t ≥ 2 s) de todas las corridas guardadas en `analisis/`, mismo método (CSV, 1800 muestras):** 2026-11-01: 0.653, 0.090, 0.018; 11-05: -0.202, -0.325; 11-09 (carga desde CCS): 0.138, -0.014; 11-17 (carga con DSLite): 1.213, 0.941, 1.071, 0.867, 0.871; 11-19: **2.696** (085527, con CoppeliaSim), luego 0.874, 0.917, 0.885, 0.928, 0.933, 0.874 (A/B). Dos corridas del 10-31/11-01 no tienen datos con t ≥ 2 s (cortas).
- Promedios: corridas anteriores al 17-nov (n = 7) ≈ 0.05 mrad; corridas con DSLite (n = 11, sin el valor de 2.696) ≈ 0.94 mrad ≈ 1.2 LSB. Como el firmware del IDE cargado con DSLite también da ≈ 0.9 (A/B), el cambio **no es del binario**: es del modo de carga o del estado del hardware entre el 9 y el 17 de noviembre (confundidos). Falta la prueba de carga con CCS hoy.
- **GEL de CCS para la F28379D:** `OnReset` libera CPU2, deshabilita el watchdog, configura DCSM y copia la configuración del dispositivo desde la OTP; **no ejecuta calibración del ADC** (ni `Device_cal`). Descarta que CCS cargue trims del ADC que DSLite no cargue. `OnTargetConnect` inicializa RAM y hace `GEL_Reset()` (ver `docs/gui/Resultados_F2_2026-11-17.md`).

### Adenda 2026-11-19 (5): CCS frente a DSLite el mismo día
- **Prueba:** tres corridas con la carga y el arranque desde CCS (Resume) y, enseguida, dos con DSLite, ambas con `hil_coppelia.py --sin-coppelia`, 20 s, mismo método de media (t ≥ 2 s). Orden confirmado por el usuario.
- **Resultados (media en mrad):** CCS 0.956 (0.226), 0.851 (0.364), 0.887 (0.322); DSLite 0.867 (0.339), 0.838 (0.369). Promedios 0.898 (CCS) y 0.853 (DSLite): la diferencia (≈ 0.05 mrad) es menor que la dispersión entre corridas.
- **Conclusión:** el modo de carga (CCS o DSLite) **no** explica el corrimiento. Tampoco el binario (A/B anterior). Con CCS hoy también se obtiene ≈ 0.9 mrad, frente a ≈ 0.05 mrad antes del 17 de noviembre. La causa queda en el estado del hardware o su entorno; no determinada. Pendiente: medir `theta_mean_acc`, `offset_lsb` y `b_sugerido` en Watch (3 repeticiones y un apagado completo) y la respuesta del usuario sobre cambios físicos entre el 9 y el 17 de noviembre.

### Adenda 2026-11-19 (6): lecturas de Watch con CCS (5 capturas) y corrección de `B` en las capturas del 2026-11-01
- **Capturas de hoy** (`ADC_CAL_B = 6.1`, SYSCLK 200 MHz, `adc_timeouts` 0, `isr_overruns` 0 en todas; usuario: 1-3 = proyecto original con CCS, 4 = prueba tras reconectar, 5 = `.out` de la carpeta de factibilidad cargado desde CCS):
  | # | `isr_max_cycles` | `theta_mean_acc` (mrad) | `offset_lsb` | `b_sugerido` |
  |---|---|---|---|---|
  | 1 | 1578 | 0.820 | 4.650 | 5.031 |
  | 2 | 1578 | 0.909 | 4.671 | 4.915 |
  | 3 | 1377 | 0.908 | 4.677 | 4.917 |
  | 4 | 1578 | 1.080 | 4.333 | 4.692 |
  | 5 (compilado por línea de comandos) | 1578 | 1.133 | 4.325 | 4.623 |
- Los valores de ISR (1377-1578 ciclos, 0 overruns) coinciden con los del IDE: queda medido el tiempo de ISR del binario compilado por línea de comandos (limitación de F2 cerrada). La media de posición de Watch (0.82-1.13 mrad) concuerda con la del CSV (0.84-0.96 mrad).
- `b_sugerido` es la estimación del offset del lazo en LSB. Hoy: 5.03 → 4.92 → 4.92 → 4.69 → 4.62 (baja ≈ 0.4 LSB durante la sesión, en el orden en que se tomaron). Frente al 2026-11-01 (≈ 5.9-6.0) es ≈ 1.0-1.4 LSB menor. Causa **no determinada**.
- **Corrección:** en las capturas `docs/capturas/2026-11-01/06-09`, `b_sugerido + theta_mean_acc/LSB` da 6.600 en las cuatro (LSB = 3.14159/4095 = 0.7672 mrad), es decir, estaban medidas con **`ADC_CAL_B = 6.6`**, no con 6.1 (las capturas de hoy dan exactamente 6.100 con la misma cuenta). La Prueba 12 de esta bitácora y la tabla de `docs/reporte/reporte_avances.tex` (sección de calibración) rotulan las repeticiones 1-3 como `B = 6.1`: debe leerse `B = 6.6`. Con el offset de entonces (≈ 5.98 LSB), `B = 6.1` predice ≈ 0.09 mrad, lo que concuerda con las corridas de ese día con CSV (0.018 y 0.090 mrad). **Pendiente:** corregir el reporte antes de la entrega del 2026-11-23 (el usuario decide).

### Adenda 2026-11-19 (7): firmware con autocalibración del offset (propuesta en prueba, **no validada**)
- **Decisión del usuario:** la deriva del offset del lazo DAC→ADC (≈ 1 LSB entre el 2026-11-01 y hoy; ≈ 0.4 LSB dentro de una sesión) se resuelve midiendo el offset al arrancar, no con un `ADC_CAL_B` fijo.
- **Archivo nuevo:** `firmware/fuentes/autocal/control_hil_autocal.c` (copia del firmware `sin_perturbacion` con 4 cambios; no se modificó ningún archivo existente). Cambios, todos marcados `[AUTOCAL]`:
  1. `USE_AUTOCAL` (1 = autocalibra, 0 = comportamiento anterior con `ADC_CAL_B` fijo) y parámetros `N_CAL = 5000`, `CAL_DAC_CODE = 2048`, `CAL_TOL_LSB = 10`, `CAL_SPREAD_MAX = 24`.
  2. Variables para Watch: `adc_cal_b` (B que usa la ISR), `b_auto`, `cal_ok`, `cal_min`, `cal_max`, `cal_runs`.
  3. `AutoCalibrar()`: al recibir `S`, con el timer detenido, fija el DAC en 2048 (θ = 0), espera 2 ms, promedia 5000 lecturas del ADC y toma `B = media − 2047.5` (porque −THETA_MIN/LSB = 2047.5 y el controlador ve θ = 0 cuando adc = B + 2047.5). Si |B − 6.1| > 10 LSB o (máx − mín) > 24, conserva `ADC_CAL_B` y deja `cal_ok = 0`.
  4. La ISR y `b_sugerido` usan `adc_cal_b` en lugar de la constante.
- **Base de la fórmula:** derivación del propio firmware (θ_medido = THETA_MIN + (adc − B)·LSB); coherencia con los datos de hoy: `b_sugerido − offset_lsb` = 0.24-0.38 LSB (≈ la mitad de un LSB que separa 2047.5 de 2048). Residuo esperado por la cuantización del DAC/ADC: menos de ±0.4 mrad (medio LSB).
- **Autocomprobación en Watch:** `b_auto` debe quedar a menos de ≈ 0.3 LSB de `b_sugerido` (medido al final de 12 s) y `theta_mean_acc` cerca de 0.
- **No compilado ni probado** al escribir esta adenda. Criterio de aceptación propuesto (a acordar): media de posición (t ≥ 2 s) dentro de ±0.4 mrad en tres sesiones separadas por horas, incluida una tras apagado completo, con `cal_ok = 1`, `isr_max_cycles` < 2000, `isr_overruns` = 0.
- Copias en el equipo: `firmware\fuentes\autocal\` y `firmware\_factibilidad\control_hil_autocal.c`. Para restaurar el firmware anterior basta con volver a copiar el `main` original (hash que empieza con 65A47C32).

### Adenda 2026-11-19 (8): compilación de la versión con autocalibración
- Compilación completa por línea de comandos (`ccs-serverc`, `CPU1_RAM_DEBUG`, 15:04) de `control_hil_autocal.c` colocado como `main` de la copia aislada: **0 errores, 2 advertencias** (las mismas de siempre: sin codificación explícita; última línea sin salto de línea, ahora en la línea 468). Respaldo del `main` original en la copia: `ipc_gpio_toggle_cpu01.c.orig_autocal` (hash inicial 65A47C32 verificado antes de reemplazarlo).
- Pendiente: corridas con la tarjeta, lectura de Watch y repeticiones en otros momentos.

### Adenda 2026-11-19 (9): autocalibración v1 rechazada, v2 con descarte de lecturas atípicas, validación en lazo (sesión 1)
- **Resultado de la v1 (compilada 15:04):** 3 corridas con DSLite dieron medias de **+1.384, +1.160 y +1.034 mrad** (desv. 0.49, 0.37, 0.19), peores que el ≈ 0.9 mrad con B fijo. Captura en Watch: `cal_ok = 0`, `adc_cal_b = 6.1` (respaldo), `b_auto = 6.219`, `cal_min = 2051`, `cal_max = 2176`, `b_sugerido = 6.047`. **Causa:** el criterio de aceptación `max − min ≤ 24` rechazó la medición por lecturas atípicas aisladas; la media (`b_auto`) era correcta (a 0.17 LSB de `b_sugerido`), pero se usó el respaldo. Las medias decrecientes de esas 3 corridas (1.38 → 1.03) se explican por el respaldo fijo más variación de la sesión, no por la autocalibración.
- **Cambio (v2):** se descartan las lecturas a más de 12 LSB de (2048 + `ADC_CAL_B`) (`CAL_OUT_LSB`); `b_auto` es la media de las válidas; se acepta si |b − 6.1| ≤ 10 y se descartaron ≤ 250 de 5000 (`CAL_OUT_MAX`). Variable nueva `cal_out`. `cal_min`/`cal_max` siguen sobre todas las lecturas. La v1 queda en `firmware\fuentes\autocal\control_hil_autocal_v1_rechazada.c` (en el equipo también como `firmware\_factibilidad\control_hil_autocal_v1.c`). La v2 está en `firmware\fuentes\autocal\control_hil_autocal.c`.
- **Compilación v2** (16:55, `ccs-serverc`, `CPU1_RAM_DEBUG`): 0 errores, 2 advertencias (las de siempre; la línea del aviso pasó de 468 a 484). Nota: la compilación desde CCS (IDE) respondió "Nothing to be done" y no regeneró el `.out`; se compiló por línea de comandos.
- **Watch con la v2 (17:3x):** `cal_ok = 1`, `cal_out = 1` (de 5000), `b_auto = adc_cal_b = 5.512`, `b_sugerido = 5.566` (Δ = 0.054 LSB), `cal_min = 2050`, `cal_max = 2173`, `cal_runs = 1`, `isr_max_cycles = 1385`, `isr_overruns = 0`, `adc_timeouts = 0`. El offset medido cambió 0.7 LSB respecto a la captura de la v1 (6.22 → 5.51), y la autocalibración lo siguió.
- **Lazo completo, 6 corridas seguidas con DSLite (17:40-17:42), media de posición t ≥ 2 s:** +0.039, +0.054, +0.174, +0.059, +0.336, +0.030 mrad (desv. 0.18-0.47). **Las 6 dentro de ±0.4 mrad** (máx. 0.336; media de medias 0.115) frente a ≈ 0.9 mrad con B fijo.
- **Criterio de aceptación (sesiones separadas):** cumplida la sesión 1 (`cal_ok = 1`, ciclos < 2000, 0 overruns, medias dentro de ±0.4). Pendientes: una sesión varias horas después y otra tras apagado completo de la tarjeta.
- Para restaurar el firmware anterior: `Copy-Item "$f.orig_autocal" $f -Force` (`$f` = `ipc_gpio_toggle_cpu01.c` de la copia aislada).

### Adenda 2026-11-19 (10): autocalibración v2, segunda tanda de 6 corridas (misma sesión térmica)
- 6 corridas seguidas con DSLite (17:49-17:51), sin recompilar, media de posición t ≥ 2 s: **−0.018, +0.019, +0.041, +0.079, +0.143, +0.023 mrad** (desv. 0.16-0.35). Las 6 dentro de ±0.4 mrad; media de medias +0.048 mrad.
- Junto con la adenda (9) suman **12 corridas consecutivas dentro de ±0.4 mrad** (rango −0.018 a +0.336 mrad).
- **Alcance:** esta tanda se hizo ≈ 8 min después de la anterior, con la tarjeta ya caliente: **no cuenta como sesión separada** del criterio de aceptación. Siguen pendientes la sesión de varias horas después y la sesión tras apagado completo.

### Adenda 2026-11-19 (11): aclaración sobre la segunda tanda de la v2
- El usuario aclara que la segunda tanda (adenda 10, 17:49) se hizo **tras 8 min con la tarjeta desconectada**, es decir, tras un arranque nuevo sin enfriamiento largo. Se corrige la lectura de la adenda 10: cuenta como arranque distinto del de la adenda 9, no como repetición en la misma sesión.
- Estado de la autocalibración v2: 12 corridas dentro de ±0.4 mrad en 2 arranques; falta una confirmación con la tarjeta fría (a la mañana siguiente, mismo bloque de 6 corridas). Su inclusión en el firmware de la tesis queda pendiente de la decisión del director.
- F6 permanece cerrada y aprobada; la autocalibración no es condición de ninguna F.

### Adenda 2026-11-19 (12): F5 preparada (comandos por UART con la ISR activa), sin ejecutar en la tarjeta
- **Firmware:** `firmware\fuentes\f5\control_hil_f5.c` = autocalibración v2 + `[F5]`. **La ISR no cambia.** Se añade `PollRx()` (lee el UART sin bloquear; solo levanta banderas) llamada en el bucle principal y dentro de las esperas de TXRDY de la trama. Comando `D` → responde entre tramas `D,isr_max,overruns,adc_to,rx_err,tx_frames,cal_ok,B_milli,cmd_diag,cmd_unk`. `S`, CR y LF se ignoran; cualquier otro byte suma a `cmd_unk`. Un middleware actual descarta la línea `D,...` como inválida (no coincide con el patrón de 3 enteros), sin otro efecto.
- **Riesgo conocido a vigilar:** `SciReadByte()` reinicia el SCI (`SWRESET`) ante un error de recepción, lo que podría truncar una trama en transmisión; la prueba exige `rx_errors = 0`.
- **Prueba:** `firmware\fuentes\f5\f5_prueba.py` (destino previsto en el equipo: `_factibilidad\f5_prueba.py`). Fases de 10 s: A base, B `D` a 10 Hz (latencia), C `D` a 100 Hz, D basura a 2 kHz + `D` final. Criterios: tasa 100 ± 2 tramas/s en cada fase, 0 líneas inválidas, respuestas completas, latencia mediana < 10 ms y máxima < 25 ms, ninguna `D` ni byte perdido (comparando contadores del DSP), `isr_max_cycles` < 2000, 0 overruns/timeouts/`rx_errors`.
- Verificado solo contra un DSP simulado (lógica de evaluación); un primer error propio del script (tiempo de la espera final no sumado a la fase D) se corrigió antes de entregarlo. **Pendiente: entregar los archivos al equipo (el puente con la computadora no estaba conectado al preparar esta adenda; en el equipo siguen la v2 y sin `f5_prueba.py`), compilar y correr en la tarjeta.** Al entregarse, el `main` de la copia aislada pasa a ser F5; para volver a la v2: `Copy-Item "$W\control_hil_autocal.c" $f -Force`; al original: `Copy-Item "$f.orig_autocal" $f -Force`.

### Adenda 2026-11-20: reporte de prácticas sobre las pruebas de factibilidad de la GUI
- **Archivos nuevos:** `docs/gui/Reporte_pruebas_factibilidad_GUI.tex` y `.pdf` (21 páginas). Reporte formal con tono de prácticas para el director: F1, F2 y F6 con objetivo, fundamento, procedimiento, resultados y decisión; hallazgo transversal del offset del ADC y su autocalibración; F5 preparada; tabla de decisiones para la GUI; limitaciones; apéndices con comandos.
- **Fuentes citadas, verificadas en esta sesión:** guía de CCS 20.5.1 (línea de comandos y opciones de `buildProject`), guía de UniFlash (DSLite), página GEL `OnTargetConnect()`, manual de CoppeliaSim (opciones de línea de comandos y API ZeroMQ), documentación de PowerShell (`about_Parsing`), issue 6292 de PowerShell, artículo de Microsoft sobre Maven y PowerShell, documentación de `subprocess` y de pySerial.
- **No verificadas en línea (libros):** Åström y Murray (acción integral) y Oppenheim y Schafer (error de cuantización); citadas como referencias de texto estándar. **Solo índice consultado:** manual de referencia técnico SPRUHM8; no se citan valores numéricos de él ni de la hoja de datos.
- **Corrección de la explicación de F1:** la documentación de PowerShell consultada no menciona el caso de argumentos con guion y punto (el issue 6292 trata de dos puntos). En el reporte se presenta como observación medida y causa inferida, no como regla documentada. Se anota además que la documentación de CCS consultada no documenta códigos de salida.
- **Error propio corregido antes de entregar:** la lectura atípica de la autocalibración v1 se había escrito como +128 cuentas; es +122 respecto al valor esperado (2176 − 2054.1), efecto en la media 0.024 cuentas.
- **PDF:** compilado sin el paquete de español de LaTeX (no disponible en el entorno de trabajo): rótulos en español puestos a mano, pero con patrones de guionado en inglés. Al compilar el `.tex` en el equipo del estudiante (babel con `spanish`) se obtiene el guionado correcto. El `.tex` original no se modificó para esa prueba.

### Adenda 2026-11-24: autocalibración v2, arranque tras reconexión (sesión del 24 de noviembre)
- **Primer intento sin valor:** un bloque de 6 corridas se ejecutó con la tarjeta desconectada. Las cargas fallaron en segundos (duración total 21 s en vez de ≈ 3 min) y la tabla que mostró eran los mismos 6 archivos del 19 de noviembre (no se había creado ninguno nuevo). **Error propio:** el bloque no comprobaba el código de salida de la carga ni filtraba los archivos por fecha. Se corrigió: el bloque se detiene si `DSLite load` devuelve distinto de 0 y solo lista los CSV creados después del inicio.
- **Resultado válido (13:52-13:55, binario v2 sin recompilar, `.out` del 19/11 16:55):** 6 de 6 archivos nuevos; medias de posición ($t \ge 2$ s): **0.053, 0.008, 0.085, 0.056, 0.140, 0.081 mrad** (desv. 0.12-0.35). Las 6 dentro de ±0.4 mrad (máx. 0.140); media de medias 0.071 mrad. Con $B$ fijo eran ≈ 0.9 mrad.
- **Alcance:** arranque 5 días después de la v2, tras reconectar el USB. **No se registró el tiempo exacto sin energía** (la tarjeta estaba desconectada al menos desde poco antes de las 13:49). No se capturó `Watch` (`cal_ok`, `cal_out`, `b_auto`) en esta sesión.
- **Estado de la autocalibración v2:** 18 corridas dentro de ±0.4 mrad en tres arranques distintos (19/11 17:40, 19/11 17:49 tras 8 min desconectada, 24/11 tras reconexión). Cumple el criterio de aceptación propuesto salvo la captura de `Watch` de esta sesión. Su inclusión en el firmware de la tesis sigue pendiente de la decisión del director.

## Depuración del repositorio, 2026-11-24

**Qué se pidió:** quitar lo que sobra para que se vea lo importante, revisar que todo sea coherente, retirar las pruebas que ya no tienen sentido, no publicar el documento de tesis y dejar bien explicadas las carpetas. Los commits los hace el usuario; esta entrada no cambia ningún resultado de laboratorio.

**Respaldo (antes de tocar nada).** Dos archivos fuera del repositorio: `arbol_antes_depuracion_2026-11-24.tar.gz` (el árbol de trabajo de ese momento) y `git_antes_depuracion_2026-11-24.tar.gz` (la carpeta `.git` con todos los commits; el último, `9bcbc20`). Para recuperar un archivo: `tar xzf arbol_antes_depuracion_2026-11-24.tar.gz tesis_hil_pendulo/<ruta>`; para verlo como estaba en el último commit: extraer el segundo y ejecutar `git --git-dir=<carpeta>/.git show 9bcbc20:<ruta>`. Además, `aplicar_depuracion.py` guarda su propio respaldo de lo que cambia en la PC.

**Qué se retiró y por qué**

| Qué | Motivo |
|---|---|
| `pruebas_respaldo/` (7 archivos y su README) | Copias de versiones de octubre del firmware y del middleware; nadie las usa y quedan en el historial de git. Dos eran idénticas byte a byte. |
| `firmware/fuentes/control_hil_v3.c` | Su lógica es idéntica a `sin_perturbacion/control_hil_sin_perturbacion.c` (que es el `main` probado del proyecto, hallazgo H1); solo añadía la opción `DIAG_TELE_ERR`. El README la llamaba "vigente" sin serlo. |
| `firmware/fuentes/autocal/control_hil_autocal_v1_rechazada.c` | Versión rechazada de la autocalibración; el cambio a la v2 está descrito en la adenda del 2026-11-19 (9). |
| `middleware/hil_coppelia_261004.py`, `test_serial.py`, `datos_tesis_pendulo.csv` | Archivo vacío, prueba de baud de octubre reemplazada por `sniffer_serial.py`, y captura antigua que nadie lee (el CSV de `analisis/historico_octubre/` es otro). |
| `firmware/Proyecto_Pendulo_CPU1/`: `.theia/`, `x.theia-workspace`, `.clangd`, `.settings/`, `targetConfigs/TMS320F28377D.ccxml` | Archivos del IDE (con rutas de la PC del usuario, el `.clangd` dice "no lo agregues al control de versiones") y la configuración de otro dispositivo; el proyecto usa `TMS320F28379D.ccxml`. |
| `analisis/sim_perturbacion.py` | Borrador del 2026-11-01 que ningún documento cita. |
| `analisis/datos_sin_perturbacion.csv` | Copia que el middleware sobrescribe en cada corrida; la publicada era previa a la calibración. Ahora está en `.gitignore`, igual que los CSV con fecha. |
| `docs/gui/versiones_previas/` (reportes de la GUI v0.1 y versiones sin portada) | Versiones superadas de documentos vigentes. |
| `docs/gui/evidencias_F1/simbolos_cli.txt` | Duplicado exacto de `simbolos_linea_base.txt` (misma huella); se dejó una nota en el `LEEME.md`. |

**Qué se movió**

| De | A |
|---|---|
| `analisis/Comapracion*.m`, `analisi_ruido.m`, `analisis_multivariable.m`, `analisis_transitorio.m` y los tres CSV de octubre | `analisis/historico_octubre/` (sustentan las pruebas 2 a 5; la carpeta se puede borrar entera) |
| Figuras sueltas de `docs/` y `docs/capturas/` (5 PNG de octubre) | `analisis/historico_octubre/figuras/` |
| `docs/Modelo_péndulo_invertido_.pdf` y `docs/Reporte_eliminación_de_ruido.pdf` | `docs/reporte/` con nombres sin acentos |
| Archivos del documento de tesis (versión 9) y tres entradas de esta bitácora sobre su corrección y estructura | `privado/`, carpeta que está en `.gitignore` y no se publica. De las entradas se rescataron los hechos de laboratorio (sección "Notas de laboratorio rescatadas") |

**Qué se editó**
- READMEs de la raíz, `firmware/`, `middleware/`, `analisis/`, `simulacion/` y `docs/` reescritos al estado del 2026-11-24; nuevos `docs/gui/README.md` y `analisis/historico_octubre/README.md`.
- `.gitignore`: añade `privado/`, los archivos del IDE, los CSV generados y `firmware/_factibilidad/`.
- Esta bitácora: pendientes de las sesiones del 2026-10-08 y 10-31/11-01 reemplazados por su estado (hecho, retirado con motivo, o abierto) y rutas de restauración corregidas.
- `docs/gui/Guia_verificacion_factibilidad.md`: estado de las pruebas, paso P3 retirado (`--help` no existe en DSLite), tabla de resultados completada, rutas corregidas.
- `docs/reporte/reporte_avances.tex` y `reporte_teoria_control.tex`: se retiran los puntos pendientes que ya no tienen sentido (repetibilidad de la calibración, `ACQPS`, barrido a 200 MHz, margen de la ISR ya medido).
- Dos comentarios que citaban `control_hil_v3.c` (`middleware/hil_coppelia.py`, `analisis/comparacion_sin_perturbacion.m`) ahora citan `control_hil_sin_perturbacion.c`. No cambió ninguna línea de código.

**Ajustes al aplicarlo en la PC del laboratorio (2026-11-24).** El repositorio de la PC no era idéntico al del entorno de trabajo, y la depuración se adaptó:
- `pruebas_respaldo/` tenía dos archivos más (`Hil_Coppelia_ConPerturbacion.txt` y `Hil_Coppelia_SinPerturbaciontxt.txt`, versiones viejas del middleware); se retiraron con el resto y quedaron en el respaldo que guarda el script.
- `docs/gui/`, `firmware/fuentes/con_perturbacion/`, `firmware/fuentes/f5/`, `analisis/latencia_csv.py` y `analisis/analisis_perturbacion_python.py` no estaban en el repositorio de la PC y se copiaron desde el entorno de trabajo.
- `docs/reporte/reporte_avances.tex` de la PC era la versión anterior ("Corrección de ruido en la plataforma HIL"), distinta del PDF que ya estaba en `docs/` ("Diagnóstico y validación del modelo sin perturbación"). Se sustituyó por la fuente que corresponde a ese PDF, con los pendientes ya resueltos retirados. La versión anterior queda en el historial de git y en el respaldo.
- Se quitaron del seguimiento los archivos del IDE y la configuración de la F28377D también en `Proyecto_HIL_Perturbacion/` (antes `Proyecto_Pendulo_Perturbacion/`), la copia `analisis/datos_con_perturbacion.csv` y dos capturas vacías (64 bytes, solo encabezado). Las capturas con datos del 2026-10-31 al 11-05 que ya estaban versionadas se conservan.
- El PDF largo de `docs/` pasó a `docs/reporte/reporte_avances_2026-11-01.pdf`.

**Decisiones que quedan al usuario**
- Si se quiere conservar `DIAG_TELE_ERR`, hay que portarlo de `control_hil_v3.c` (en el respaldo) a la versión vigente; es la única forma de diagnosticar el transitorio más amortiguado.
- Si `analisis/historico_octubre/` también debe desaparecer.
- Falta un CSV de la autocalibración en el repositorio (las corridas están en la PC) y una entrada de bitácora para las sesiones con perturbación del 2026-11-03 y 11-05.
