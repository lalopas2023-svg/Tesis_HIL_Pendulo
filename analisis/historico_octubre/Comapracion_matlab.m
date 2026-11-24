% ========================================================
% Comparación del lazo cerrado: DSP vs MATLAB
% Autor: Eduardo Pascual Pérez
% Fecha: 2026-10-05
% ========================================================
clc; clear; close all;

% Cargar datos del DSP
datos = readtable('datos_perturbacion_fija.csv');
t_dsp = datos.Tiempo_s;
pos_dsp = datos.Posicion_rad;
vel_dsp = datos.Velocidad_rad_s;
u_dsp = datos.Control_u;
N = length(t_dsp);

% Parámetros (idénticos al DSP)
Ts = 10e-6;
KP = 5.0;
KD = 0.15;
KI = 2.0;
U_MIN = -10; U_MAX = 10;
THETA_MIN = -pi/2; THETA_MAX = pi/2;
g = 9.81; L = 0.3; m = 0.1; b_fric = 0.005;
J = m*L^2;
invJ = 1/J;
gL = g/L;

% Estado inicial (misma CI que el DSP)
theta = pos_dsp(1);
theta_dot = 0;
theta_prev_slow = theta;
theta_dot_meas = 0;
theta_dot_filt = 0;
error_integral = 0;
contador_derivada = 0;
u = 0;
perturbacion = 0;

% Preasignar
pos_sim = zeros(N,1);
vel_sim = zeros(N,1);
u_sim = zeros(N,1);

% Simular
for k = 1:N
    % Modelar el offset del loopback DAC->ADC medido experimentalmente
    offset_loopback = -0.014;
    theta_measured_sim = theta + offset_loopback;
    error = 0 - theta_measured_sim;
    error_integral = error_integral + error * Ts;
    error_integral = max(min(error_integral, 2), -2);

    contador_derivada = contador_derivada + 1;
    if contador_derivada >= 1000
        theta_dot_meas = (theta - theta_prev_slow) / 0.01;
        theta_prev_slow = theta;
        contador_derivada = 0;
        theta_dot_filt = 0.614 * theta_dot_filt + 0.386 * theta_dot_meas;
    end

    u = KP*error - KD*theta_dot_filt + KI*error_integral;
    u = max(min(u, U_MAX), U_MIN);

    theta = theta + theta_dot * Ts;
    theta_dot = theta_dot + Ts * (gL*theta - (b_fric*invJ*theta_dot) + invJ*(u + perturbacion));
    theta = max(min(theta, THETA_MAX), THETA_MIN);

    pos_sim(k) = theta;
    vel_sim(k) = theta_dot;
    u_sim(k) = u;
end

% Errores RMS
err_pos = rms(pos_dsp - pos_sim);
err_vel = rms(vel_dsp - vel_sim);
err_u   = rms(u_dsp - u_sim);

fprintf('\n=== COMPARACIÓN DSP vs MATLAB ===\n');
fprintf('Error RMS posición:  %.6f rad\n', err_pos);
fprintf('Error RMS velocidad: %.6f rad/s\n', err_vel);
fprintf('Error RMS control:   %.6f\n', err_u);

% Gráficas
figure('Position', [100, 100, 1000, 700]);

subplot(3,1,1);
plot(t_dsp, pos_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, pos_sim, 'r--', 'LineWidth', 1.5);
ylabel('\theta (rad)'); legend('DSP', 'MATLAB'); grid on;
title('Comparación DSP vs MATLAB');

subplot(3,1,2);
plot(t_dsp, vel_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, vel_sim, 'r--', 'LineWidth', 1.5);
ylabel('\omega (rad/s)'); grid on;

subplot(3,1,3);
plot(t_dsp, u_dsp, 'b', 'LineWidth', 1.5); hold on;
plot(t_dsp, u_sim, 'r--', 'LineWidth', 1.5);
ylabel('u'); xlabel('Tiempo (s)'); grid on;

saveas(gcf, 'C:\Users\epasc\Documents\Eddy\Maestria\Tesis\Tesis_HIL\docs\comparacion_dsp_matlab_v2_ConOffset.png');
fprintf('\nFigura guardada en docs/capturas/comparacion_dsp_matlab.png\n');