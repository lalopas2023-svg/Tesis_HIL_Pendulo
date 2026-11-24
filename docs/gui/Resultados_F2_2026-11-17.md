# Resultados de F2: cargar con DSLite sin IDE (2026-11-17)

Equipo `lap-pascual`. DSLite 20.5.0.4063, sonda XDS100v2, `.ccxml` `TMS320F28379D.ccxml` de la copia aislada, `.out` de la compilación por línea de comandos de F1 (SHA-256 `FCDC6912...80C5`, igual al de antes de la prueba de modificación).

## Sintaxis real (corrige la guía)
- La ayuda se pide con `DSLite help` (`--help` da "Unknown operation"). Operaciones: `load`, `flash`, `memory`, `identifyProbe`, `help`.
- `load`: `-c/--config`, `-f/--file`, `-t/--timeout`, `-g/--log`, `-n/--core` (por omisión 0), `-h`. No tiene bandera de ejecución ni de reinicio: carga y sale "dejando el target corriendo". Las suposiciones `-r` de la guía quedan descartadas.

## Resultado
- Carga a RAM de CPU1: `exit=0`, mensaje `Success`, 17.8 s (primera ejecución, incluye la lectura inicial de la configuración del chip).
- Con CCS cerrado y sin procesos `ccs|theia|dslite|gmake` activos.
- Comprobación con el middleware de Python (`hil_coppelia.py`, COM10, 115200): `S` confirmado con `OK`, 2000 líneas válidas, 0 ignoradas, "Reloj del DSP correcto: 100 tramas/s", duración 20.00 s por conteo y 20.12 s de reloj real, buffer serie máximo 0 bytes. Mismos valores que las corridas del 2026-11-09 con la carga desde el IDE (2000/0, 100 tramas/s, 20.12-20.13 s).
- La tasa de 100 tramas/s valida el reloj: a 100 MHz sería de 50 tramas/s.

## Hallazgo lateral (para F6)
Con la escena de CoppeliaSim cerrada, el middleware imprime `Error al conectar con CoppeliaSim: 1189: in sim._getObject: object does not exist, or alias/path is ill formatted`. Sirve de mensaje para la prueba negativa de escena ausente.

## Repetibilidad
10 cargas seguidas (la primera ya con el programa corriendo en las demás): **10 de 10 con `exit=0`**, de 7.5 a 8.4 s cada una (la primera carga de la sesión, con lectura inicial de la configuración del chip, tardó 17.8 s). Tabla en `logs\f2_repeticion.csv` del equipo.

Corridas del middleware tras cargas con DSLite (14:37, 14:54 y 16:32): `OK`, 2000 líneas válidas, 0 ignoradas, 100 tramas/s, 20.12-20.13 s de reloj real en las tres.

## `DSLite memory`: descartada para inspeccionar la tarjeta en operación
Intento de leer `isr_max_cycles` (0xAA24), `isr_overruns` (0xAA26) y `sysclk_mhz_reg` (0xAA1C): todas las lecturas salieron en cero, con rango normal y con `@1`, antes y después de una corrida. La lectura de `0x8000` (código recién cargado) también salió en cero. Tras dos lecturas de memoria, el middleware **no recibió `OK`** a la `S` (error de "el DSP no respondió"), cosa que no ocurrió en ninguna corrida donde no hubo lecturas de memoria entre la carga y el middleware.
Conclusión: la operación `memory` altera el programa en ejecución. **Causa (2026-11-19, leída del archivo GEL de CCS `ccs_base\emulation\gel\f28379d_cpu1.gel`):** su función `OnTargetConnect()`, que se ejecuta en cada conexión, escribe los registros de inicialización de RAM (`0x5F412`, `0x5F432`, `0x5F452`: M0/M1/D0/D1, LS0-LS5, GS0-GS15; el propio GEL los comenta como "RAM INIT") y llama a `GEL_Reset()` (reinicio del DSP). Por eso toda lectura posterior sale en cero y el programa en RAM desaparece. Toda operación de DSLite que conecte (`load`, `memory`) hace esto antes de actuar; en `load` es inocuo porque el programa se carga justo después. No se verificó con una prueba directa que la RAM quede en cero; es la lectura del GEL más lo observado. **No es utilizable** como consulta en vivo. Una nueva carga con `load` restableció el funcionamiento (2000 líneas, 0 ignoradas, 100 tramas/s), por lo que la interferencia no es permanente.
Alternativa a futuro: exponer los contadores por la UART (p. ej. comando `D` del analizador mínimo de F5).
El criterio de rendimiento queda respaldado indirectamente: misma tabla de símbolos que la línea base (280/280) y corridas sin líneas perdidas ni deriva de reloj. Los ciclos de la ISR con el binario compilado por línea de comandos **no se midieron** en esta prueba.

