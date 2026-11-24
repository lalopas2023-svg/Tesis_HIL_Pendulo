# Guía de verificación de factibilidad (F1 a F6)

Procedimiento paso a paso para comprobar, en el equipo del laboratorio, que lo propuesto en el capítulo 10 del *Reporte de diseño de la GUI* puede funcionar. Cada prueba indica **para qué** sirve, **por qué** se espera que funcione, el **procedimiento**, qué **observar**, el **criterio** de aceptación y qué hacer **si falla**.

Estado (2026-11-24): **F1, F2 y F6 aprobadas**; F5 preparada, sin ejecutar en la tarjeta; F3 pendiente; F4 diferida. Los resultados están en `Resultados_F1_2026-11-17.md`, `Resultados_F2_2026-11-17.md` y `Resultados_F6_2026-11-19.md`; la preparación, en `registro_preparacion_2026-11-12.md`. Donde un resultado contradice lo escrito en esta guía, manda el resultado. Entorno de referencia: Code Composer Studio 20.5.1, Windows 11, LAUNCHXL-F28379D, Python y CoppeliaSim.

## Convenciones de esta guía

- **Evidencia.** Cada afirmación se marca como *documentado* (se apoya en documentación oficial o en un hilo de TI citado en el reporte) o *por verificar* (es lo que la prueba debe confirmar). Nada de lo que está marcado *por verificar* debe darse por cierto en la tesis hasta tener el registro de la prueba.
- **Banderas de `dslite`.** La sintaxis se tomó de la documentación de TI y de un hilo de su foro, y F2 la corrigió con la herramienta real: la ayuda se pide con `DSLite help` (no con `--help`) y `load` no tiene bandera de ejecución ni de reinicio. Manda `Resultados_F2_2026-11-17.md`.
- **Registro.** Cada prueba se registra con el formato del apartado final. La salida de cada comando se guarda en un archivo (`> archivo.log 2>&1`), que se conserva como evidencia (los de F1 están en `evidencias_F1/`).
- **Orden recomendado.** P0, F1, F2, F5, F6, F3 y, al final, F4. F1 y F2 sostienen todo el flujo; si fallan, las demás pierden sentido.
- **No se modifica el proyecto de trabajo.** Todas las pruebas se hacen sobre una copia del proyecto y con un espacio de trabajo aparte.

---

## P0. Preparación

