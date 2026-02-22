#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import argparse
import os
import sys

def generate_harmonic_signal(frequency, sample_rate, duration, amplitude=1.0, phase=0.0):
    """
    Генерирует гармонический сигнал (синусоиду)
    
    Параметры:
    frequency - частота сигнала в Гц
    sample_rate - частота дискретизации в Гц
    duration - длительность сигнала в секундах
    amplitude - амплитуда сигнала (по умолчанию 1.0)
    phase - начальная фаза в радианах (по умолчанию 0)
    
    Возвращает:
    numpy array с сигналом в формате float32
    """
    # Количество отсчетов
    num_samples = int(sample_rate * duration)
    
    # Временные метки
    t = np.arange(num_samples) / sample_rate
    
    # Генерация синусоидального сигнала
    signal = amplitude * np.sin(2 * np.pi * frequency * t + phase)
    
    # Преобразование в float32
    signal = signal.astype(np.float32)
    
    return signal

def main():
    # Настройка парсера аргументов командной строки
    parser = argparse.ArgumentParser(
        description='Генерация гармонического сигнала и запись в бинарный файл float32'
    )
    
    parser.add_argument('-f', '--frequency', type=float, required=True,
                        help='Частота сигнала в Гц')
    
    parser.add_argument('-r', '--rate', '--sample-rate', type=float, required=True,
                        dest='sample_rate',
                        help='Частота дискретизации в Гц')
    
    parser.add_argument('-d', '--duration', type=float, default=1.0,
                        help='Длительность сигнала в секундах (по умолчанию: 1.0)')
    
    parser.add_argument('-a', '--amplitude', type=float, default=1.0,
                        help='Амплитуда сигнала (по умолчанию: 1.0)')
    
    parser.add_argument('-p', '--phase', type=float, default=0.0,
                        help='Начальная фаза в радианах (по умолчанию: 0.0)')
    
    parser.add_argument('-o', '--output', type=str, default='sin_signal.bin',
                        help='Имя выходного файла (по умолчанию: signal.bin)')
    
    parser.add_argument('--info', action='store_true',
                        help='Показать информацию о сгенерированном сигнале')
    
    # Парсинг аргументов
    args = parser.parse_args()
    
    # Проверка аргументов
    if args.frequency <= 0:
        print("Ошибка: Частота сигнала должна быть положительной")
        sys.exit(1)
    
    if args.sample_rate <= 0:
        print("Ошибка: Частота дискретизации должна быть положительной")
        sys.exit(1)
    
    if args.frequency > args.sample_rate / 2:
        print("Предупреждение: Частота сигнала превышает частоту Найквиста")
        print(f"Максимальная допустимая частота: {args.sample_rate/2} Гц")
    
    if args.duration <= 0:
        print("Ошибка: Длительность должна быть положительной")
        sys.exit(1)
    
    # Генерация сигнала
    print(f"Генерация сигнала частотой {args.frequency} Гц...")
    signal = generate_harmonic_signal(
        args.frequency, 
        args.sample_rate, 
        args.duration,
        args.amplitude,
        args.phase
    )
    
    # Запись в бинарный файл
    try:
        signal.tofile(args.output)
        file_size = os.path.getsize(args.output)
        print(f"Сигнал успешно записан в файл: {args.output}")
        print(f"Размер файла: {file_size} байт")
        print(f"Количество отсчетов: {len(signal)}")
    except IOError as e:
        print(f"Ошибка при записи файла: {e}")
        sys.exit(1)
    
    # Дополнительная информация, если запрошена
    if args.info:
        print("\nИнформация о сигнале:")
        print(f"  Частота сигнала: {args.frequency} Гц")
        print(f"  Частота дискретизации: {args.sample_rate} Гц")
        print(f"  Длительность: {args.duration} с")
        print(f"  Амплитуда: {args.amplitude}")
        print(f"  Начальная фаза: {args.phase} рад")
        print(f"  Количество отсчетов: {len(signal)}")
        print(f"  Длительность сигнала: {len(signal)/args.sample_rate:.6f} с")
        print(f"  Минимальное значение: {np.min(signal):.6f}")
        print(f"  Максимальное значение: {np.max(signal):.6f}")
        print(f"  Среднее значение: {np.mean(signal):.6f}")
        print(f"  СКО: {np.std(signal):.6f}")
        
        # Проверка соответствия теоретическим значениям
        theoretical_rms = args.amplitude / np.sqrt(2)
        print(f"  Теоретическое СКО (для синуса): {theoretical_rms:.6f}")

if __name__ == "__main__":
    main()