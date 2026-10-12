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