### P1. Copia de trabajo (ya realizada)
La copia está en `...\Tesis_HIL\firmware\_factibilidad\` (en la guía, `$W` es esa carpeta): `proyecto\Proyecto_Pendulo_CPU1` (copia aislada, con el `main` como archivo local), `linea_base\` (`.out`, `.map` y SHA-256), `ws_cli\` (espacio de trabajo vacío) y `logs\`. Detalle y hallazgos en `registro_preparacion_2026-11-12.md`.

**Para qué:** una falla de la prueba no puede dañar el proyecto que ya funciona. **Por qué un espacio de trabajo aparte:** CCS guarda sus metadatos en el espacio de trabajo; usar uno nuevo evita bloqueos o conflictos con el IDE (la documentación no aclara qué pasa si ambos comparten espacio, por eso no se arriesga).

### P2. Localizar las herramientas (resuelto, se conserva para verificar)
**Resultado de la preparación (2026-11-12, equipo `lap-pascual`):** CCS 20.5.1.00012 en `C:\ti\ccs2051`; `ccs-server-cli` se invoca como `C:\ti\ccs2051\ccs\eclipse\ccs-server-cli.bat`; `dslite` es `C:\ti\ccs2051\ccs\ccs_base\DebugServer\bin\DSLite.exe`; compilador `ti-cgt-c2000_25.11.0.LTS`. Las rutas de abajo ya están resueltas, y los comandos se pueden verificar así, en PowerShell:

```powershell
$CCS = "C:\ti\ccs2051"
$W   = "C:\Users\epasc\Documents\Eddy\Maestria\Tesis\Tesis_HIL\firmware\_factibilidad"
$DSL = "$CCS\ccs\ccs_base\DebugServer\bin\DSLite.exe"
Get-ChildItem "$CCS\ccs\eclipse" -Filter "ccs-server-cli*"
Get-ChildItem "$CCS\ccs\ccs_base\DebugServer\bin" -Filter "dslite*"
Get-ChildItem -Recurse "$W\proyecto" -Filter "*.ccxml"
```

El `.ccxml` de la tarjeta es `targetConfigs\TMS320F28379D.ccxml` del proyecto (sonda **XDS100v2**, la integrada en la LaunchPad).

### P3. Confirmar la sintaxis de `dslite` (retirado el 2026-11-24)
Ya no se ejecuta: `DSLite.exe` no acepta `--help` ("Unknown operation"); la ayuda sale con `DSLite help`, y la sintaxis real de `load`, `flash`, `memory` e `identifyProbe` quedó registrada en `Resultados_F2_2026-11-17.md`.

### P4. Versiones
Anotar versión de CCS (Ayuda → Acerca de), del compilador C2000 (en las propiedades del proyecto), de CoppeliaSim, de Python y del sistema operativo. Las versiones obtenidas están en el registro de la preparación y en `README.md` (raíz).

### P5. Línea base del firmware actual
**Ya guardada** la compilación del IDE (`linea_base\`; `.out` de 494 300 bytes, compilada el 2026-11-09). Falta el dato de ejecución: con CCS (flujo normal) se carga el proyecto actual en la tarjeta y se verifica que el reloj sea de 200 MHz y que `S` responda `OK`. Se guardan los datos de referencia: tamaño del `.out`, tamaño del `.map`, `isr_max_cycles` observado y una captura de la salida. **Para qué:** F1 y F2 se comparan contra este estado; sin línea base no hay forma de saber si "funciona igual".

---

## F1. Compilar con `ccs-server-cli`

**Para qué.** El generador producirá archivos fuente en C. Para que la GUI pueda compilarlos sin abrir el IDE, se necesita que la línea de comandos produzca el mismo ejecutable que el IDE.

**Por qué se espera que funcione.** *Documentado:* la guía de CCS 20.5.1 describe las aplicaciones `importProject` y `buildProject` de `ccs-server-cli`; el hilo de TI sobre CCS 20 muestra compilaciones correctas una vez corregida la sintaxis; un error con `${CCS_WORKSPACE_DIR}` en modo sin interfaz se corrigió en la versión 20.3.0. La compilación usa la configuración del propio proyecto (símbolos predefinidos, rutas de inclusión, archivo de enlazado), así que el símbolo `_LAUNCHXL_F28379D` definido a nivel de proyecto debería conservarse. Esto último es lo que la prueba confirma.

**Particularidades de este proyecto (encontradas en la preparación).**
- El `.project` enlaza los archivos de soporte del SDK mediante la variable `COM_TI_C2000WARE_INSTALL_DIR` y el proyecto exige **C2000Ware 5.2.0.00**, pero hay otra versión (26.00) instalada. En modo sin interfaz esa variable puede resolverse mal; por eso el paso 0 registra las rutas de productos con `initialize`. Si la compilación falla por archivos no encontrados, es la primera causa a revisar.
- El `main` compilado es `ipc_gpio_toggle_cpu01.c`, **idéntico byte a byte** a `fuentes\sin_perturbacion\control_hil_sin_perturbacion.c` (es el modelo sin perturbación de las pruebas, confirmado por el usuario; no es `control_hil_v3.c`, que tenía la misma lógica y se retiró del repositorio el 2026-11-24). En la copia ya es un archivo local, de modo que modificarlo en el paso 5 no toca el SDK.
- El `main` termina con `#error` si falta `_LAUNCHXL_F28379D`: si la línea de comandos pierde el símbolo, el resultado es un error de compilación, no un binario a 100 MHz.
- Configuraciones disponibles: `CPU1_RAM_DEBUG`, `CPU1_FLASH_DEBUG`, `CPU1_FLASH_STANDALONE`. El nombre del proyecto es `Proyecto_Pendulo_CPU1`.

**Hallazgo del 2026-11-17 (`-help` en el equipo):** en CCS 20.5.1 las aplicaciones se llaman `com.ti.ccs.initialize`, `com.ti.ccs.apps.projectImport` y `com.ti.ccs.apps.projectBuild`, y se aceptan los nombres cortos `initialize`, `importProject` y `buildProject`; `-noSplash` no aparece en el uso. Los comandos de esta guía usan los nombres cortos. Las opciones de cada aplicación se obtienen con `-ccs.help` (paso 0b) y prevalecen sobre las de esta guía.

