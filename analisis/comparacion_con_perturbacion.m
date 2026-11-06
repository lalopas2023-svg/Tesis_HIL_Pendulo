% ========================================================================
% Comparación DSP vs MATLAB - CON perturbación            (v1)
% Compatible con control_perturbacion.c y hil_coppelia_perturbacion.py
%
% IDEA: el CSV trae la perturbación d(t) que realmente entró a la planta
% (columna "Perturbacion", ya filtrada por el DSP). Se le pasa a la simulación
% de MATLAB EN CADA INSTANTE, así que potenciómetro o escalón da igual: ambos
% lazos reciben exactamente la misma entrada y se pueden comparar.
%
% Detalle de tiempos (para que la comparación sea exacta):
%  - El DSP actualiza d cada 10 ms, desfasado 5 ms de la telemetría. La fila k del
%    CSV trae el d que entró en la 2.ª mitad del intervalo k; en la 1.ª mitad
%    estaba el de la fila k-1.  [opción usar_fase_5ms]
%  - Fila k = estado tras k*10 ms (el tiempo del CSV, Tiempo_s, empieza en 0).
%  - La perturbación del CSV está redondeada a 1 mrad (resolución de la trama).
% ========================================================================
clc; clear; close all;

% ---------------- OPCIONES ----------------
archivo               = 'datos_con_perturbacion.csv';
usar_fase_5ms         = true;    % true: d(k-1) en los primeros 500 pasos y d(k) en los otros 500
                                 % false: retención simple (d(k) todo el intervalo)
derivada_inicial_csv  = false;   % true: replica el arranque del DSP (ver nota abajo)
redondear_como_dsp    = true;
T_TRANS               = 0.6;     % s  (transitorio inicial)
T_PERM                = 2.0;     % s  (a partir de aquí: "régimen")
% NOTA sobre derivada_inicial_csv: en las capturas con control_perturbacion.c la 1.ª
% velocidad filtrada del DSP sale ~ -2 rad/s (el modelo da ~ -0.1). Con true, la
% simulación arranca con el valor previo de la derivada que reproduce ESA primera
% velocidad (prev = theta(10ms) - vel_dsp(1)*0.01/0.386). Sirve para comprobar si
% ese arranque explica el transitorio. Con false se simula el arranque ideal.

% --- Cargar datos del DSP ---
datos   = readtable(archivo);
pos_dsp = datos.Posicion_rad;
vel_dsp = datos.Velocidad_rad_s;
u_dsp   = datos.Control_u;
d_dsp   = datos.Perturbacion;
N = numel(pos_dsp);
t_dsp = (1:N)' * 0.01;
fprintf('Datos cargados: %d muestras, duración %.2f s\n', N, t_dsp(end));
fprintf('Perturbación: min %+.3f  max %+.3f\n', min(d_dsp), max(d_dsp));

% --- Parámetros (idénticos al firmware) ---
Ts = 10e-6;
KP = 5.0; KI = 2.0; KD = 0.15;
U_MIN = -10; U_MAX = 10;
THETA_MIN = -pi/2; THETA_MAX = pi/2;
g = 9.81; L = 0.3; m = 0.1; b_fric = 0.005;
J = m*L^2; invJ = 1/J; gL = g/L;

% --- Condiciones iniciales del firmware ---
THETA_INIT      = 0.1;
theta           = THETA_INIT;
theta_dot       = 0.0;
theta_prev_slow = THETA_INIT;
theta_dot_filt  = 0.0;
error_integral  = 0.0;
contador_derivada = 0;
u = 0;
primera_derivada = true;

pos_sim = zeros(N,1); vel_sim = zeros(N,1); u_sim = zeros(N,1);
N_SUB = 1000;                      % 1000 ISR por fila del CSV

for k = 1:N
    for sub = 1:N_SUB
        % --- perturbación vigente en este paso ---
        if usar_fase_5ms
            if k > 1 && sub <= 500
                d = d_dsp(k-1);
            else
                d = d_dsp(k);
            end
        else
            d = d_dsp(k);
        end

        theta_measured = theta;                     % lazo calibrado: sin offset
        error = 0 - theta_measured;
        error_integral = max(min(error_integral + error*Ts, 2), -2);

        contador_derivada = contador_derivada + 1;
        if contador_derivada >= 1000
            if derivada_inicial_csv && primera_derivada
                theta_prev_slow = theta_measured - vel_dsp(1)*0.01/0.386;
            end
            primera_derivada = false;
            theta_dot_meas  = (theta_measured - theta_prev_slow) / 0.01;
            theta_prev_slow = theta_measured;
            contador_derivada = 0;
            theta_dot_filt  = 0.614*theta_dot_filt + 0.386*theta_dot_meas;
        end

        u = KP*error - KD*theta_dot_filt + KI*error_integral;
        u = max(min(u, U_MAX), U_MIN);

        % planta con perturbación: invJ*(u + d)
        theta_dot_new = theta_dot + Ts*(gL*theta - (b_fric*invJ*theta_dot) + invJ*(u + d));
        theta_new = max(min(theta + Ts*theta_dot_new, THETA_MAX), THETA_MIN);
        theta = theta_new;  theta_dot = theta_dot_new;
    end
    pos_sim(k) = theta;
    vel_sim(k) = theta_dot_filt;
    u_sim(k)   = u;
