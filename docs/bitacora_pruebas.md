\# Bitácora de pruebas - 2026-10-04



## Prueba 1: Verificación de baud rate
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



\## Prueba 2: Diagnóstico de ruido

...