**Procedimiento.**

0b. **Obtener las opciones reales:** `ccs-server-cli.bat -workspace "$W\ws_cli" -application <app> -ccs.help` para `initialize`, `importProject` y `buildProject`; guardar en `logs\f1_help_<app>.log`.
0. **Registrar los productos** (una sola vez; la guía de CCS documenta `initialize` con `-ccs.productDiscoveryPath`):
   ```powershell
   & "$CCS\ccs\eclipse\ccs-server-cli.bat" -workspace "$W\ws_cli" `
     -application initialize `
     -ccs.productDiscoveryPath "C:\ti\c2000\C2000Ware_5_02_00_00" `
     > $W\logs\f1_initialize.log 2>&1
   ```
   (La ruta exacta que espera esta opción —la carpeta del producto o su carpeta contenedora— es *por verificar*; si no surte efecto, probar con `C:\ti\c2000`.)
1. **Importar sin copiar** (el proyecto queda donde está; solo se registra en el espacio de trabajo):
   ```powershell
   & "$CCS\ccs\eclipse\ccs-server-cli.bat" -workspace "$W\ws_cli" `
     -application importProject `
     -ccs.location "$W\proyecto\Proyecto_Pendulo_CPU1" `
     > $W\logs\f1_import.log 2>&1
   $LASTEXITCODE
   ```
   Si la herramienta imprime solo su ayuda, revisar la sintaxis: ruta entre comillas y sin diagonal invertida final, `-ccs.location` correcto.