## Pruebas negativas hechas (mensaje y código de salida)
| Caso | exit | Mensaje clave |
|---|---|---|
| `.out` inexistente | 1 | `Failed: Encountered a problem loading file: <ruta>` / `Could not open file` (conecta primero a la tarjeta y falla al abrir el archivo) |
| `.ccxml` inexistente | 1 | `Failed: Cannot read System Setup data from XML file <ruta>` / `unable to open primary document entity` |
Los mensajes salen por el canal de error: en PowerShell aparecen como `NativeCommandError`; desde Python se leen en `stderr`.

## Nota para la GUI
El middleware actual, ante falta de `OK`, sugiere revisar "CCS en Resume con USE_TRIGGER = 1", texto del flujo manual con el IDE. Con carga por DSLite ese mensaje no aplica y la GUI deberá dar el suyo.

## Más pruebas negativas (16:45-16:55)
| Caso | exit | Duración | Mensaje clave |
|---|---|---|---|
| USB desconectado | 1 | 1.1 s | `fatal: IcePick_C_0: Error connecting to the target: (Error -151 @ 0x0) One of the FTDI driver functions ... returned bad status ... no XDS100 is plugged in, invalid XDS100 serial number, blank XDS100 EEPROM, missing FTDI drivers, faulty USB cable` / `Failed: Operation was aborted on 'C28xx_CPU1'` |
| CCS con sesión de depuración abierta | 1 | 3.3 s | **El mismo mensaje -151**, idéntico al de la tarjeta desconectada |
| Recuperación tras cada caso | 0 | 6.1 s y 6.0 s | `Success` |
| `identifyProbe -c <ccxml>` con la tarjeta desconectada | **0** | inmediato | Solo imprime la versión de DSLite; no sirve como prueba de presencia (no se probó conectada) |

Hallazgos de diseño para la GUI:
- Los fallos de conexión son rápidos (1 a 3 s, no los 60 s del tiempo máximo), por lo que el tiempo máximo de 60 s solo importa para cargas lentas.
- **"Sin tarjeta" y "sonda ocupada por CCS" no se distinguen por el mensaje de DSLite** (mismo error -151). La GUI debe distinguirlos por otro medio, por ejemplo comprobando si hay procesos `ccs`/`theia`/`ccs-server` en ejecución antes de culpar al cable. Un chequeo de presencia de la sonda por USB (p. ej. la utilidad `xds100serial` que cita el propio mensaje) no se probó.
- Tras desconectar o liberar la sonda, una carga posterior funciona sin pasos adicionales (esperar ~5 s tras reconectar el USB).
- El mensaje de DSLite sale por el canal de error: en Python se lee de `stderr`; la última línea `Failed: ...` resume el fallo.

## Veredicto de F2
**Aprobada** con una limitación: los ciclos de la ISR no se midieron sin IDE (`DSLite memory` altera el programa; alternativa futura: exponerlos por UART).

Criterios: carga con exit 0 (sí), programa corre y responde `OK` (sí, 5 corridas del middleware), reloj de 200 MHz por la tasa de 100 tramas/s (sí), 10 de 10 repeticiones (sí), mensajes de error legibles (sí; sin tarjeta y sonda ocupada comparten mensaje).

## Adenda 2026-11-19: equivalencia de la carga con DSLite frente a CCS
Media de posición en régimen permanente el mismo día: CCS 0.898 mrad (3 corridas) y DSLite 0.853 mrad (2 corridas); diferencia menor que la dispersión entre corridas. La carga con DSLite deja la tarjeta funcionalmente igual que la carga desde CCS en este indicador. Hay un corrimiento de ≈ 0.9 mrad frente a las corridas anteriores al 17 de noviembre (≈ 0.05 mrad) que aparece con ambas herramientas y con ambos binarios (ver bitácora, adendas 2026-11-19).

## Adenda 2026-11-19: ciclos de la ISR del binario compilado por línea de comandos
Con el `.out` de `_factibilidad` (compilado por `ccs-serverc`) cargado desde CCS y leído en Watch: `isr_max_cycles` = 1578, `isr_overruns` = 0, `adc_timeouts` = 0, `sysclk_mhz_reg` = 200.0. Es el mismo máximo (1578; con 1377 en una corrida del proyecto original) que el del binario del IDE. Con esto, el criterio de rendimiento de F2 queda medido, aunque con CCS y no sin IDE (`DSLite memory` no sirve para esto; ver arriba). Detalle y tabla en la bitácora (adenda 2026-11-19 (6)).
