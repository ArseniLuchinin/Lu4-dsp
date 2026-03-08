#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import argparse
import os
import sys

def generate_modulating_signal(sample_rate, duration, mod_type='random', 
                              mod_freq=None, amplitude=1.0, cutoff_freq=None):
    """
    Генерирует модулирующий сигнал заданного типа
    
    Параметры:
    sample_rate - частота дискретизации в Гц
    duration - длительность сигнала в секундах
    mod_type - тип сигнала: 'random' (случайный) или 'cosine' (косинус)
    mod_freq - частота модулирующего сигнала (для cosine)
    amplitude - амплитуда модулирующего сигнала
    cutoff_freq - частота среза фильтра (для random, ограничение полосы)
    
    Возвращает:
    numpy array с модулирующим сигналом в формате float32 (нормализованный к [-1, 1])
    """
    num_samples = int(sample_rate * duration)
    
    if mod_type == 'cosine':
        if mod_freq is None:
            mod_freq = 100  # значение по умолчанию
        if mod_freq > sample_rate / 2:
            print(f"Предупреждение: Частота модуляции {mod_freq} Гц превышает частоту Найквиста")
            mod_freq = sample_rate / 4
            print(f"Частота модуляции уменьшена до {mod_freq} Гц")
        
        t = np.arange(num_samples) / sample_rate
        modulating_signal = amplitude * np.cos(2 * np.pi * mod_freq * t)
        
    else:  # random
        if cutoff_freq is None or cutoff_freq >= sample_rate/2:
            # Белый шум
            modulating_signal = np.random.normal(0, 1, num_samples)
        else:
            # Ограниченный по полосе шум
            white_noise = np.random.normal(0, 1, num_samples)
            
            # Создаем ФНЧ в частотной области
            fft_noise = np.fft.rfft(white_noise)
            freqs = np.fft.rfftfreq(num_samples, 1/sample_rate)
            
            # Применяем фильтр (идеальный ФНЧ)
            fft_noise[freqs > cutoff_freq] = 0
            
            # Обратное преобразование
            modulating_signal = np.fft.irfft(fft_noise, num_samples)
        
        # Нормализуем случайный сигнал к диапазону [-amplitude, amplitude]
        if np.max(np.abs(modulating_signal)) > 0:
            modulating_signal = amplitude * modulating_signal / np.max(np.abs(modulating_signal))
        else:
            modulating_signal = np.zeros(num_samples)
    
    return modulating_signal.astype(np.float32)

def generate_am_signal(carrier_freq, sample_rate, duration, 
                      modulation_index=0.5, carrier_amplitude=1.0,
                      mod_type='random', mod_freq=None,
                      modulating_cutoff=None, modulating_amplitude=1.0,
                      noise_level=0.0):
    """
    Генерирует AM сигнал с заданным типом модуляции
    
    Параметры:
    carrier_freq - частота несущей в Гц
    sample_rate - частота дискретизации в Гц
    duration - длительность сигнала в секундах
    modulation_index - индекс модуляции (0-1)
    carrier_amplitude - амплитуда несущей
    mod_type - тип модулирующего сигнала: 'random' или 'cosine'
    mod_freq - частота модулирующего сигнала (для cosine)
    modulating_cutoff - частота среза модулирующего сигнала (для random)
    modulating_amplitude - амплитуда модулирующего сигнала
    noise_level - уровень шума в сигнале (0-1)
    
    Возвращает:
    tuple (am_signal, modulating_signal, carrier_signal)
    """
    num_samples = int(sample_rate * duration)
    
    # Временные метки
    t = np.arange(num_samples) / sample_rate
    
    # Генерация модулирующего сигнала
    modulating_signal = generate_modulating_signal(
        sample_rate, duration,
        mod_type=mod_type,
        mod_freq=mod_freq,
        amplitude=modulating_amplitude,
        cutoff_freq=modulating_cutoff
    )
    
    # Нормализуем модулирующий сигнал к диапазону [-1, 1]
    # (если это еще не сделано в функции генерации)
    if np.max(np.abs(modulating_signal)) > 0:
        modulating_signal_norm = modulating_signal / np.max(np.abs(modulating_signal))
    else:
        modulating_signal_norm = modulating_signal
    
    # Для AM глубина модуляции должна быть такой, чтобы 
    # (1 + m * modulating_signal) всегда было положительным
    # Ограничиваем индекс модуляции, если нужно
    if modulation_index > 1.0:
        print(f"Предупреждение: Индекс модуляции {modulation_index} > 1.0 может вызвать перемодуляцию")
    
    # Генерация несущей
    carrier_signal = carrier_amplitude * np.sin(2 * np.pi * carrier_freq * t)
    
    # AM модуляция: s(t) = A_c * (1 + m * m(t)) * cos(2πf_c t)
    # где m(t) - нормализованный модулирующий сигнал
    am_signal = carrier_amplitude * (1 + modulation_index * modulating_signal_norm) * np.sin(2 * np.pi * carrier_freq * t)
    
    # Добавление шума, если нужно
    if noise_level > 0:
        noise = np.random.normal(0, noise_level * carrier_amplitude, num_samples)
        am_signal += noise
        print(f"Добавлен шум с уровнем {noise_level}")
    
    return am_signal.astype(np.float32), modulating_signal.astype(np.float32), carrier_signal.astype(np.float32)

