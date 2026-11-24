# Resultados de F6: CoppeliaSim por línea de comandos y ZeroMQ (2026-11-19)

Equipo `lap-pascual`. CoppeliaSim Edu (`C:\Program Files\CoppeliaRobotics\CoppeliaSimEdu\coppeliaSim.exe`), cliente `coppeliasim_zmqremoteapi_client` 2.0.4, escena `Tesis_HIL\simulacion\Escena_Pendulo.ttt` (17 nov). Existe otra copia más antigua en `Descargas\simulacion` (1 nov, tamaño distinto); no se usó.

## Hallazgo previo de lectura de código
`viz_coppelia.py` ya usa la API ZeroMQ: `RemoteAPIClient().getObject('sim')`, `getObject('/pendulum_joint')`, `getObject('/graph')`, `addGraphStream`, `startSimulation`, `setJointPosition`, `stopSimulation`. El control por ZeroMQ (paso 3 de la guía) y el ciclo con la tarjeta (paso 4) ya estaban en uso en todas las corridas anteriores. El visor es pasivo: fija la posición del joint con lo que llega del DSP; la simulación no calcula el péndulo.

## Resultados
| Prueba | Resultado |
|---|---|
| Lanzar con ventana y escena como argumento | Cliente ZMQ (puerto por omisión) responde; lee `/pendulum_joint` (15) y `/graph` (17); inicia y detiene la simulación. Primer puerto en escucha (19997) a los 4.5 s |
| Lanzar con `-h` (sin ventana) y escena | Responde igual; primeros puertos (19997 y 23409) a los 0.8 s; simulación corre |
| Cierre con ventana (`CloseMainWindow`) | Cerró solo en 9.4 s, 0 procesos restantes |
| Cierre sin ventana | **No cierra solo**: hubo que forzar a los 15 s (0 procesos restantes después) |
| Escena inexistente (con ventana) | CoppeliaSim arranca igual (escena vacía) y el servidor responde; el fallo aparece al pedir `/pendulum_joint`: `1189: in sim._getObject: object does not exist, or alias/path is ill formatted` (el mismo mensaje que imprime el middleware). No hubo error en el lanzamiento |

Notas y limitaciones:
- El sondeo de puertos se detuvo en el primer puerto que apareció, así que **no se midió con precisión cuándo queda listo el servidor ZMQ**: respondió a la primera llamada del cliente en las tres pruebas (cota superior: unos 6 s con ventana y 2 s sin ventana). Los puertos listados varían entre lanzamientos (23409, 23411) y no se interpretaron.
- Tiempo simulado tras 3 s reales: 53.55 s con ventana y 519.05 s sin ventana (la simulación corre sin sincronía a tiempo real, unas 18 y 173 veces más rápido). El visor no depende de eso, pero el eje de tiempo de `/graph` es el simulado. El CSV de la tesis sale del puerto serie.
- No se registró lo observado en pantalla (ventana en `-h`, diálogos con escena inexistente). El resultado del último `Cerrar` no se pegó.

## Consecuencias para la GUI
- Esperar el servidor reintentando la conexión (no una pausa fija) y con tiempo máximo.
- Con escena inexistente CoppeliaSim no avisa: comprobar que el archivo existe antes de lanzar y verificar `/pendulum_joint` y `/graph` al conectar.
- En modo `-h` el cierre debe ser por terminación del proceso.

## Observaciones del usuario (2026-11-19 09:04)
- Paso 2 (`-h`): solo se vio una ventana de terminal de CoppeliaSim, ninguna ventana gráfica.
- Paso 4 (escena inexistente): aparecieron mensajes de error en la ventana de CoppeliaSim; esa instancia hubo que cerrarla a mano.

## Ciclo completo (CoppeliaSim lanzado por línea de comandos + DSLite + middleware)
Lanzamiento con la escena como argumento, espera de 12 s, carga con DSLite (`Success`, exit 0), middleware con CoppeliaSim: `OK`, 2000 líneas válidas, 0 ignoradas, 20.00 s por conteo y 20.08 s de reloj real, buffer serie máximo 44 bytes, 638 actualizaciones de la escena (costo medio 5.0 ms, máximo 26.1 ms). Cierre con `CloseMainWindow` y 0 procesos restantes. No se registró si el péndulo de la escena se movió.

## Veredicto de F6
**Aprobada**: la escena se abre por línea de comandos, el servidor ZeroMQ responde, el middleware existente funciona contra esa instancia y el cierre no deja procesos. Pendientes menores: tiempo exacto de disponibilidad del servidor (se usará reintento con tiempo máximo) y huérfanos ante cierre abrupto de la aplicación que lanza (se prueba con la GUI).
