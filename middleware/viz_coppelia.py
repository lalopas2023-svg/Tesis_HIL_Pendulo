# ============================================================
# Visualización en CoppeliaSim DESACOPLADA del lazo de lectura (tiempo real)
#
# PROBLEMA QUE RESUELVE: antes, por cada línea del DSP (100/s) el script hacía 4-5
# llamadas a CoppeliaSim en el MISMO hilo que lee el puerto. Si esas llamadas tardan
# más de 10 ms en total, el script se atrasa; el DSP sigue mandando y las líneas se
# acumulan en el buffer del puerto: la pantalla va cada vez más atrasada (en una
# corrida de 90 s llegó a 7.9 s de retraso).
#
# AHORA: el hilo principal solo lee, guarda el CSV (completo, 100 Hz) y "publica" la
# última muestra. Este hilo, a su propio ritmo, toma SIEMPRE la muestra más reciente y
# la manda a CoppeliaSim. Si CoppeliaSim es lento se saltan cuadros (se ve menos
# fluido), pero NUNCA se acumula retraso: lo que ves es lo de hace unos milisegundos.
# ============================================================
import threading
import time


class VisorCoppelia(threading.Thread):
    def __init__(self, con_perturbacion=False, viz_hz=50.0, grafica_hz=25.0):
        super().__init__(name='visor-coppelia', daemon=True)
        self.con_pert = con_perturbacion
        self.periodo_viz = 1.0 / viz_hz        # cada cuánto se mueve el péndulo
        self.periodo_graf = 1.0 / grafica_hz   # cada cuánto se añaden puntos a la gráfica
        self._lock = threading.Lock()
        self._ultimo = None                    # (n, t_llegada, pos, vel, u, d)
        self._fin = threading.Event()
        self.listo = threading.Event()         # conectado y simulación iniciada
        self.error = None                      # excepción si CoppeliaSim deja de responder
        self.publicadas = 0                    # muestras entregadas por el lazo de lectura
        self.enviadas = 0                      # muestras que sí llegaron a CoppeliaSim
        self.costo_tot = self.costo_max = 0.0  # duración de cada actualización (s)
        self.edad_tot = self.edad_max = 0.0    # edad del dato al terminar de enviarlo (s)

    # --- lo llama el hilo de lectura (no bloquea) ---------------------------------
    def publicar(self, pos, vel, u, d=None):
        t = time.perf_counter()
        with self._lock:
            self.publicadas += 1
            self._ultimo = (self.publicadas, t, pos, vel, u, d)

    def detener(self, espera=10.0):
        self._fin.set()
        self.join(espera)

    def resumen(self):
        if self.enviadas == 0:
            return "CoppeliaSim: no se envió ninguna muestra."
        desc = 100.0 * (self.publicadas - self.enviadas) / max(self.publicadas, 1)
        return (f"CoppeliaSim: {self.enviadas} actualizaciones de {self.publicadas} muestras "
                f"({desc:.0f} % omitidas a propósito para no atrasarse).\n"
                f"  Costo por actualización: media {1e3 * self.costo_tot / self.enviadas:.1f} ms, "
                f"máx {1e3 * self.costo_max:.1f} ms.\n"
                f"  Edad del dato al mostrarse: media {1e3 * self.edad_tot / self.enviadas:.1f} ms, "
                f"máx {1e3 * self.edad_max:.1f} ms.")

    # --- hilo --------------------------------------------------------------------
    def run(self):
        sim = None
        try:
            from coppeliasim_zmqremoteapi_client import RemoteAPIClient
            client = RemoteAPIClient()          # el cliente ZMQ se crea y usa SOLO en este hilo
            sim = client.getObject('sim')
            joint = sim.getObject('/pendulum_joint')
            graf = sim.getObject('/graph')
            id_pos = sim.addGraphStream(graf, 'Posicion (rad)', 'rad', 0, [1, 0, 0])
            id_vel = sim.addGraphStream(graf, 'Velocidad (rad/s)', 'rad/s', 0, [0, 1, 0])
            id_u = sim.addGraphStream(graf, 'Control (u)', 'u', 0, [0, 0, 1])
            id_d = (sim.addGraphStream(graf, 'Perturbacion (d)', 'u', 0, [1, 0, 1])
                    if self.con_pert else None)
            sim.startSimulation()
        except Exception as e:
            self.error = e
            self.listo.set()
            return
        self.listo.set()

        n_visto = 0
        t_graf = 0.0
        try:
            while not self._fin.is_set():
                t0 = time.perf_counter()
                with self._lock:
                    m = self._ultimo
                if m is not None and m[0] != n_visto:
                    n_visto, t_llegada, pos, vel, u, d = m
                    sim.setJointPosition(joint, pos)
                    if t0 - t_graf >= self.periodo_graf:
                        t_graf = t0
                        sim.setGraphStreamValue(graf, id_pos, pos)
                        sim.setGraphStreamValue(graf, id_vel, vel)
                        sim.setGraphStreamValue(graf, id_u, u)
                        if id_d is not None and d is not None:
                            sim.setGraphStreamValue(graf, id_d, d)
                    t1 = time.perf_counter()
                    self.enviadas += 1
                    self.costo_tot += t1 - t0
                    self.costo_max = max(self.costo_max, t1 - t0)
                    edad = t1 - t_llegada
                    self.edad_tot += edad
                    self.edad_max = max(self.edad_max, edad)
                resto = self.periodo_viz - (time.perf_counter() - t0)
                if resto > 0:
                    self._fin.wait(resto)
        except Exception as e:
            self.error = e
        finally:
            try:
                sim.stopSimulation()
            except Exception as e:
                print(f"(aviso) no se pudo detener CoppeliaSim: {e}")
