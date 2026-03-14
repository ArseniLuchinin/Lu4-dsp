#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import argparse
import os
import sys

def generate_complex_modulating_signal(sample_rate, duration, mod_type='random',
                                      mod_freq=None, amplitude=1.0, cutoff_freq=None):
    """
    Генерирует комплексный модулирующий сигнал заданного типа
    
    Параметры:
    sample_rate - частота дискретизации в Гц
    duration - длительность сигнала в секундах
    mod_type - тип сигнала: 'random' (случайный) или 'cosine' (косинус)
    mod_freq - частота модулирующего сигнала (для cosine)
    amplitude - амплитуда модулирующего сигнала
    cutoff_freq - частота среза фильтра (для random, ограничение полосы)
    
    Возвращает:
    numpy array с комплексным модулирующим сигналом в формате complex64 (нормализованный к [-1, 1])
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
        # Комплексный экспоненциальный сигнал (одна боковая полоса)
        modulating_signal = amplitude * np.exp(1j * 2 * np.pi * mod_freq * t)
        
    else:  # random
        if cutoff_freq is None or cutoff_freq >= sample_rate/2:
            # Комплексный белый шум
            modulating_signal = (np.random.normal(0, 1, num_samples) + 
                               1j * np.random.normal(0, 1, num_samples)) / np.sqrt(2)
        else:
            # Комплексный ограниченный по полосе шум
            white_noise = (np.random.normal(0, 1, num_samples) + 
                          1j * np.random.normal(0, 1, num_samples)) / np.sqrt(2)
            
            # Создаем ФНЧ в частотной области
            fft_noise = np.fft.fft(white_noise)
            freqs = np.fft.fftfreq(num_samples, 1/sample_rate)
            
            # Применяем фильтр (идеальный ФНЧ)
            fft_noise[np.abs(freqs) > cutoff_freq] = 0
            
            # Обратное преобразование
            modulating_signal = np.fft.ifft(fft_noise)
        
        # Нормализуем случайный сигнал к диапазону [-amplitude, amplitude]
        max_val = np.max(np.abs(modulating_signal))
        if max_val > 0:
            modulating_signal = amplitude * modulating_signal / max_val
        else:
            modulating_signal = np.zeros(num_samples, dtype=complex)
    
    return modulating_signal.astype(np.complex64)

def generate_complex_am_signal(carrier_freq, sample_rate, duration,
                              modulation_index=0.5, carrier_amplitude=1.0,
                              mod_type='random', mod_freq=None,
                              modulating_cutoff=None, modulating_amplitude=1.0,
                              noise_level=0.0, use_complex_carrier=True):
    """
    Генерирует комплексный AM сигнал с заданным типом модуляции
    
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
    use_complex_carrier - использовать комплексную несущую (exp) или вещественную (cos)
    
    Возвращает:
    tuple (am_signal, modulating_signal, carrier_signal)
    """
    num_samples = int(sample_rate * duration)
    
    # Временные метки
    t = np.arange(num_samples) / sample_rate
    
    # Генерация комплексного модулирующего сигнала
    modulating_signal = generate_complex_modulating_signal(
        sample_rate, duration,
        mod_type=mod_type,
        mod_freq=mod_freq,
        amplitude=modulating_amplitude,
        cutoff_freq=modulating_cutoff
    )
    
    # Нормализуем модулирующий сигнал к диапазону [-1, 1]
    max_val = np.max(np.abs(modulating_signal))
    if max_val > 0:
        modulating_signal_norm = modulating_signal / max_val
    else:
        modulating_signal_norm = modulating_signal
    
    # Для AM глубина модуляции должна быть такой, чтобы 
    # (1 + m * |modulating_signal|) всегда было положительным
    if modulation_index > 1.0:
        print(f"Предупреждение: Индекс модуляции {modulation_index} > 1.0 может вызвать перемодуляцию")
    
    # Генерация несущей (комплексной или вещественной)
    if use_complex_carrier:
        # Комплексная экспонента
        carrier_signal = carrier_amplitude * np.exp(1j * 2 * np.pi * carrier_freq * t)
    else:
        # Вещественная несущая (для совместимости с обычным AM)
        carrier_signal = carrier_amplitude * np.sin(2 * np.pi * carrier_freq * t)
        carrier_signal = carrier_signal.astype(np.complex64)
    
    # Комплексная AM модуляция: 
    # Для комплексной огибающей: s(t) = A_c * (1 + m * m(t)) * exp(j*2π*f_c*t)
    # где m(t) - комплексный модулирующий сигнал
    am_signal = carrier_amplitude * (1 + modulation_index * modulating_signal_norm) * np.exp(1j * 2 * np.pi * carrier_freq * t)
    
    # Добавление комплексного шума, если нужно
    if noise_level > 0:
        noise = (np.random.normal(0, noise_level * carrier_amplitude / np.sqrt(2), num_samples) + 
                1j * np.random.normal(0, noise_level * carrier_amplitude / np.sqrt(2), num_samples))
        am_signal += noise
        print(f"Добавлен комплексный шум с уровнем {noise_level}")
    
    return am_signal.astype(np.complex64), modulating_signal.astype(np.complex64), carrier_signal.astype(np.complex64)

def save_complex_signal(filename, signal):
    """
    Сохраняет комплексный сигнал в бинарный файл
    Формат: чередующиеся действительная и мнимая части (float32)
    """
    # Преобразуем в чередующиеся float32
    interleaved = np.zeros(2 * len(signal), dtype=np.float32)
    interleaved[0::2] = np.real(signal)
    interleaved[1::2] = np.imag(signal)
    interleaved.tofile(filename)

def load_complex_signal(filename, dtype=np.complex64):
    """Загружает комплексный сигнал из бинарного файла"""
    data = np.fromfile(filename, dtype=np.float32)
    return (data[0::2] + 1j * data[1::2]).astype(dtype)

def main():
    # Настройка парсера аргументов командной строки
    parser = argparse.ArgumentParser(
        description='Генерация комплексного AM сигнала со случайным или косинусоидальным модулирующим сигналом'
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
    
    parser.add_argument('-o', '--output', type=str, default='complex_am_signal.bin',
                        help='Имя выходного файла для комплексного AM сигнала (по умолчанию: complex_am_signal.bin)')
    
    parser.add_argument('--output-mod', type=str, default='complex_modulating_signal.bin',
                        help='Имя выходного файла для комплексного модулирующего сигнала (по умолчанию: complex_modulating_signal.bin)')
    
    parser.add_argument('--output-carrier', type=str, default='complex_carrier_signal.bin',
                        help='Имя выходного файла для комплексной несущей (по умолчанию: complex_carrier_signal.bin)')
    
    parser.add_argument('--seed', type=int, default=None,
                        help='Зерно для генератора случайных чисел (для воспроизводимости)')
    
    parser.add_argument('--info', action='store_true',
                        help='Показать информацию о сгенерированных сигналах')
    
    parser.add_argument('--save-all', action='store_true',
                        help='Сохранить все компоненты (AM, модулирующий, несущую)')
    
    parser.add_argument('--real-carrier', action='store_true',
                        help='Использовать вещественную несущую (sin) вместо комплексной экспоненты')
    
    parser.add_argument('--text-output', action='store_true',
                        help='Сохранить сигналы также в текстовом формате (для отладки)')
    
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
    mod_type_desc = "косинус (комплексный)" if args.mod_type == 'cosine' else "случайный комплексный"
    carrier_type = "комплексная экспонента" if not args.real_carrier else "вещественная (sin)"
    print(f"Генерация комплексного AM сигнала с несущей {args.carrier_freq} Гц")
    print(f"Тип несущей: {carrier_type}")
    print(f"Тип модулирующего сигнала: {mod_type_desc}")
    
    # Генерация сигналов
    am_signal, modulating_signal, carrier_signal = generate_complex_am_signal(
        args.carrier_freq,
        args.sample_rate,
        args.duration,
        modulation_index=args.modulation_index,
        carrier_amplitude=args.carrier_amplitude,
        mod_type=args.mod_type,
        mod_freq=args.mod_freq,
        modulating_cutoff=args.modulating_cutoff,
        modulating_amplitude=args.modulating_amplitude,
        noise_level=args.noise,
        use_complex_carrier=not args.real_carrier
    )
    
    # Сохранение AM сигнала
    try:
        save_complex_signal(args.output, am_signal)
        print(f"Комплексный AM сигнал записан в файл: {args.output}")
        print(f"Размер файла: {os.path.getsize(args.output)} байт")
        print(f"Количество комплексных отсчетов: {len(am_signal)}")
        
        if args.text_output:
            txt_output = args.output.replace('.bin', '.txt')
            with open(txt_output, 'w') as f:
                for i, val in enumerate(am_signal[:100]):  # первые 100 отсчетов
                    f.write(f"{i}: {np.real(val):.6f} + {np.imag(val):.6f}j\n")
            print(f"Текстовый превью сохранено в: {txt_output}")
            
    except IOError as e:
        print(f"Ошибка при записи AM сигнала: {e}")
        sys.exit(1)
    
    # Сохранение дополнительных сигналов, если запрошено
    if args.save_all:
        try:
            save_complex_signal(args.output_mod, modulating_signal)
            print(f"Комплексный модулирующий сигнал записан в файл: {args.output_mod}")
            
            save_complex_signal(args.output_carrier, carrier_signal)
            print(f"Комплексная несущая записана в файл: {args.output_carrier}")
        except IOError as e:
            print(f"Ошибка при записи дополнительных файлов: {e}")
    
    # Дополнительная информация, если запрошена
    if args.info:
        print("\n" + "="*50)
        print("ИНФОРМАЦИЯ О КОМПЛЕКСНЫХ СИГНАЛАХ")
        print("="*50)
        
        print("\n--- Параметры генерации ---")
        print(f"Частота несущей: {args.carrier_freq} Гц")
        print(f"Частота дискретизации: {args.sample_rate} Гц")
        print(f"Длительность: {args.duration} с")
        print(f"Индекс модуляции: {args.modulation_index}")
        print(f"Амплитуда несущей: {args.carrier_amplitude}")
        
        if args.mod_type == 'cosine':
            print(f"Тип модуляции: Комплексный косинус частотой {args.mod_freq} Гц")
        else:
            if args.modulating_cutoff is None:
                print("Тип модуляции: Комплексный белый шум")
            else:
                print(f"Тип модуляции: Комплексный шум с ЧС {args.modulating_cutoff} Гц")
        
        print(f"Уровень шума: {args.noise}")
        
        print("\n--- Комплексный AM сигнал ---")
        print(f"Мин. действительная часть: {np.min(np.real(am_signal)):.6f}")
        print(f"Макс. действительная часть: {np.max(np.real(am_signal)):.6f}")
        print(f"Мин. мнимая часть: {np.min(np.imag(am_signal)):.6f}")
        print(f"Макс. мнимая часть: {np.max(np.imag(am_signal)):.6f}")
        print(f"Модуль (огибающая): мин={np.min(np.abs(am_signal)):.6f}, макс={np.max(np.abs(am_signal)):.6f}")
        print(f"Среднее значение: {np.mean(am_signal):.6f}")
        print(f"СКО: {np.std(am_signal):.6f}")
        print(f"Энергия сигнала: {np.sum(np.abs(am_signal)**2):.6f}")
        
        print("\n--- Комплексный модулирующий сигнал ---")
        print(f"Мин. действительная часть: {np.min(np.real(modulating_signal)):.6f}")
        print(f"Макс. действительная часть: {np.max(np.real(modulating_signal)):.6f}")
        print(f"Мин. мнимая часть: {np.min(np.imag(modulating_signal)):.6f}")
        print(f"Макс. мнимая часть: {np.max(np.imag(modulating_signal)):.6f}")
        print(f"Модуль: мин={np.min(np.abs(modulating_signal)):.6f}, макс={np.max(np.abs(modulating_signal)):.6f}")
        
        # Для случайного сигнала - оценка спектра
        if args.mod_type == 'random' and args.modulating_cutoff is not None:
            fft_mod = np.fft.fft(modulating_signal)
            freqs = np.fft.fftfreq(len(modulating_signal), 1/args.sample_rate)
            # Оценка энергии в полосе
            energy_in_band = np.sum(np.abs(fft_mod[np.abs(freqs) <= args.modulating_cutoff])**2)
            total_energy = np.sum(np.abs(fft_mod)**2)
            if total_energy > 0:
                print(f"Энергия в полосе до {args.modulating_cutoff} Гц: {100*energy_in_band/total_energy:.1f}%")
        
        print("\n--- Несущий сигнал ---")
        print(f"Мин. действительная часть: {np.min(np.real(carrier_signal)):.6f}")
        print(f"Макс. действительная часть: {np.max(np.real(carrier_signal)):.6f}")
        print(f"Мин. мнимая часть: {np.min(np.imag(carrier_signal)):.6f}")
        print(f"Макс. мнимая часть: {np.max(np.imag(carrier_signal)):.6f}")
        print(f"Модуль: мин={np.min(np.abs(carrier_signal)):.6f}, макс={np.max(np.abs(carrier_signal)):.6f}")
        
        if not args.real_carrier:
            theoretical_rms = args.carrier_amplitude / np.sqrt(2)
            print(f"Теоретическое СКО несущей: {theoretical_rms:.6f}")
        
        # Проверка индекса модуляции
        modulating_norm = modulating_signal / np.max(np.abs(modulating_signal)) if np.max(np.abs(modulating_signal)) > 0 else modulating_signal
        envelope_min = np.min(np.abs(1 + args.modulation_index * modulating_norm))
        envelope_max = np.max(np.abs(1 + args.modulation_index * modulating_norm))
        print(f"\n--- Проверка модуляции ---")
        print(f"Минимальное значение огибающей (норм.): {envelope_min:.6f}")
        print(f"Максимальное значение огибающей (норм.): {envelope_max:.6f}")
        if envelope_min < 0:
            print("⚠️  ВНИМАНИЕ: Обнаружена перемодуляция (envelope < 0)")
        else:
            print("✓ Перемодуляции нет")
        
        print("\n--- Функции для загрузки ---")
        print("# Пример загрузки комплексного сигнала в Python:")
        print("def load_complex_signal(filename):")
        print("    data = np.fromfile(filename, dtype=np.float32)")
        print("    return (data[0::2] + 1j * data[1::2]).astype(np.complex64)")
        print("\nam_signal = load_complex_signal('{}')".format(args.output))

if __name__ == "__main__":
    main()