% ========================================================
% ANÁLISIS DE RESPUESTA HIL - PÉNDULO (TESIS)
% ========================================================
clc; clear; close all;

% --- 1. EXTRACCIÓN DE DATOS ---
datos = readtable('datos_tesis_pendulo.csv');
t = datos.Tiempo_s;       
theta = datos.Angulo_rad; 

% --- 2. ANÁLISIS MATEMÁTICO (MÉTRICAS) ---
% El set-point (referencia) es 0 radianes (péndulo vertical)
error = 0 - theta;

% Cálculo del Error Cuadrático Integral (ISE) usando integración numérica (Trapezoidal)
ISE_total = trapz(t, error.^2);
sobreimpulso_max = max(abs(theta));

% Impresión de resultados en consola
fprintf('=== RESULTADOS DEL CONTROL PID ===\n');
fprintf('Error Cuadrático Integral (ISE): %.4f\n', ISE_total);
fprintf('Desviación Máxima: %.4f rad\n', sobreimpulso_max);

% --- 3. VISUALIZACIÓN ACADÉMICA ---
figure('Name', 'Respuesta del Péndulo', 'Color', 'w');
plot(t, theta, 'b-', 'LineWidth', 1.5);
hold on;

% Línea de referencia
yline(0, 'r--', 'Referencia (0 rad)', 'LineWidth', 1.5, 'LabelHorizontalAlignment', 'left');

% Formato tipográfico
grid on;
ax = gca;
ax.FontSize = 12;
xlabel('Tiempo (s)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('\theta (rad)', 'FontSize', 12, 'FontWeight', 'bold'); % Renderiza símbolo LaTeX
title('Respuesta Transitoria del Controlador HIL', 'FontSize', 14);
legend('Posición real medido', 'Set-point', 'Location', 'northeast');