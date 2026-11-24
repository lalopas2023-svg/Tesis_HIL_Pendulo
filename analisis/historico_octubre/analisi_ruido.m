% ========================================================
% ANÁLISIS DE RUIDO PURO DEL ADC
% ========================================================
clc; clear; close all;

datos = readtable('datos_tesis_pendulo.csv');
pos = datos.Posicion_rad;
t = datos.Tiempo_s;

% Estadísticas
media_r = mean(pos);
sigma_r = std(pos);
p2p_r = max(pos) - min(pos);

fprintf('\n=== RUIDO PURO DEL ADC ===\n');
fprintf('Media:          %.6f rad (%.3f mrad)\n', media_r, media_r*1000);
fprintf('Desv. est.:     %.6f rad (%.3f mrad)\n', sigma_r, sigma_r*1000);
fprintf('Pico a pico:    %.6f rad (%.3f mrad)\n', p2p_r, p2p_r*1000);
fprintf('N muestras:     %d\n', length(pos));

% Proyección a la velocidad
Ts_actual = 10e-6;
Ts_nuevo  = 0.01;

sigma_vel_actual = sigma_r * sqrt(2) / Ts_actual;
sigma_vel_nuevo  = sigma_r * sqrt(2) / Ts_nuevo;

fprintf('\n=== PROYECCIÓN A LA VELOCIDAD ===\n');
fprintf('Con Ts=10 us:  sigma_vel = %.1f rad/s\n', sigma_vel_actual);
fprintf('Con Ts=10 ms:  sigma_vel = %.3f rad/s\n', sigma_vel_nuevo);
fprintf('Reducción:     %.0fx\n', sigma_vel_actual / sigma_vel_nuevo);

% Gráficas
figure('Position', [100, 100, 900, 600]);

subplot(2,1,1);
plot(t, pos, 'b');
xlabel('Tiempo (s)'); ylabel('\theta (rad)');
title(sprintf('Ruido del ADC: \\sigma = %.3f mrad', sigma_r*1000));
grid on;

subplot(2,1,2);
histogram(pos, 50);
xlabel('\theta (rad)'); ylabel('Frecuencia');
title('Distribución del ruido');
grid on;