2. **Conocer el nombre del proyecto y la configuración.** El nombre es el del archivo `.project` (campo `<name>`); las configuraciones aparecen como carpetas dentro del proyecto (por ejemplo `CPU1_RAM`, `CPU1_FLASH`). Anotarlos.
3. **Compilar de cero:**
   ```powershell
   & "$CCS\ccs\eclipse\ccs-server-cli.bat" -workspace "$W\ws_cli" `
     -application buildProject `
     -ccs.projects <NombreProyecto> -ccs.configuration <Configuracion> -ccs.buildType full `
     > $W\logs\f1_build.log 2>&1
   $LASTEXITCODE
   ```
   El valor `full` de `-ccs.buildType` aparece en un hilo de TI (la guía solo muestra `clean`); si lo rechaza, probar sin esa opción.
4. **Revisar el resultado:**
   - ¿Existe el `.out` en la carpeta de la configuración? ¿Su fecha es la actual?
   - Buscar en el registro errores y advertencias: `Select-String -Path ...\f1_build.log -Pattern "error|warning"`.
   - Buscar el símbolo del reloj: `Select-String -Path ...\f1_build.log -Pattern "LAUNCHXL"`. Si el registro no muestra las banderas del compilador, esta comprobación no es concluyente y la verificación real es la del paso F2 (reloj medido a 200 MHz).
   - Comparar con la línea base (P5): tamaños del `.out` y del `.map`, y presencia de las mismas funciones en el `.map`. No se exige identidad byte a byte (los sellos de tiempo pueden diferir); se exige el mismo conjunto de símbolos y tamaños equivalentes.
5. **Prueba de que compila lo que está en disco** (la base del generador): modificar un valor inocuo en un archivo fuente (por ejemplo una constante o un comentario con efecto en el binario), recompilar y confirmar que el `.out` cambia. Restaurar el archivo.
6. **Prueba con el IDE abierto.** Repetir el paso 3 con CCS abierto (sobre otro espacio de trabajo). *Por verificar:* la documentación no dice si es posible. El resultado decide si la GUI debe pedir cerrar CCS.
7. **Prueba negativa.** Introducir un error de sintaxis, compilar y comprobar que (a) la compilación falla, (b) el código de salida es distinto de 0 o, si no lo es, cómo se detecta el fallo en el registro, y (c) el mensaje permite localizar el archivo y la línea. Esto define cómo la GUI mostrará errores de compilación (RG6).

**Qué observar y registrar.** Códigos de salida de cada paso, duración de la compilación completa (`Measure-Command { ... }`), tamaños, y si hubo advertencias nuevas respecto al IDE.

**Criterio de éxito.** Compila sin errores, genera el `.out`, el conjunto de símbolos coincide con el del IDE, y el reloj resulta de 200 MHz al cargarlo (se confirma en F2).

**Si falla.** (a) Ayuda en pantalla: error de sintaxis, corregir como arriba. (b) "0 de 0 proyectos": el proyecto no se importó o el nombre es incorrecto. (c) Errores de rutas de inclusión: probable variable de espacio de trabajo; probar con `-ccs.copyIntoWorkspace true`. (d) Reloj a 100 MHz: el símbolo de proyecto no se aplicó; alternativa documentada en el reporte: compilar con el compilador de TI directamente.

---

## F2. Cargar con `dslite`

**Para qué.** Comprobar que el `.out` de F1 puede cargarse a la tarjeta sin abrir el IDE, que es el segundo eslabón del flujo de un solo botón.

**Por qué se espera que funcione.** *Documentado:* `dslite` es la herramienta de línea de comandos que acompaña a CCS y a UniFlash; usa el mismo archivo `.ccxml` que el depurador. Un hilo de TI sobre la F28379D confirma que el índice de núcleo `-n 0` corresponde a CPU1 y que los índices se derivan de la lista de núcleos del `.ccxml`. *Por verificar:* que funcione con el `.ccxml` de esta tarjeta y la versión instalada.

**Procedimiento.**

1. Conectar la tarjeta. **Cerrar cualquier sesión de depuración de CCS**: la sonda XDS100v2 la utiliza un solo programa a la vez (*por verificar* el mensaje de error si está ocupada; sirve para el aviso de la GUI).
2. **Carga a RAM** (modo `load`, según P3):
   ```powershell
   Measure-Command {
     & "$DSL" load -c "$W\proyecto\Proyecto_Pendulo_CPU1\targetConfigs\TMS320F28379D.ccxml" -n 0 "<ruta>\proyecto.out" `
       > $W\logs\f2_load.log 2>&1
   }
   $LASTEXITCODE
   ```
   Agregar las banderas de ejecución/reinicio que muestre `load --help`. Si el programa no arranca tras cargar, probar con la bandera de reinicio (`-r`) o de ejecución.
3. **Comprobar que corre.** Abrir el terminal serie que se usa habitualmente (o el script de Python del middleware), enviar `S` y esperar `OK` seguido de la telemetría. Es el criterio de F2 y, a la vez, prueba de que la UART quedó configurada.
4. **Verificar el reloj.** Con el método ya empleado en la tesis (medición de la tasa de tramas o periodo de la interrupción), confirmar 200 MHz. Si da 100 MHz, el símbolo de F1 no se aplicó.
5. **Verificar el comportamiento del firmware** frente a la línea base: valores de `isr_max_cycles` en el mismo rango (1378 a 1578 ciclos) y respuesta del péndulo equivalente.
6. **Repetibilidad.** Repetir los pasos 2 y 3 diez veces seguidas, incluyendo cargas con el programa ya en ejecución. Contar éxitos. Una falla intermitente es un hallazgo importante para el diseño (reintentos).
7. **Pruebas negativas** (para los mensajes de la GUI): tarjeta desconectada; `.ccxml` incorrecto; archivo `.out` inexistente; CCS con sesión abierta. Para cada una anotar el mensaje y el código de salida.

**Qué observar y registrar.** Código de salida (se espera 0 en éxito), duración, mensajes de cada paso, si hubo confirmación de verificación, y el resultado de las diez repeticiones.

**Criterio de éxito.** Código de salida 0, el programa corre y responde `OK` a `S`, reloj de 200 MHz y rendimiento equivalente a la línea base, en 10 de 10 repeticiones.

**Si falla.** (a) Error de núcleo ("Unknown core index"): comprobar que el `.ccxml` conserve los CLA habilitados, porque el índice de CPU2 depende de ellos. (b) Sonda ocupada: cerrar CCS o desconectar la sesión. (c) Carga correcta pero sin respuesta: revisar la bandera de ejecución o de reinicio. (d) Falla persistente: respaldo documentado, la GUI avisa cómo cargar con CCS.

---

## F5. Recibir comandos por UART con la interrupción activa

**Para qué.** El protocolo propuesto (`W`, `R`, `Z`, `D`) implica que el firmware reciba y procese líneas mientras la interrupción de 100 kHz sigue controlando la planta. La prueba comprueba que ese procesamiento no degrada el tiempo real.

**Por qué se espera que funcione.** Razonamiento de diseño (por verificar con la prueba): el análisis de las líneas ocurre en el bucle principal, no dentro de la interrupción, de modo que la interrupción no cambia. La interrupción mide hoy entre 1378 y 1578 ciclos de un presupuesto de 2000 (Ts = 10 µs a 200 MHz), con un margen de 422 ciclos como mínimo. El riesgo real no es la interrupción sino el bucle principal: debe vaciar el buffer de recepción a tiempo y compartir el canal de transmisión con la telemetría sin mezclar líneas.

**Requisito previo.** Una versión mínima del analizador en el firmware (comandos `W`, `R`, `D` para uno o dos parámetros de prueba). Es código de prueba; no se integra al proyecto principal hasta aprobar el protocolo.

**Procedimiento.**

1. **Línea base sin comandos.** Con la telemetría activa, registrar `isr_max_cycles` durante 60 s (mínimo, máximo y media), y el contador de *overruns*.
2. **Carga de comandos creciente.** Con un script de Python (`pyserial`) enviar, con la telemetría corriendo, a 10, 100 y 1000 comandos por segundo (según permita el baudio) durante 60 s cada nivel. Alternar `W` (escritura de una ganancia) y `R` (lectura). Registrar en cada nivel: `isr_max_cycles`, *overruns*, comandos respondidos frente a enviados y latencia comando-respuesta.
3. **Ráfaga.** Enviar de golpe más bytes de los que caben en el FIFO de recepción (el hardware tiene un FIFO de 16 niveles; confirmar en la hoja de datos). Comprobar que no se pierden bytes ni se bloquea el analizador, y revisar el indicador de desbordamiento del receptor.
4. **Integridad de las tramas.** Con telemetría y respuestas simultáneas, verificar que cada línea recibida en la PC está completa y bien formada: ninguna respuesta debe quedar insertada a mitad de una trama de telemetría. Si ocurre, se requiere una cola de transmisión única.
5. **Entradas inválidas.** Enviar basura, líneas demasiado largas, líneas sin fin de línea, parámetros desconocidos y valores fuera de rango. Esperado: `ERR` con código, sin bloqueo ni reinicio, y la planta sigue funcionando.
6. **Efecto en la planta.** Cambiar una ganancia en línea durante la ejecución y comprobar que la respuesta del péndulo cambia sin discontinuidades inesperadas (importante para decidir si el cambio debe aplicarse solo en un instante seguro del ciclo).
7. **Baudio.** Repetir el punto 2 a 115200 y a 230400 baudios (ambos con error de reloj de +0.47 % según el análisis previo); comparar la latencia.

**Qué observar y registrar.** Tablas por nivel de carga: `isr_max_cycles` (mín/máx/media), *overruns*, bytes perdidos, latencia, y mensajes de error.

**Criterio de éxito (propuesto, a acordar).** `isr_max_cycles` no aumenta más de 5 % respecto a la línea base en ningún nivel, cero *overruns*, cero bytes perdidos a la carga nominal, y respuestas siempre bien formadas. El 5 % es una propuesta; el margen disponible es de 21 % del presupuesto, por lo que el criterio es estricto a propósito.

**Si falla.** Si la interrupción se ve afectada (por ejemplo por contención del bus o por interrupciones anidadas de la UART), se pasa de sondeo a interrupción de recepción mínima con cola, midiendo de nuevo. Si se pierden bytes, se reduce la tasa de comandos o se usa el FIFO con umbral.

---

## F6. CoppeliaSim por línea de comandos y por ZeroMQ

**Para qué.** Comprobar que la GUI puede abrir CoppeliaSim con la escena correcta e iniciar la simulación sin intervención, solo para los modelos que lo requieren (`visualizacion.coppelia`).

**Por qué se espera que funcione.** *Documentado:* el manual de CoppeliaSim lista `-h` (modo sin interfaz emulado), `-H` (sin interfaz real), `-s<ms>` (iniciar la simulación; 0 desactiva el paro automático), `-q` (salir al terminar) y la carga de una escena `.ttt` como argumento. Su API ZeroMQ se instala con el paquete `coppeliasim-zmqremoteapi-client` y permite `startSimulation()` y `stopSimulation()`, además de cargar escenas. *Por verificar:* si el servidor ZeroMQ queda activo en modo `-h`, el puerto, y la integración con el middleware actual.

**Procedimiento.**

1. **Apertura con interfaz y arranque automático** (el caso más probable en las prácticas):
   ```powershell
   & "<ruta>\coppeliaSim.exe" -s0 "C:\ruta\escena_pendulo.ttt"
   ```
   Esperado: abre la escena y la simulación inicia sola, sin paro automático. Anotar si falta algún argumento.
2. **Modo sin interfaz emulado:** repetir con `-h`. Verificar que la simulación corre (por ejemplo, con el paso 3). Anotar si las vistas gráficas necesarias para esta escena se ven afectadas (con `-h` los sensores de visión no operan salvo con ciertos modos de renderizado; el péndulo no los usa, pero otros modelos sí podrían).
3. **Control por ZeroMQ:**
   ```powershell
   python -m pip install coppeliasim-zmqremoteapi-client
   ```
   Con la simulación abierta, ejecutar en Python:
   ```python
   from coppeliasim_zmqremoteapi_client import RemoteAPIClient
   client = RemoteAPIClient()          # conecta a localhost; confirmar puerto en la documentación del cliente
   sim = client.require('sim')
   print(sim.getSimulationState())
   sim.startSimulation()
   # ... esperar unos segundos
   sim.stopSimulation()
   ```
   Registrar si conecta con CoppeliaSim abierto con interfaz y con `-h`, y el tiempo que tarda el servidor en estar disponible después de lanzar el proceso (la GUI necesitará reintentar la conexión durante ese lapso).
4. **Ciclo completo con el middleware actual.** Con la tarjeta ejecutando el péndulo (F2), lanzar CoppeliaSim con la escena, arrancar el middleware existente y confirmar que el péndulo virtual sigue el ángulo medido, igual que en el flujo manual.
5. **Ciclo de vida del proceso** (se prueba desde Python, como lo haría la GUI): lanzar con `subprocess.Popen`, esperar disponibilidad, simular, y cerrar. Comprobar que no queda ningún `coppeliaSim.exe` huérfano (`Get-Process coppeliaSim`) al cerrar la aplicación de forma normal y al cerrarla de forma abrupta.
6. **Escena ausente o ruta errónea.** Anotar el comportamiento (mensaje, código de salida) para mostrar un aviso comprensible.
7. **Apertura solo cuando se requiere.** Confirmar que, con el modelo marcado como sin visualización, el flujo completo (F1 a F2) no lanza CoppeliaSim.

**Criterio de éxito.** La escena abre y la simulación inicia sin intervención; el servidor ZeroMQ responde; el middleware actual funciona con ella; no quedan procesos huérfanos.

**Si falla.** Si el servidor ZeroMQ no se activa en modo `-h`, usar la apertura con interfaz minimizada. Si el cliente no conecta, revisar el puerto y que el complemento de la API esté habilitado en esa instalación.

---

## F3. Flash frente a RAM

**Para qué.** Decidir qué modo usa la GUI por omisión. No es una prueba de si funciona, sino de cuál conviene.

**Por qué importa.** La carga a RAM no desgasta la flash y es rápida, pero se pierde al reiniciar; la flash persiste pero tarda más y se borra y reescribe en cada carga. Además, el proyecto tiene configuraciones distintas (`RAM` y `FLASH`, con archivos de enlazado diferentes), por lo que **el generador y la compilación deben elegir la configuración correspondiente** al modo.

**Procedimiento.**

1. Compilar ambas configuraciones con F1 (cambiando `-ccs.configuration`).
2. Cargar la de RAM con `dslite load` y la de flash con `dslite flash` (banderas según P3), tomando el tiempo de cinco cargas de cada una (`Measure-Command`).
3. **Comportamiento al reiniciar:**
   - Tras cargar a RAM, presionar el botón de reinicio de la tarjeta y luego desconectar y reconectar el USB. Anotar si el programa vuelve a ejecutarse.
   - Tras cargar a flash, repetir. Si no arranca solo, revisar la posición del interruptor de modo de arranque de la tarjeta según su guía de usuario (*por verificar*).
4. Anotar la diferencia de tiempo del ciclo completo (compilar + cargar + `OK`).

**Criterio.** Se documentan tiempos y comportamiento; la decisión propuesta es RAM para iterar y flash como opción explícita para dejar un programa residente. Se confirma con los datos.

---

## F4. CPU2 por línea de comandos (no bloqueante)

**Para qué.** Solo la etapa de dos núcleos la necesita. El péndulo y los modelos actuales usan CPU1.

**Por qué es la menos segura.** *Documentado:* el índice de CPU2 es `-n 2` y depende de que los CLA estén habilitados en el `.ccxml`. Un hilo de TI sin resolver reporta que CPU2 se carga pero no arranca desde `dslite` (solo CPU1 muestra ejecución). La carga de CPU2 a RAM por línea de comandos tiene problemas conocidos.

**Procedimiento.** Se realiza al final y con una imagen mínima de CPU2 (por ejemplo un parpadeo de un pin).

1. Verificar que el `.ccxml` conserve CPU1, CLA1, CPU2 y CLA2.
2. Cargar CPU1 (`-n 0`) con un programa que asigne el pin elegido a CPU2 (registro de control de CPU propietaria del GPIO) y libere el arranque de CPU2.
3. Cargar CPU2 (`-n 2`) con la imagen de prueba y la bandera de ejecución.
4. Observar el pin. Repetir con ambas imágenes en flash.
5. Anotar si CPU2 queda cargada, si arranca y en qué orden deben ejecutarse los comandos.

**Criterio.** Ambos núcleos cargados y en ejecución; o bien se documenta la limitación y la ruta alternativa (flash en ambos núcleos, o cargar CPU2 con CCS).

**Decisión.** Si falla, no afecta las etapas G1 a G4 del reporte; solo condiciona el diseño de la etapa de dos núcleos.

---

## Registro de resultados

Una fila por prueba; el detalle de cada intento está en el archivo de resultados.

| Prueba | Fecha | Resultado | Archivo de registro |
|---|---|---|---|
| F1 | 2026-11-17 | Aprobada: compilación completa sin IDE, 0 errores, mismos símbolos que la línea base (280/280) | `Resultados_F1_2026-11-17.md`, `evidencias_F1/` |
| F2 | 2026-11-17 | Aprobada: 10 de 10 cargas con `exit=0`; el middleware recibe 2000 líneas válidas a 100 tramas/s | `Resultados_F2_2026-11-17.md` |
| F6 | 2026-11-19 | Aprobada: ciclo completo con CoppeliaSim por línea de comandos, carga con DSLite y middleware; 0 procesos restantes | `Resultados_F6_2026-11-19.md` |
| F5 | — | Preparada, sin ejecutar en la tarjeta (`firmware/fuentes/f5/`) | — |
| F3 | — | Pendiente | — |
| F4 | — | Diferida | — |

## Qué cambia en los documentos según el resultado

- **F1 y F2 correctas (cumplido):** se confirma el flujo "compilar y cargar" del reporte de la GUI.
- **F1 con fallas:** se activa la alternativa de compilar con el compilador de TI directamente (Riesgos, cuadro 6).
- **F5 correcta:** se aprueba el protocolo para pasar a la etapa G3; el resultado se incorpora al reporte.
- **F5 con degradación:** el protocolo se rediseña antes de la etapa G3.
- **F6 correcta:** se confirma `visualizacion.coppelia`; se documenta el tiempo de espera para reintentar la conexión.
- **F4 sin solución:** se documenta la limitación y se ajusta la etapa de dos núcleos.
- Cualquier resultado se anota en `docs/bitacora_pruebas.md`, y los archivos de registro en esta carpeta (`docs/gui/`).
