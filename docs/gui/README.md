# docs/gui/

Propuesta de **interfaz gráfica y generador de configuración** para la plataforma HIL, con las pruebas que comprueban en el
laboratorio si lo propuesto puede funcionar. Es una propuesta en evaluación: nada de esto se incorpora a lo implementado hasta
que el director la apruebe. Lo que ya está implementado y probado (el péndulo) está en las demás carpetas del repositorio.

## Reportes

| Archivo | Contenido |
|---|---|
| `Reporte_diseno_GUI.tex` / `.pdf` | Borrador 0.2 para el director: diseño de la interfaz (Python, PySide6 y pyqtgraph), arquitectura, requisitos RG1 a RG14, protocolo de comandos por UART, etapas G0 a G5 y preguntas abiertas. |
| `Reporte_configuracion_modelos.tex` / `.pdf` | Borrador 0.2: cómo se configuraría un modelo (descriptor de planta, archivo YAML, plantillas) y qué parámetros son editables. |
| `Reporte_pruebas_factibilidad_GUI.tex` / `.pdf` | Reporte de prácticas (21 páginas) con lo hecho hasta el 2026-11-24: F1, F2 y F6, el desplazamiento del offset del ADC, la autocalibración y la preparación de F5, con las fuentes citadas. |

Para compilar los `.tex` hace falta `Imagenes/UTM.png` junto a cada archivo (es el logo de la portada).

## Pruebas de factibilidad

El procedimiento de cada prueba (para qué sirve, por qué se espera que funcione, comandos, criterio de aceptación y qué hacer si
falla) está en [`Guia_verificacion_factibilidad.md`](Guia_verificacion_factibilidad.md).

| Prueba | Qué comprueba | Estado | Resultados |
|---|---|---|---|
| F1 | Compilar el proyecto sin el IDE (`ccs-serverc`) | Aprobada, 2026-11-17 | [`Resultados_F1_2026-11-17.md`](Resultados_F1_2026-11-17.md), [`evidencias_F1/`](evidencias_F1/LEEME.md) |
| F2 | Cargar el programa en la tarjeta sin el IDE (DSLite) | Aprobada, 2026-11-17 | [`Resultados_F2_2026-11-17.md`](Resultados_F2_2026-11-17.md) |
| F6 | CoppeliaSim por línea de comandos y ZeroMQ | Aprobada, 2026-11-19 | [`Resultados_F6_2026-11-19.md`](Resultados_F6_2026-11-19.md) |
| F5 | Comandos por UART con la ISR activa | Preparada (`firmware/fuentes/f5/`); falta ejecutarla en la tarjeta | Guía, apartado F5 |
| F3 | Programa en flash frente a RAM | Pendiente | Guía, apartado F3 |
| F4 | Cargar CPU2 por línea de comandos | Diferida: no hace falta hasta la etapa de dos núcleos | Guía, apartado F4 |

Los hallazgos de cada día y las decisiones que cambiaron la guía están en las adendas del 2026-11-12 en adelante de
[`../bitacora_pruebas.md`](../bitacora_pruebas.md).

## Registro de la preparación

- [`registro_preparacion_2026-11-12.md`](registro_preparacion_2026-11-12.md): cómo se hizo la copia aislada del proyecto para
  las pruebas (P0), las herramientas localizadas y los hallazgos H1 a H6. Es un registro de esa fecha: nombra
  `control_hil_v3.c`, que se retiró del repositorio el 2026-11-24 (su lógica es la de `control_hil_sin_perturbacion.c`).
- [`registro_preparacion_SHA256.txt`](registro_preparacion_SHA256.txt): huellas SHA-256 de lo copiado.
