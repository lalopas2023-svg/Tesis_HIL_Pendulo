# Resultados de F1: compilar sin abrir el IDE (2026-11-17)

Equipo `lap-pascual`. CCS 20.5.1.00012, compilador C2000 25.11.0.LTS, copia aislada del proyecto `Proyecto_Pendulo_CPU1`, configuración `CPU1_RAM_DEBUG`.

## Resultado

La compilación por línea de comandos **funciona**: 0 errores, 2 advertencias sin importancia (el proyecto no fija codificación; el archivo termina sin salto de línea). Duración de la compilación completa: unos 8 s.

| Comprobación | Resultado |
|---|---|
| Se generó el `.out` | Sí, 493 880 bytes (línea base: 494 300) |
| Tabla de símbolos con direcciones | **Idéntica** a la línea base: 280 de 280. El `.map` difiere en una sola línea, la fecha de enlazado |
| Versión de C2000Ware usada | 5.2.0.00 (la correcta; no se tomó la 26.00) |
| Símbolo `_LAUNCHXL_F28379D` | Presente como `--define=_LAUNCHXL_F28379D` en las líneas de compilación (14 apariciones) |
| Compilador | `ti-cgt-c2000_25.11.0.LTS`, mismas banderas del IDE (`-v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2`) |
| `-ccs.buildType full` | Aceptado: hace clean y luego build |
| Tamaño del `.out` | 420 bytes menos que la línea base; se atribuye a rutas de depuración (el proyecto está en otra carpeta). **No verificado**; la prueba de reloj de F2 es la que valida el comportamiento |

## Hallazgo principal: el problema era la sintaxis de PowerShell, no la herramienta

Todos los comandos escritos "en línea" (`-ccs.location "ruta"`) imprimieron la ayuda. Todos los pasados como arreglo de cadenas entre comillas (función `T` y `B` con `@a`) funcionaron. **Causa (confirmada con la prueba del lanzador entre comillas):** Windows PowerShell 5.1 divide los argumentos sin comillas que empiezan con `-` y contienen un punto (`-ccs.location` se convierte en `-ccs` y `.location`), por lo que la aplicación no recibe el valor y imprime la ayuda sin decir por qué. Revisión del código de TI (desensamblado de `BaseHeadlessApp` y `ProjectImportApp`): la ayuda se imprime cuando `-ccs.location` llega vacío, sin mensaje de error, que es lo observado.

Consecuencias:
- Con las opciones entre comillas (`"-ccs.location"`) debería funcionar también el lanzador `ccs-server-cli.bat`. **Verificado.**
- La GUI en Python no tiene este problema, porque pasa los argumentos como lista y no por PowerShell.

## Otros hallazgos
- `-workspace` del lanzador no es el espacio de trabajo real: solo es el destino de importación por omisión. Los metadatos están en `%LOCALAPPDATA%\Texas Instruments\CCS\.ccs-server\workspaces\<md5 de la ruta>`. Con `ccs-serverc.exe` se indica con `-data`.
- Los nombres cortos `importProject`, `buildProject`, `initialize` y los largos `com.ti.ccs.apps.*` son válidos.
- La importación reescribió el `.cproject` de la copia y creó `copiedFileOrigins.txt` vacío (sin consecuencias; es la copia).
- `-ccs.buildType full` imprime "Clean-only build" durante la fase de limpieza y después compila.

## Pruebas complementarias (13:51-13:52, mismo día)

| Prueba | Resultado |
|---|---|
| Lanzador oficial `ccs-server-cli.bat` con opciones entre comillas: importar y compilar | **Funciona** (import exit=0, build exit=0, 0 errores). La hipótesis de PowerShell 5.1 queda **confirmada** |
| Paso 6: `ADC_CAL_B` 6.1f -> 6.2f en la copia, build incremental | El `.out` cambió: SHA-256 `FCDC6912...80C5` -> `B94B8B75...6E56`. La herramienta recompila lo que cambia el código |
| Paso 8: línea `int error_prueba = ;` añadida | Error `#29: expected an expression` (línea 405), "1 out of 1 projects have errors", **exit=1** |
| Restauración desde el respaldo y build completo | exit=0, 0 errores. Hash del `.out` restaurado **no comparado** con `FCDC6912` (no se midió) |

Consecuencias para la GUI: el código de salida distinto de 0 y la línea `Error: archivo [line N]: mensaje` bastan para detectar y mostrar fallos de compilación. Con `-ccs.listProblems` el resumen de errores y advertencias sale en la salida estándar y se puede analizar con una expresión regular.

## Estado de F1
Aprobada. Queda solo el paso 7 (compilar con CCS abierto), opcional.

Evidencias en `docs/gui/evidencias_F1/`.
