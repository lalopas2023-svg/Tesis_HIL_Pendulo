% ========================================================
% Comparación DSP vs MATLAB - Sin perturbación
% Captura del transitorio completo con offset modelado
% ========================================================
clc; clear; close all;

% --- Cargar datos del DSP ---
datos = readtable('datos_sin_perturbacion.csv');
t_dsp    = datos.Tiempo_s;
pos_dsp  = datos.Posicion_rad;
vel_dsp  = datos.Velocidad_rad_s;
u_dsp    = datos.Control_u;
N = length(t_dsp);

fprintf('Datos cargados: %d muestras, duración %.2f s\n', N, t_dsp(end));
fprintf('Estado inicial DSP: theta=%.4f rad, theta_dot=%.4f rad/s\n', ...
        pos_dsp(1), vel_dsp(1));

% --- Parámetros (idénticos al firmware) ---
Ts = 10e-6;
KP = 5.0; KI = 2.0; KD = 0.15;
U_MIN = -10; U_MAX = 10;
THETA_MIN = -pi/2; THETA_MAX = pi/2;
g = 9.81; L = 0.3; m = 0.1; b_fric = 0.005;
J = m*L^2;
invJ = 1/J;
gL = g/L;

% --- Offset del loopback DAC-ADC ---
offset_loopback = 0; %-0.014

% --- Condiciones iniciales: EMPEZAR IGUAL QUE EL DSP ---
theta           = pos_dsp(1);
theta_dot       = vel_dsp(1);
theta_prev_slow = theta;
theta_dot_meas  = 0.0;
theta_dot_filt  = 0.0;
error_integral  = 0.0;
contador_derivada = 0;
u = 0;

% --- Preasignar ---
pos_sim = zeros(N,1);
vel_sim = zeros(N,1);
u_sim   = zeros(N,1);

% --- Simulación con sub-pasos (1000 por muestra del CSV) ---
N_SUB = 1000;   % el DSP hace 1000 ISR por cada fila del CSV

for k = 1:N
    for sub = 1:N_SUB
        % Modelar el offset del loopback
        theta_measured = theta + offset_loopback;

        % PID
        error = 0 - theta_measured;
        error_integral = error_integral + error * Ts;
        error_integral = max(min(error_integral, 2), -2);

        % Derivada lenta (cada 1000 ISR = 10 ms)
        contador_derivada = contador_derivada + 1;
        if contador_derivada >= 1000
            theta_dot_meas = (theta_measured - theta_prev_slow) / 0.01;
            theta_prev_slow = theta_measured;
            contador_derivada = 0;
            theta_dot_filt = 0.614 * theta_dot_filt + 0.386 * theta_dot_meas;
        end

        u = KP*error - KD*theta_dot_filt + KI*error_integral;
        u = max(min(u, U_MAX), U_MIN);

        % Planta (Euler-Cromer)
        theta_dot_new = theta_dot + Ts * (gL*theta - (b_fric*invJ*theta_dot) + invJ*u);
        theta_new = theta + Ts * theta_dot_new;
        theta_new = max(min(theta_new, THETA_MAX), THETA_MIN);

        theta = theta_new;
        theta_dot = theta_dot_new;
    end

    % Guardar el estado cada 10 ms (una vez por fila del CSV)
    pos_sim(k) = theta;
    vel_sim(k) = theta_dot;
    u_sim(k)   = u;
end

% --- Comparación ---
err_pos = rms(pos_dsp - pos_sim);
err_vel = rms(vel_dsp - vel_sim);
err_u   = rms(u_dsp - u_sim);

fprintf('\n=== ERRORES RMS ===\n');
fprintf('Posición:  %.6f rad\n', err_pos);
fprintf('Velocidad: %.6f rad/s\n', err_vel);
fprintf('Control:   %.6f\n', err_u);

% --- Gráficas ---
figure('Position', [100, 100, 1000, 700]);

subplot(3,1,1);
plot(t_dsp, pos_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, pos_sim, 'r--', 'LineWidth', 1.5);
ylabel('\theta (rad)', 'FontSize', 12, 'FontWeight', 'bold');
legend('DSP', 'MATLAB', 'Location', 'best');
title('Comparación DSP vs MATLAB - Transitorio completo', 'FontSize', 14);
grid on;

subplot(3,1,2);
plot(t_dsp, vel_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, vel_sim, 'r--', 'LineWidth', 1.5);
ylabel('\omega (rad/s)', 'FontSize', 12, 'FontWeight', 'bold');
grid on;

subplot(3,1,3);
plot(t_dsp, u_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, u_sim, 'r--', 'LineWidth', 1.5);
ylabel('u', 'FontSize', 12, 'FontWeight', 'bold');
xlabel('Tiempo (s)', 'FontSize', 12, 'FontWeight', 'bold');
grid on;

% --- Guardar figura ---
ruta_fig = 'C:\Users\epasc\Documents\Eddy\Maestria\Tesis\Tesis_HIL\docs\capturas\comparacion_transitorio.png';
carpeta = fileparts(ruta_fig);
if ~exist(carpeta, 'dir')
    mkdir(carpeta);
end
saveas(gcf, ruta_fig);
fprintf('\nFigura guardada en: %s\n', ruta_fig);