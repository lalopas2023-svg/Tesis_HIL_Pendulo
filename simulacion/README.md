# simulacion/

- **`Escena_Pendulo.ttt`** — escena de CoppeliaSim con los objetos `/pendulum_joint` y `/graph`. Debe ser la escena activa
  al ejecutar `middleware/hil_coppelia.py` (si no, falla con "object /pendulum_joint does not exist").
  CoppeliaSim puede lanzarse por línea de comandos con la escena como argumento (con o sin ventana); el servidor ZeroMQ
  responde a la primera llamada. Resultados y límites en `docs/gui/Resultados_F6_2026-11-19.md`.