end

if redondear_como_dsp
    pos_sim = round(pos_sim*1000)/1000;
    vel_sim = round(vel_sim*1000)/1000;
    u_sim   = round(u_sim*1000)/1000;
end

% --- Métricas ---
tr = t_dsp <= T_TRANS;  pm = t_dsp >= T_PERM;
fprintf('\n=== ERRORES RMS (DSP - MATLAB) ===\n');
fprintf('                 Posición(rad)  Velocidad(rad/s)  Control\n');
fprintf('Total         :  %.5f        %.4f            %.4f\n', rms(pos_dsp-pos_sim), rms(vel_dsp-vel_sim), rms(u_dsp-u_sim));
fprintf('Transitorio   :  %.5f        %.4f            %.4f\n', rms(pos_dsp(tr)-pos_sim(tr)), rms(vel_dsp(tr)-vel_sim(tr)), rms(u_dsp(tr)-u_sim(tr)));
fprintf('Resto (>=%.1fs):  %.5f        %.4f            %.4f\n', T_PERM, rms(pos_dsp(pm)-pos_sim(pm)), rms(vel_dsp(pm)-vel_sim(pm)), rms(u_dsp(pm)-u_sim(pm)));
fprintf('\nPicos de theta: DSP %+.1f / %+.1f mrad   MATLAB %+.1f / %+.1f mrad\n', ...
        max(pos_dsp)*1e3, min(pos_dsp)*1e3, max(pos_sim)*1e3, min(pos_sim)*1e3);
fprintf('Resolución de la telemetría: 1 mrad  (1 LSB del ADC = 0.767 mrad)\n');

% --- Rechazo de la perturbación: en tramos con d casi constante, u debe ser = -d ---
fprintf('\n=== TRAMOS CON PERTURBACIÓN CONSTANTE (>= 3 s) ===\n');
fprintf('  t_ini  t_fin     d      theta[mrad]   u      u+d\n');
i = 1;
while i <= N
    j = i;
    while j < N && abs(d_dsp(j+1) - d_dsp(i)) < 0.01, j = j + 1; end
    if t_dsp(j) - t_dsp(i) >= 3.0
        w = max(i, j-150):j;                        % últimos 1.5 s del tramo
        fprintf('%7.1f %6.1f  %+.3f   %+8.2f   %+.3f  %+.4f\n', t_dsp(i), t_dsp(j), ...
                mean(d_dsp(w)), mean(pos_dsp(w))*1e3, mean(u_dsp(w)), mean(u_dsp(w))+mean(d_dsp(w)));
    end
    i = j + 1;
end

% --- Gráficas ---
figure('Position', [100, 100, 1000, 850]);
subplot(4,1,1);
plot(t_dsp, pos_dsp*1e3, 'b', 'LineWidth', 1.3); hold on;
plot(t_dsp, pos_sim*1e3, 'r--', 'LineWidth', 1.3);
ylabel('\theta (mrad)'); legend('DSP', 'MATLAB'); grid on;
title('Comparación DSP vs MATLAB - Con perturbación');
subplot(4,1,2);
plot(t_dsp, vel_dsp, 'b', 'LineWidth', 1.3); hold on;
plot(t_dsp, vel_sim, 'r--', 'LineWidth', 1.3);
ylabel('\omega_{filt} (rad/s)'); grid on;
subplot(4,1,3);
plot(t_dsp, u_dsp, 'b', 'LineWidth', 1.3); hold on;
plot(t_dsp, u_sim, 'r--', 'LineWidth', 1.3);
ylabel('u'); grid on;
subplot(4,1,4);
plot(t_dsp, d_dsp, 'g', 'LineWidth', 1.3);
ylabel('d (perturbación)'); xlabel('Tiempo (s)'); grid on;

figure('Position', [150, 150, 1000, 400]);           % zoom al arranque
idx = t_dsp <= 1.0;
plot(t_dsp(idx), pos_dsp(idx)*1e3, 'b.-'); hold on;
plot(t_dsp(idx), pos_sim(idx)*1e3, 'r--', 'LineWidth', 1.5);
xlabel('Tiempo (s)'); ylabel('\theta (mrad)'); legend('DSP', 'MATLAB'); grid on;
title('Zoom al transitorio inicial (0-1 s)');