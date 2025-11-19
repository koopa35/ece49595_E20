clc;
clear;
close all;

note = 440;
Tnote = 1/note;

fsw = 35000;
Tsw = 1/fsw;

t = linspace(0, 100*Tsw, 10000);
triangle = sawtooth(2*pi*fsw.*t,1/2);
audio = sin(2*pi*note.*t);


figure;
hold on;
plot(t, triangle);
plot(t, audio);


