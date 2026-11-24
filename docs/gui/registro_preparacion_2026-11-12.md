# Registro de la preparación (P0) — 2026-11-12

Hecho con asistencia de Claude. Todo lo copiado es una **copia**; nada del proyecto original se modificó.

## 1. Qué se copió y dónde

Destino: `firmware\_factibilidad\` (dentro de una carpeta conectada a la sesión; se puede borrar completa sin afectar nada).

| Origen | Destino | Nota |
|---|---|---|
| `firmware\Proyecto_Pendulo_CPU1\` (archivos fuente y de configuración) | `_factibilidad\proyecto\Proyecto_Pendulo_CPU1\` | Sin `CPU1_RAM_DEBUG\` (salidas de compilación), sin `.clangd\` (caché), sin `.theia\` ni `x.theia-workspace` |
| `C:\ti\c2000\C2000Ware_5_02_00_00\...\ipc_gpio_toggle\cpu01\ipc_gpio_toggle_cpu01.c` | `...\Proyecto_Pendulo_CPU1\ipc_gpio_toggle_cpu01.c` | Es el `main` real del proyecto (ver hallazgo H1). Ahora es archivo local |
| `.project` | `.project` (modificado) y `.project.orig` (original) | Única modificación: se quitó el bloque `<link>` de `ipc_gpio_toggle_cpu01.c` para que la copia no compile el archivo vivo del SDK (hallazgo H1) |
| `CPU1_RAM_DEBUG\Proyecto_Pendulo_CPU1.out` y `.map` | `_factibilidad\linea_base\` | Línea base P5: compilación hecha en el IDE el 2026-11-09 a las 18:20 hora local (fecha del archivo; no es de hoy) |
| `fuentes\control_hil_v3.c` | `linea_base\control_hil_v3.c.ref` | Solo referencia (no es el `main` del proyecto, ver H1) |

Carpetas vacías creadas: `ws_cli\` (espacio de trabajo de `ccs-server-cli`) y `logs\`.
Huellas SHA-256 de lo copiado: `linea_base\SHA256.txt`.

## 2. Datos de la línea base

- `Proyecto_Pendulo_CPU1.out`: 494 300 bytes; `.map`: 58 708 bytes.
- Enlazador: TI TMS320C2000 Linker v25.11.0 (compilador C2000 25.11.x).
- Punto de entrada `_c_int00` en 0x8c46; `isr_max_cycles` en 0xaa24 (símbolo presente).
- Configuraciones del proyecto: `CPU1_RAM_DEBUG`, `CPU1_FLASH_DEBUG`, `CPU1_FLASH_STANDALONE`.
- Símbolo `_LAUNCHXL_F28379D` definido en las 3 configuraciones (3 apariciones en `.cproject`).
- `.ccxml` del proyecto: `TMS320F28379D.ccxml` con sonda **XDS100v2** (no XDS110) y dependencia de **C2000Ware 5.2.0.00**.

## 3. Hallazgos que cambian la guía

- **H1. El `main` del proyecto no está en el proyecto.** El `.project` lo enlaza (`PARENT-1-ORIGINAL_PROJECT_ROOT`) al archivo `ipc_gpio_toggle_cpu01.c` del ejemplo del SDK, y ese archivo es **idéntico byte a byte** (mismo SHA-256, 16 288 bytes) a `fuentes\sin_perturbacion\control_hil_sin_perturbacion.c`. O sea, el firmware que se compila hoy es la versión *sin perturbación*, no `control_hil_v3.c` (20 253 bytes, distinto). Consecuencia: editar `fuentes\...` no cambia lo que compila el IDE, y la prueba F1 con "modificar un archivo" habría tocado el archivo del SDK. **Aclarado por el usuario (2026-11-12):** el modelo de péndulo sin perturbación usado en las pruebas es el proyecto original `firmware\Proyecto_Pendulo_CPU1`, no la v3; por tanto la línea base y la copia son las correctas. Queda una discrepancia de documentación: el README de `firmware` presenta `control_hil_v3.c` como firmware vigente y no menciona que el `main` de este proyecto vive en la carpeta del ejemplo del SDK.
- **H2. Dependencia de la variable `COM_TI_C2000WARE_INSTALL_DIR`.** Los archivos de soporte del SDK están enlazados con rutas que parten de esa variable, y el proyecto está fijado a C2000Ware 5.2.0.00, pero en `C:\ti` hay **dos** versiones (`c2000\C2000Ware_5_02_00_00` y `C2000Ware_26_00_00_00`). En modo sin interfaz esa variable puede resolverse a otra versión o no resolverse. Riesgo nuevo para F1; se cubre con el paso `initialize -ccs.productDiscoveryPath` de la guía.
- **H3. Autoverificación del reloj.** El `main` incluye `#error` si falta `_LAUNCHXL_F28379D`. Si la compilación por línea de comandos pierde el símbolo, **falla en vez de generar un binario a 100 MHz**. Esto refuerza F1: un error `Falta _LAUNCHXL_F28379D` es un resultado informativo, no un fallo de la herramienta.
- **H4. Sonda XDS100v2.** La guía de verificación mencionaba XDS110; el `.ccxml` del proyecto usa XDS100v2. Corregido en la guía.
- **H5. Salidas obsoletas.** `CPU1_RAM_DEBUG\` conserva `ipc_gpio_toggle_cpu01.out/.map` de julio (no son de este proyecto). No se copiaron.
- **H6. Carpeta del SDK conectada a la sesión.** La carpeta `...\ipc_gpio_toggle\cpu01` está conectada y contiene el `main` activo; cualquier cambio ahí afecta al proyecto original.

## 4. Herramientas localizadas (P2, P4)

Se concedió acceso de solo lectura a `C:\ti\ccs2051`.

| Elemento | Resultado |
|---|---|
| CCS | 20.5.1.00012 (`ccs.properties`: `ccs_buildid`), instalado en `C:\ti\ccs2051`; instalación del 2026-06-11 |
| `ccs-server-cli` | `C:\ti\ccs2051\ccs\eclipse\ccs-server-cli.bat` (también existe `.sh`). El `.bat` ejecuta Node (`ccs\tools\node\node.exe`) con un `app-launcher.js` del plugin `com.ti.ccs.apps`. **Resuelve la duda de la extensión en Windows: se invoca como `ccs-server-cli.bat`.** |
| `dslite` | `C:\ti\ccs2051\ccs\ccs_base\DebugServer\bin\DSLite.exe` (**confirmado** en 20.5.1; antes era "por verificar") |
| Compilador | `ti-cgt-c2000_25.11.0.LTS` (coincide con el enlazador v25.11.0 de la línea base) |
| Conexión XDS100v2 | `ccs_base\common\targetdb\connections\TIXDS100v2_Connection.xml` presente |
| Otros compiladores instalados | armllvm 4.0.4, c29 2.2.0, c6000 8.5.0, c7000 5.0.1, msp430 21.6.1 (no se usan) |
| C2000Ware | 5.2.0.00 en `C:\ti\c2000\C2000Ware_5_02_00_00` (el proyecto lo requiere) y 26.00.00.00 en `C:\ti\C2000Ware_26_00_00_00` |
| CoppeliaSim, Python | No localizados todavía (carpetas no conectadas) |

## 5. Pendiente

- Ejecutar `dslite --help` (P3) y los comandos de F1 en la computadora: esta sesión solo mueve archivos, no ejecuta comandos en el equipo.
- Corregir el README de `firmware` (H1): indicar que el modelo sin perturbación de las pruebas es `Proyecto_Pendulo_CPU1` y dónde está su `main` (pendiente de aprobación del usuario; no se modificó).
- Localizar CoppeliaSim y Python (P4).
