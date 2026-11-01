% ========================================================
% Comparación DSP vs MATLAB - Sin perturbación   (v2)
% Compatible con control_hil_v3.c y hil_coppelia_v2.py
%
% CAMBIOS respecto a tu versión (cada uno corrige algo que falseaba la comparación):
%  [1] Condiciones iniciales = las del FIRMWARE (theta=0.1, theta_dot=0, prev=0.1).
%      Antes se tomaban de la 1.a fila del CSV, pero la columna "Velocidad" del CSV
%      es theta_dot_FILT (derivada filtrada de la medición), NO la velocidad real de
%      la planta: usarla como theta_dot inicial metía +2.96 rad/s falsos.
%  [2] "Velocidad" simulada = theta_dot_filt (lo mismo que registra el DSP), no theta_dot.
%      Antes se comparaban dos cantidades distintas.
%  [3] Alineación en el tiempo: la fila k del CSV es el estado tras k*10 ms. Se simula
%      desde t=0 (antes se arrancaba desde la fila 1 y todo quedaba corrido 10 ms).
%  [4] Se redondea la simulación a mrad como hace el firmware (to_milli).
%  [5] Métricas separadas: transitorio (<0.6 s) y régimen permanente (>2 s).
%  [6] Opción para modelar el arranque del firmware ANTERIOR (DAC en 2048).
% ========================================================
clc; clear; close all;

% ---------------- OPCIONES ----------------
archivo                 = 'datos_sin_perturbacion.csv';
modelar_arranque_viejo  = false;  % true SOLO para CSV capturados con el firmware sin
                                  % la corrección [v3-12] (el DAC arrancaba en 2048)
offset_loopback         = 0;      % rad. 0 = lazo calibrado (ADC_CAL_B ya lo corrige)
redondear_como_dsp      = true;
T_TRANS                 = 0.6;    % s
T_PERM                  = 2.0;    % s

% --- Cargar datos del DSP ---
datos = readtable(archivo);
pos_dsp = datos.Posicion_rad;
vel_dsp = datos.Velocidad_rad_s;
u_dsp   = datos.Control_u;
N = numel(pos_dsp);
t_dsp = (1:N)' * 0.01;            % [3] fila k = k*10 ms
fprintf('Datos cargados: %d muestras, duración %.2f s\n', N, t_dsp(end));

% --- Parámetros (idénticos al firmware) ---
Ts = 10e-6;
KP = 5.0; KI = 2.0; KD = 0.15;
U_MIN = -10; U_MAX = 10;
THETA_MIN = -pi/2; THETA_MAX = pi/2;
g = 9.81; L = 0.3; m = 0.1; b_fric = 0.005;
J = m*L^2; invJ = 1/J; gL = g/L;

% --- [1] Condiciones iniciales del firmware ---
THETA_INIT      = 0.1;
theta           = THETA_INIT;
theta_dot       = 0.0;
theta_prev_slow = THETA_INIT;
theta_dot_filt  = 0.0;
error_integral  = 0.0;
contador_derivada = 0;
u = 0;

pos_sim = zeros(N,1); vel_sim = zeros(N,1); u_sim = zeros(N,1);
N_SUB = 1000;                      % 1000 ISR por fila del CSV

for k = 1:N
    for sub = 1:N_SUB
        theta_measured = theta + offset_loopback;
        if modelar_arranque_viejo && k == 1 && sub == 1
            theta_measured = 0;    % [6] el ADC leía el DAC en 2048 (= ángulo 0)
            theta_prev_slow = 0;
        end

        error = 0 - theta_measured;
        error_integral = max(min(error_integral + error*Ts, 2), -2);

        contador_derivada = contador_derivada + 1;
        if contador_derivada >= 1000
            theta_dot_meas  = (theta_measured - theta_prev_slow) / 0.01;
            theta_prev_slow = theta_measured;
            contador_derivada = 0;
            theta_dot_filt  = 0.614*theta_dot_filt + 0.386*theta_dot_meas;
        end

        u = KP*error - KD*theta_dot_filt + KI*error_integral;
        u = max(min(u, U_MAX), U_MIN);

        theta_dot_new = theta_dot + Ts*(gL*theta - (b_fric*invJ*theta_dot) + invJ*u);
        theta_new = max(min(theta + Ts*theta_dot_new, THETA_MAX), THETA_MIN);
        theta = theta_new;  theta_dot = theta_dot_new;
    end
    pos_sim(k) = theta;
    vel_sim(k) = theta_dot_filt;   % [2] lo mismo que registra el DSP
    u_sim(k)   = u;
end

if redondear_como_dsp                % [4]
    pos_sim = round(pos_sim*1000)/1000;
    vel_sim = round(vel_sim*1000)/1000;
    u_sim   = round(u_sim*1000)/1000;
end

% --- Métricas [5] ---
tr = t_dsp <= T_TRANS;  pm = t_dsp >= T_PERM;
fprintf('\n=== ERRORES RMS (DSP - MATLAB) ===\n');
fprintf('                 Posición(rad)  Velocidad(rad/s)  Control\n');
fprintf('Total         :  %.5f        %.4f            %.4f\n', rms(pos_dsp-pos_sim), rms(vel_dsp-vel_sim), rms(u_dsp-u_sim));
fprintf('Transitorio   :  %.5f        %.4f            %.4f\n', rms(pos_dsp(tr)-pos_sim(tr)), rms(vel_dsp(tr)-vel_sim(tr)), rms(u_dsp(tr)-u_sim(tr)));
fprintf('Permanente    :  %.5f        %.4f            %.4f\n', rms(pos_dsp(pm)-pos_sim(pm)), rms(vel_dsp(pm)-vel_sim(pm)), rms(u_dsp(pm)-u_sim(pm)));
fprintf('\nPosición en régimen permanente (t >= %.1f s):\n', T_PERM);
fprintf('  DSP    : media = %+.3f mrad, desv. est. = %.3f mrad\n', mean(pos_dsp(pm))*1e3, std(pos_dsp(pm))*1e3);
fprintf('  MATLAB : media = %+.3f mrad\n', mean(pos_sim(pm))*1e3);
fprintf('  (la telemetría del DSP está redondeada a 1 mrad: la media fina está en\n');
fprintf('   theta_mean_acc del Watch)\n');

% --- Gráficas ---
figure('Position', [100, 100, 1000, 700]);
subplot(3,1,1);
plot(t_dsp, pos_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, pos_sim, 'r--', 'LineWidth', 1.5);
ylabel('\theta (rad)'); legend('DSP', 'MATLAB'); grid on;
title('Comparación DSP vs MATLAB - Transitorio completo');
subplot(3,1,2);
plot(t_dsp, vel_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, vel_sim, 'r--', 'LineWidth', 1.5);
ylabel('\omega_{filt} (rad/s)'); grid on;
subplot(3,1,3);
plot(t_dsp, u_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, u_sim, 'r--', 'LineWidth', 1.5);
ylabel('u'); xlabel('Tiempo (s)'); grid on;

figure('Position', [150, 150, 1000, 400]);   % zoom al transitorio
idx = t_dsp <= 1.0;
plot(t_dsp(idx), pos_dsp(idx)*1e3, 'b.-'); hold on;
plot(t_dsp(idx), pos_sim(idx)*1e3, 'r--', 'LineWidth', 1.5);
xlabel('Tiempo (s)'); ylabel('\theta (mrad)'); legend('DSP', 'MATLAB'); grid on;
title('Zoom al transitorio (0-1 s)');
