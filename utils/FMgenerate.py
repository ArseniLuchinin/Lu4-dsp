#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import argparse
import os
import sys

def generate_fm_signal(carrier_frequency, modulating_frequency, sample_rate, 
                       duration, deviation, amplitude=1.0, carrier_phase=0.0, modulating_phase=0.0):
    """
    Генерирует FM сигнал (частотная модуляция)
    
    Параметры:
    carrier_frequency - несущая частота в Гц
    modulating_frequency - частота модулирующего сигнала (косинуса) в Гц
    sample_rate - частота дискретизации в Гц
    duration - длительность сигнала в секундах
    deviation - девиация частоты (максимальное отклонение от несущей) в Гц
    amplitude - амплитуда сигнала (по умолчанию 1.0)
    carrier_phase - начальная фаза несущей в радианах (по умолчанию 0)
    modulating_phase - начальная фаза модулирующего сигнала в радианах (по умолчанию 0)
    
    Возвращает:
    numpy array с сигналом в формате float32
    """
    # Количество отсчетов
    num_samples = int(sample_rate * duration)
    
    # Временные метки
    t = np.arange(num_samples) / sample_rate
    
    # Индекс модуляции (для информации)
    modulation_index = deviation / modulating_frequency if modulating_frequency > 0 else 0
    
    # Мгновенная частота: f(t) = carrier_frequency + deviation * cos(2π * modulating_frequency * t + modulating_phase)
    # Фаза как интеграл от мгновенной частоты: φ(t) = 2π * carrier_frequency * t + (deviation/modulating_frequency) * sin(2π * modulating_frequency * t + modulating_phase)
    
    if modulating_frequency > 0:
        # Фаза модулирующего сигнала
        modulating_signal_phase = 2 * np.pi * modulating_frequency * t + modulating_phase
        # Фаза FM сигнала
        signal_phase = 2 * np.pi * carrier_frequency * t + \
                      (deviation / modulating_frequency) * np.sin(modulating_signal_phase) + \
                      carrier_phase
    else:
        # Если частота модуляции равна 0, это просто синусоида с постоянной частотой
        signal_phase = 2 * np.pi * carrier_frequency * t + carrier_phase
    
    # Генерация FM сигнала
    signal = amplitude * np.sin(signal_phase)
    
    # Преобразование в float32
    signal = signal.astype(np.float32)
    
    return signal, modulation_index

def main():
    # Настройка парсера аргументов командной строки
    parser = argparse.ArgumentParser(
        description='Генерация FM сигнала (частотная модуляция) и запись в бинарный файл float32'
    )
    
    parser.add_argument('-c', '--carrier', type=float, required=True,
                        help='Несущая частота в Гц')
    
    parser.add_argument('-m', '--modulating', type=float, required=True,
                        dest='modulating_frequency',
                        help='Частота модулирующего сигнала (косинуса) в Гц')
    
    parser.add_argument('-d', '--deviation', type=float, required=True,
                        help='Девиация частоты (максимальное отклонение от несущей) в Гц')
    
    parser.add_argument('-r', '--rate', '--sample-rate', type=float, required=True,
                        dest='sample_rate',
                        help='Частота дискретизации в Гц')
    
    parser.add_argument('-t', '--duration', type=float, default=1.0,
                        help='Длительность сигнала в секундах (по умолчанию: 1.0)')
    
    parser.add_argument('-a', '--amplitude', type=float, default=1.0,
                        help='Амплитуда сигнала (по умолчанию: 1.0)')
    
    parser.add_argument('--carrier-phase', type=float, default=0.0,
                        help='Начальная фаза несущей в радианах (по умолчанию: 0.0)')
    
    parser.add_argument('--modulating-phase', type=float, default=0.0,
                        help='Начальная фаза модулирующего сигнала в радианах (по умолчанию: 0.0)')
    
    parser.add_argument('-o', '--output', type=str, default='fm_signal.bin',
                        help='Имя выходного файла (по умолчанию: fm_signal.bin)')
    
    parser.add_argument('--info', action='store_true',
                        help='Показать информацию о сгенерированном сигнале')
    
    # Парсинг аргументов
    args = parser.parse_args()
    
    # Проверка аргументов
    if args.carrier <= 0:
        print("Ошибка: Несущая частота должна быть положительной")
        sys.exit(1)
    
    if args.modulating_frequency < 0:
        print("Ошибка: Частота модулирующего сигнала должна быть неотрицательной")
        sys.exit(1)
    
    if args.deviation <= 0:
        print("Ошибка: Девиация частоты должна быть положительной")
        sys.exit(1)
    
    if args.sample_rate <= 0:
        print("Ошибка: Частота дискретизации должна быть положительной")
        sys.exit(1)
    
    max_frequency = args.carrier + args.deviation
    if max_frequency > args.sample_rate / 2:
        print("Предупреждение: Максимальная частота сигнала превышает частоту Найквиста")
        print(f"Максимальная частота сигнала: {max_frequency} Гц")
        print(f"Частота Найквиста: {args.sample_rate/2} Гц")
    
    if args.duration <= 0:
        print("Ошибка: Длительность должна быть положительной")
        sys.exit(1)
    
    # Генерация сигнала
    print(f"Генерация FM сигнала с несущей {args.carrier} Гц и модуляцией {args.modulating_frequency} Гц...")
    signal, modulation_index = generate_fm_signal(
        args.carrier,
        args.modulating_frequency,
        args.sample_rate,
        args.duration,
        args.deviation,
        args.amplitude,
        args.carrier_phase,
        args.modulating_phase
    )
    
    # Запись в бинарный файл
    try:
        signal.tofile(args.output)
        file_size = os.path.getsize(args.output) / (1024 ** 3) 
        print(f"Сигнал успешно записан в файл: {args.output}")
        print(f"Размер файла: {file_size} ГБ")
        print(f"Количество отсчетов: {len(signal)}")
    except IOError as e:
        print(f"Ошибка при записи файла: {e}")
        sys.exit(1)
    
    # Дополнительная информация, если запрошена
    if args.info:
        print("\nИнформация о сигнале:")
        print(f"  Несущая частота: {args.carrier} Гц")
        print(f"  Частота модулирующего сигнала: {args.modulating_frequency} Гц")
        print(f"  Девиация частоты: {args.deviation} Гц")
        print(f"  Индекс модуляции: {modulation_index:.3f}")
        print(f"  Полоса частот по Карсону: {2 * (args.deviation + args.modulating_frequency):.1f} Гц")
        print(f"  Частота дискретизации: {args.sample_rate} Гц")
        print(f"  Длительность: {args.duration} с")
        print(f"  Амплитуда: {args.amplitude}")
        print(f"  Начальная фаза несущей: {args.carrier_phase} рад")
        print(f"  Начальная фаза модулирующего: {args.modulating_phase} рад")
        print(f"  Количество отсчетов: {len(signal)}")
        print(f"  Длительность сигнала: {len(signal)/args.sample_rate:.6f} с")
        print(f"  Минимальное значение: {np.min(signal):.6f}")
        print(f"  Максимальное значение: {np.max(signal):.6f}")
        print(f"  Среднее значение: {np.mean(signal):.6f}")
        print(f"  СКО: {np.std(signal):.6f}")
        
        # Проверка соответствия теоретическим значениям
        theoretical_rms = args.amplitude / np.sqrt(2)
        print(f"  Теоретическое СКО (для синусоиды): {theoretical_rms:.6f}")

if __name__ == "__main__":
    main()