def main():
    # Настройка парсера аргументов командной строки
    parser = argparse.ArgumentParser(
        description='Генерация AM сигнала со случайным или косинусоидальным модулирующим сигналом'
    )
    
    parser.add_argument('-fc', '--carrier-freq', type=float, required=True,
                        help='Частота несущей в Гц')
    
    parser.add_argument('-r', '--rate', '--sample-rate', type=float, required=True,
                        dest='sample_rate',
                        help='Частота дискретизации в Гц')
    
    parser.add_argument('-d', '--duration', type=float, default=1.0,
                        help='Длительность сигнала в секундах (по умолчанию: 1.0)')
    
    parser.add_argument('-m', '--mod-index', type=float, default=0.5,
                        dest='modulation_index',
                        help='Индекс модуляции (0-1, по умолчанию: 0.5)')
    
    parser.add_argument('-Ac', '--carrier-amp', type=float, default=1.0,
                        dest='carrier_amplitude',
                        help='Амплитуда несущей (по умолчанию: 1.0)')
    
    # Параметры для выбора типа модуляции
    parser.add_argument('--mod-type', type=str, choices=['random', 'cosine'], 
                        default='random',
                        help='Тип модулирующего сигнала: random (случайный) или cosine (косинус) (по умолчанию: random)')
    
    parser.add_argument('-fm', '--mod-freq', type=float, default=None,
                        dest='mod_freq',
                        help='Частота модулирующего сигнала в Гц (для --mod-type=cosine)')
    
    parser.add_argument('-fc-mod', '--mod-cutoff', type=float, default=None,
                        dest='modulating_cutoff',
                        help='Частота среза модулирующего сигнала в Гц (для --mod-type=random, по умолчанию: белый шум)')
    
    parser.add_argument('-Am', '--mod-amp', type=float, default=1.0,
                        dest='modulating_amplitude',
                        help='Амплитуда модулирующего сигнала (по умолчанию: 1.0)')
    
    parser.add_argument('-n', '--noise', type=float, default=0.0,
                        help='Уровень шума (0-1, по умолчанию: 0.0)')
    
    parser.add_argument('-o', '--output', type=str, default='am_signal.bin',
                        help='Имя выходного файла для AM сигнала (по умолчанию: am_signal.bin)')
    
    parser.add_argument('--output-mod', type=str, default='modulating_signal.bin',
                        help='Имя выходного файла для модулирующего сигнала (по умолчанию: modulating_signal.bin)')
    
    parser.add_argument('--output-carrier', type=str, default='carrier_signal.bin',
                        help='Имя выходного файла для несущей (по умолчанию: carrier_signal.bin)')
    
    parser.add_argument('--seed', type=int, default=None,
                        help='Зерно для генератора случайных чисел (для воспроизводимости)')
    
    parser.add_argument('--info', action='store_true',
                        help='Показать информацию о сгенерированных сигналах')
    
    parser.add_argument('--save-all', action='store_true',
                        help='Сохранить все компоненты (AM, модулирующий, несущую)')
    
    # Парсинг аргументов
    args = parser.parse_args()
    
    # Проверка аргументов
    if args.carrier_freq <= 0:
        print("Ошибка: Частота несущей должна быть положительной")
        sys.exit(1)
    
    if args.sample_rate <= 0:
        print("Ошибка: Частота дискретизации должна быть положительной")
        sys.exit(1)
    
    if args.carrier_freq > args.sample_rate / 2:
        print("Предупреждение: Частота несущей превышает частоту Найквиста")
        print(f"Максимальная допустимая частота: {args.sample_rate/2} Гц")
    
    if args.modulation_index < 0 or args.modulation_index > 2:
        print("Предупреждение: Индекс модуляции вне рекомендуемого диапазона 0-1")
    
    # Проверки для разных типов модуляции
    if args.mod_type == 'cosine':
        if args.mod_freq is None:
            args.mod_freq = 100
            print(f"Частота модулирующего сигнала не указана, используется значение по умолчанию: {args.mod_freq} Гц")
        if args.mod_freq <= 0:
            print("Ошибка: Частота модулирующего сигнала должна быть положительной")
            sys.exit(1)
        if args.mod_freq > args.sample_rate / 2:
            print(f"Предупреждение: Частота модуляции {args.mod_freq} Гц превышает частоту Найквиста")
    else:  # random
        if args.modulating_cutoff is not None:
            if args.modulating_cutoff <= 0:
                print("Ошибка: Частота среза модулирующего сигнала должна быть положительной")
                sys.exit(1)
            if args.modulating_cutoff > args.sample_rate / 2:
                print("Предупреждение: Частота среза превышает частоту Найквиста")
                args.modulating_cutoff = args.sample_rate / 2
    
    if args.noise < 0 or args.noise > 1:
        print("Ошибка: Уровень шума должен быть в диапазоне [0, 1]")
        sys.exit(1)
    
    # Установка зерна для воспроизводимости
    if args.seed is not None:
        np.random.seed(args.seed)
        print(f"Установлено зерно ГСЧ: {args.seed}")
    
    # Информация о типе модуляции
    mod_type_desc = "косинус" if args.mod_type == 'cosine' else "случайный"
    print(f"Генерация AM сигнала с несущей {args.carrier_freq} Гц")
    print(f"Тип модулирующего сигнала: {mod_type_desc}")
    
    # Генерация сигналов
    am_signal, modulating_signal, carrier_signal = generate_am_signal(
        args.carrier_freq,
        args.sample_rate,
        args.duration,
        modulation_index=args.modulation_index,
        carrier_amplitude=args.carrier_amplitude,
        mod_type=args.mod_type,
        mod_freq=args.mod_freq,
        modulating_cutoff=args.modulating_cutoff,
        modulating_amplitude=args.modulating_amplitude,
        noise_level=args.noise
    )
    
    # Сохранение AM сигнала
    try:
        am_signal.tofile(args.output)
        print(f"AM сигнал записан в файл: {args.output}")
        print(f"Размер файла: {os.path.getsize(args.output)} байт")
        print(f"Количество отсчетов: {len(am_signal)}")
    except IOError as e:
        print(f"Ошибка при записи AM сигнала: {e}")
        sys.exit(1)
    
    # Сохранение дополнительных сигналов, если запрошено
    if args.save_all:
        try:
            modulating_signal.tofile(args.output_mod)
            print(f"Модулирующий сигнал записан в файл: {args.output_mod}")
            
            carrier_signal.tofile(args.output_carrier)
            print(f"Несущая записана в файл: {args.output_carrier}")
        except IOError as e:
            print(f"Ошибка при записи дополнительных файлов: {e}")
    
    # Дополнительная информация, если запрошена
    if args.info:
        print("\n" + "="*50)
        print("ИНФОРМАЦИЯ О СИГНАЛАХ")
        print("="*50)
        
        print("\n--- Параметры генерации ---")
        print(f"Частота несущей: {args.carrier_freq} Гц")
        print(f"Частота дискретизации: {args.sample_rate} Гц")
        print(f"Длительность: {args.duration} с")
        print(f"Индекс модуляции: {args.modulation_index}")
        print(f"Амплитуда несущей: {args.carrier_amplitude}")
        
        if args.mod_type == 'cosine':
            print(f"Тип модуляции: Косинус частотой {args.mod_freq} Гц")
        else:
            if args.modulating_cutoff is None:
                print("Тип модуляции: Белый шум")
            else:
                print(f"Тип модуляции: Шум с ЧС {args.modulating_cutoff} Гц")
        
        print(f"Уровень шума: {args.noise}")
        
        print("\n--- AM сигнал ---")
        print(f"Минимальное значение: {np.min(am_signal):.6f}")
        print(f"Максимальное значение: {np.max(am_signal):.6f}")
        print(f"Среднее значение: {np.mean(am_signal):.6f}")
        print(f"СКО: {np.std(am_signal):.6f}")
        print(f"Энергия сигнала: {np.sum(am_signal**2):.6f}")
        
        # Огибающая (для проверки)
        envelope = np.abs(am_signal)
        print(f"Среднее значение огибающей: {np.mean(envelope):.6f}")
        
        print("\n--- Модулирующий сигнал ---")
        print(f"Минимальное значение: {np.min(modulating_signal):.6f}")
        print(f"Максимальное значение: {np.max(modulating_signal):.6f}")
        print(f"Среднее значение: {np.mean(modulating_signal):.6f}")
        print(f"СКО: {np.std(modulating_signal):.6f}")
        
        # Для случайного сигнала - оценка спектра
        if args.mod_type == 'random' and args.modulating_cutoff is not None:
            fft_mod = np.fft.rfft(modulating_signal)
            freqs = np.fft.rfftfreq(len(modulating_signal), 1/args.sample_rate)
            # Оценка энергии в полосе
            energy_in_band = np.sum(np.abs(fft_mod[freqs <= args.modulating_cutoff])**2)
            total_energy = np.sum(np.abs(fft_mod)**2)
            if total_energy > 0:
                print(f"Энергия в полосе до {args.modulating_cutoff} Гц: {100*energy_in_band/total_energy:.1f}%")
        
        print("\n--- Несущий сигнал ---")
        print(f"Минимальное значение: {np.min(carrier_signal):.6f}")
        print(f"Максимальное значение: {np.max(carrier_signal):.6f}")
        print(f"Среднее значение: {np.mean(carrier_signal):.6f}")
        print(f"СКО: {np.std(carrier_signal):.6f}")
        
        # Теоретическое СКО для синуса
        theoretical_rms = args.carrier_amplitude / np.sqrt(2)
        print(f"Теоретическое СКО несущей: {theoretical_rms:.6f}")
        
        # Проверка индекса модуляции
        modulating_norm = modulating_signal / np.max(np.abs(modulating_signal)) if np.max(np.abs(modulating_signal)) > 0 else modulating_signal
        envelope_min = np.min(1 + args.modulation_index * modulating_norm)
        envelope_max = np.max(1 + args.modulation_index * modulating_norm)
        print(f"\n--- Проверка модуляции ---")
        print(f"Минимальное значение огибающей (норм.): {envelope_min:.6f}")
        print(f"Максимальное значение огибающей (норм.): {envelope_max:.6f}")
        if envelope_min < 0:
            print("⚠️  ВНИМАНИЕ: Обнаружена перемодуляция (envelope < 0)")
        else:
            print("✓ Перемодуляции нет")

if __name__ == "__main__":
    main()