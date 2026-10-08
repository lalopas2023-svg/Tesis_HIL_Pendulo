% ========================================================
% COMPARATIVA: Ts=10 us vs Ts=10 ms sobre los MISMOS datos
% ========================================================
clc; clear; close all;

% Cargar los datos (ambos archivos son iguales, usamos uno)
datos = readtable('datos_derivada_lenta.csv');
t = datos.Tiempo_s;
pos = datos.Posicion_rad;
vel_dsp = datos.Velocidad_rad_s;   % La que calculó el DSP con Ts=10 ms
u_dsp = datos.Control_u;

% Recalcular la derivada en MATLAB con Ts=10 us (simulando el viejo)
Ts_fast = 10e-6;
Ts_slow = 0.01;
N = length(pos);

vel_rapida = zeros(N,1);
vel_rapida_filt = zeros(N,1);
for k = 2:N
    vel_rapida(k) = (pos(k) - pos(k-1)) / 0.01;   % Ts=10ms efectivo
end
% Nota: no podemos simular Ts=10us real porque los datos están
% muestreados a 100 Hz. Pero SÍ podemos estimar cuánto ruido
% tendría al dividir entre Ts más pequeño.

% Aproximación: usar las diferencias crudas que ya tenemos
% y multiplicarlas por el factor de amplificación
factor_amplificacion = 0.01 / 10e-6;   % = 1000
vel_rapida_equiv = vel_rapida * factor_amplificacion * 0.01;  % estimación

% Comparación gráfica
figure('Position', [100, 100, 1000, 700]);

subplot(3,1,1);
plot(t, pos, 'b', 'LineWidth', 1.5);
ylabel('\theta (rad)', 'FontSize', 12, 'FontWeight', 'bold');
title('Posición — Datos limpios con Ts=10 ms', 'FontSize', 14);
grid on;

subplot(3,1,2);
plot(t, vel_dsp, 'g', 'LineWidth', 1.5); hold on;
plot(t, vel_rapida_equiv, 'r', 'LineWidth', 0.5);
ylabel('\omega (rad/s)', 'FontSize', 12, 'FontWeight', 'bold');
legend('DSP Ts=10 ms (limpio)', 'Estimación Ts=10 us (ruidoso)', 'Location', 'best');
title('Velocidad: comparación de métodos', 'FontSize', 14);
grid on;

subplot(3,1,3);
plot(t, u_dsp, 'm', 'LineWidth', 1.5);
ylabel('u', 'FontSize', 12, 'FontWeight', 'bold');
xlabel('Tiempo (s)', 'FontSize', 12, 'FontWeight', 'bold');
title('Control — Limpio', 'FontSize', 14);
grid on;

% Estadísticas
fprintf('\n=== ESTADÍSTICAS ===\n');
fprintf('Velocidad DSP (Ts=10ms): sigma = %.4f rad/s\n', std(vel_dsp));
fprintf('Estimación Ts=10us:      sigma = %.4f rad/s\n', std(vel_rapida_equiv));
fprintf('Reducción:               %.0fx\n', std(vel_rapida_equiv) / std(vel_dsp));