% ========================================================================
% analisis_barrido.m
% Ajusta la recta  ADC = m * DAC + b  del lazo DAC-B -> ADC-A0 a partir de lo
% que vuelca diag_adc_dac.c por UART (líneas "FIX,..." y "SW,...").
%
% USO:
%   1) Captura la salida UART (115200 8N1) en un archivo de texto,
%      p.ej. barrido_sin_adcsetmode.txt (la tabla se repite cada 3 s; basta
%      con que haya al menos una tabla completa, de "# diag" a "# END").
%   2) Cambia 'archivo' y ejecuta. Imprime m y b y las constantes para el
%      firmware (ADC_CAL_M, ADC_CAL_B).
% ========================================================================
clc; clear; close all;

archivo = 'barrido_sin_adcsetmode.txt';      % <-- cambia aquí
LSB_RAD = pi / 4095;                          % 0.7672 mrad por LSB

% ---- 1) Leer y parsear ------------------------------------------------
txt = readlines(archivo);
sw  = zeros(0,3);   % [codigo media sd]
fix = zeros(0,3);
for i = 1:numel(txt)
    tok = regexp(strtrim(txt(i)), '^(SW|FIX),(\d+),(-?\d+),(\d+)$', 'tokens', 'once');
    if isempty(tok), continue; end
    fila = [str2double(tok{2}), str2double(tok{3})/100, str2double(tok{4})/100];
    if tok{1} == "SW",  sw  = [sw;  fila]; else, fix = [fix; fila]; end
end
% La tabla se repite: me quedo con la última aparición de cada código
[~, ia] = unique(sw(:,1),  'last');  sw  = sw(sort(ia),:);
[~, ia] = unique(fix(:,1), 'last');  fix = fix(sort(ia),:);
code = sw(:,1);  mu = sw(:,2);  sd = sw(:,3);
fprintf('Puntos de barrido: %d   puntos fijos: %d\n\n', numel(code), size(fix,1));

% ---- 2) Puntos fijos: ADC - DAC en LSB --------------------------------
fprintf('=== PUNTOS FIJOS (Pruebas 1 y 2) ===\n');
fprintf('  DAC    ADC medio   ADC-DAC [LSB]   sd [LSB]   ADC-DAC [mrad]\n');
for i = 1:size(fix,1)
    d = fix(i,2) - fix(i,1);
    fprintf('%5d   %9.2f   %12.2f   %8.2f   %12.2f\n', fix(i,1), fix(i,2), d, fix(i,3), d*LSB_RAD*1e3);
end

% ---- 3) Ajuste global (zona lineal) y local (zona de operación) -------
selG = code >= 200 & code <= 3900;               % excluye extremos
pG = polyfit(code(selG), mu(selG), 1);
selL = code >= 1792 & code <= 2304;              % alrededor de theta = 0
pL = polyfit(code(selL), mu(selL), 1);

fprintf('\n=== AJUSTE GLOBAL (200..3900) ===\n  m = %.6f   b = %.3f LSB\n', pG(1), pG(2));
fprintf('=== AJUSTE LOCAL (1792..2304, tu zona de operación) ===\n  m = %.6f   b = %.3f LSB\n', pL(1), pL(2));
d2048 = polyval(pL, 2048) - 2048;
fprintf('\nOffset del lazo en el centro (código 2048): %.2f LSB = %.2f mrad\n', d2048, d2048*LSB_RAD*1e3);
% El controlador lleva theta_medido a 0, y theta_medido = theta + (ADC-DAC)[rad],
% así que theta_real en equilibrio = -(ADC-DAC)[rad].
fprintf('theta de equilibrio esperado SIN corregir: %+.2f mrad\n', -d2048*LSB_RAD*1e3);

resG = mu(selG) - polyval(pG, code(selG));
resL = mu(selL) - polyval(pL, code(selL));
fprintf('Residuo máximo global: %.2f LSB   local: %.2f LSB\n', max(abs(resG)), max(abs(resL)));

% ---- 4) Constantes para control_hil_v2.c ------------------------------
fprintf('\n=== PEGAR EN control_hil_v2.c (ajuste LOCAL) ===\n');
fprintf('#define ADC_CAL_M   %.6ff\n#define ADC_CAL_B   %.3ff\n', pL(1), pL(2));
fprintf('(si el residuo local es grande, usa un ajuste con más puntos cerca de 2048)\n');

% ---- 5) Gráficas ------------------------------------------------------
figure('Position',[100 100 1000 700]);
subplot(3,1,1);
plot(code, mu, 'o-'); hold on; plot(code, code, 'k--');
xlabel('Código DAC'); ylabel('Código ADC medio'); grid on; legend('medido','ideal (y=x)','Location','best');
title('Transferencia DAC-B \rightarrow ADC-A0');
subplot(3,1,2);
plot(code, mu - code, 'o-'); grid on;
xlabel('Código DAC'); ylabel('ADC - DAC [LSB]'); title('Error del lazo');
subplot(3,1,3);
errorbar(code(selL), mu(selL) - polyval(pL, code(selL)), sd(selL), 'o'); grid on;
xlabel('Código DAC'); ylabel('Residuo local [LSB]'); title('Residuo del ajuste local (barras = sd por punto)');
