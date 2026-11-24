% ========================================================
% ANÁLISIS MULTIVARIABLE DEL CONTROLADOR HIL
% ========================================================
clc; clear; close all;

% --- 1. IMPORTAR DATOS ---
datos = readtable('datos_tesis_pendulo.csv');
t = datos.Tiempo_s;
pos = datos.Posicion_rad;
vel = datos.Velocidad_rad_s;
u = datos.Control_u;

% --- 2. CONFIGURACIÓN DE LA FIGURA ---
figure('Name', 'Análisis Modular del Péndulo', 'Color', 'w', 'Position', [100, 100, 800, 600]);

% --- GRAFICA 1: POSICIÓN ---
subplot(3, 1, 1);
plot(t, pos, 'b', 'LineWidth', 1.5); 
hold on; grid on;
yline(0, 'r--', 'Referencia', 'LineWidth', 1);
ylabel('\theta (rad)', 'FontSize', 12, 'FontWeight', 'bold');
title('Evaluación Multivariable del Sistema HIL', 'FontSize', 14);

% --- GRAFICA 2: VELOCIDAD ANGULAR ---
subplot(3, 1, 2);
plot(t, vel, 'g', 'LineWidth', 1.5); 
hold on; grid on;
yline(0, 'k--', 'Reposo', 'LineWidth', 1);
ylabel('Velocidad (rad/s)', 'FontSize', 12, 'FontWeight', 'bold');

% --- GRAFICA 3: ESFUERZO DE CONTROL ---
subplot(3, 1, 3);
plot(t, u, 'm', 'LineWidth', 1.5); 
hold on; grid on;
% Límites de saturación física programados en tu DSP (-10 a 10)
yline(10, 'r:', 'Saturación Max (+10)', 'LineWidth', 1.5);
yline(-10, 'r:', 'Saturación Min (-10)', 'LineWidth', 1.5);
ylabel('Control (u)', 'FontSize', 12, 'FontWeight', 'bold');
xlabel('Tiempo determinista (s)', 'FontSize', 12, 'FontWeight', 'bold');
% 
% Ahora como veo que efectivamente se subió? O después de esto que sigue? Para poder compartirle el repositorio a mi director. 