#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import argparse
import os
import sys

def generate_dual_frequency_signal(freq1, freq2, amp1, amp2, sample_rate, duration):
    """
    Генерирует сигнал, состоящий из двух комплексных экспонент
    с заданными частотами (могут быть положительными и отрицательными)
    """
    num_samples = int(sample_rate * duration)
    t = np.arange(num_samples) / sample_rate
    
    # Сигнал = сумма двух комплексных экспонент
    signal = amp1 * np.exp(1j * 2 * np.pi * freq1 * t) + amp2 * np.exp(1j * 2 * np.pi * freq2 * t)
    
    return signal.astype(np.complex64)

def save_complex_signal(filename, signal):
    """Сохраняет комплексный сигнал в бинарный файл"""
    interleaved = np.zeros(2 * len(signal), dtype=np.float32)
    interleaved[0::2] = np.real(signal)
    interleaved[1::2] = np.imag(signal)
    interleaved.tofile(filename)

def main():
    parser = argparse.ArgumentParser(
        description='Генерация сигнала с двумя заданными частотами (положительными и отрицательными)'
    )
    
    parser.add_argument('-f1', '--freq1', type=float, required=True,
                        help='Первая частота в Гц (может быть отрицательной)')
    
    parser.add_argument('-f2', '--freq2', type=float, required=True,
                        help='Вторая частота в Гц (может быть отрицательной)')
    
    parser.add_argument('-a1', '--amp1', type=float, default=1.0,
                        help='Амплитуда первой частоты (по умолчанию: 1.0)')
    
    parser.add_argument('-a2', '--amp2', type=float, default=1.0,
                        help='Амплитуда второй частоты (по умолчанию: 1.0)')
    
    parser.add_argument('-r', '--rate', '--sample-rate', type=float, required=True,
                        dest='sample_rate',
                        help='Частота дискретизации в Гц')
    
    parser.add_argument('-d', '--duration', type=float, default=1.0,
                        help='Длительность сигнала в секундах')
    
    parser.add_argument('-o', '--output', type=str, default='dual_freq_signal.bin',
                        help='Имя выходного файла')
    
    parser.add_argument('--plot', action='store_true',
                        help='Показать спектр сигнала')
    
    args = parser.parse_args()
    
    # Проверка частоты Найквиста
    nyquist = args.sample_rate / 2
    if abs(args.freq1) > nyquist or abs(args.freq2) > nyquist:
        print(f"Предупреждение: Частоты превышают частоту Найквиста ({nyquist} Гц)")
    
    # Генерация сигнала
    signal = generate_dual_frequency_signal(
        args.freq1, args.freq2,
        args.amp1, args.amp2,
        args.sample_rate, args.duration
    )
    
    # Сохранение
    save_complex_signal(args.output, signal)
    print(f"Сигнал сохранен в: {args.output}")
    print(f"Количество отсчетов: {len(signal)}")
    print(f"\nЧастоты: {args.freq1} Гц (амплитуда {args.amp1}) и {args.freq2} Гц (амплитуда {args.amp2})")
    
    # Построение спектра
    if args.plot:
        try:
            import matplotlib.pyplot as plt
            
            # Двусторонний спектр
            freqs = np.fft.fftshift(np.fft.fftfreq(len(signal), 1/args.sample_rate))
            spectrum = np.fft.fftshift(np.fft.fft(signal))
            spectrum_db = 20 * np.log10(np.abs(spectrum) + 1e-10)
            
            plt.figure(figsize=(12, 5))
            
            plt.subplot(1, 2, 1)
            plt.plot(freqs/1000, spectrum_db)
            plt.xlabel('Частота (кГц)')
            plt.ylabel('Амплитуда (dB)')
            plt.title('Двусторонний спектр')
            plt.grid(True)
            plt.xlim([-args.sample_rate/2/1000, args.sample_rate/2/1000])
            plt.axvline(x=0, color='r', linestyle='--', alpha=0.5)
            
            # Отметим заданные частоты
            plt.axvline(x=args.freq1/1000, color='g', linestyle=':', alpha=0.7, label=f'f1={args.freq1/1000} кГц')
            plt.axvline(x=args.freq2/1000, color='b', linestyle=':', alpha=0.7, label=f'f2={args.freq2/1000} кГц')
            plt.legend()
            
            plt.subplot(1, 2, 2)
            plt.specgram(signal, NFFT=1024, Fs=args.sample_rate/1000, 
                        cmap='viridis', scale='dB')
            plt.xlabel('Время (с)')
            plt.ylabel('Частота (кГц)')
            plt.title('Спектрограмма')
            plt.colorbar(label='Интенсивность (dB)')
            
            plt.tight_layout()
            plt.savefig(args.output.replace('.bin', '_spectrum.png'))
            plt.show()
            
        except ImportError:
            print("Matplotlib не установлен, спектр не построен")

if __name__ == "__main__":
    main()