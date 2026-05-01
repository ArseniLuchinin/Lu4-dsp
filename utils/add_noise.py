#!/usr/bin/env python3
"""Добавление AWGN шума в бинарный сигнал."""

import argparse
import sys

import numpy as np


def add_awgn(signal: np.ndarray, snr_db: float) -> np.ndarray:
    """Добавляет AWGN с заданным SNR (дБ).

    Для вещественного сигнала:
        P_signal = mean(x^2)
        P_noise  = P_signal / 10^(SNR/10)
        noise ~ N(0, sqrt(P_noise))

    Для комплексного сигнала:
        P_signal = mean(|x|^2)
        P_noise  = P_signal / 10^(SNR/10)
        noise ~ CN(0, P_noise)  (Re и Im по N(0, sqrt(P_noise/2)))
    """
    signal = signal.astype(np.float64) if np.isrealobj(signal) else signal.astype(np.complex128)

    if np.isrealobj(signal):
        signal_power = np.mean(signal ** 2)
        snr_linear = 10 ** (snr_db / 10.0)
        noise_power = signal_power / snr_linear
        noise_std = np.sqrt(noise_power)
        noise = np.random.normal(0, noise_std, size=signal.shape)
    else:
        signal_power = np.mean(np.abs(signal) ** 2)
        snr_linear = 10 ** (snr_db / 10.0)
        noise_power = signal_power / snr_linear
        noise_std = np.sqrt(noise_power / 2.0)
        noise = (
            np.random.normal(0, noise_std, size=signal.shape)
            + 1j * np.random.normal(0, noise_std, size=signal.shape)
        )

    return (signal + noise).astype(signal.dtype)


def main():
    parser = argparse.ArgumentParser(
        description="Добавляет AWGN шум в бинарный файл сигнала."
    )
    parser.add_argument("-i", "--input", help="Входной бинарный файл")
    parser.add_argument("--snr", type=float, required=True, help="SNR в дБ")
    parser.add_argument("-o", "--output", required=True, help="Выходной бинарный файл")
    parser.add_argument(
        "--type",
        choices=["float32", "complex64"],
        default="complex64",
        help="Тип данных во входном файле (по умолчанию complex64)",
    )
    parser.add_argument("--seed", type=int, default=None, help="Seed для генератора шума")
    args = parser.parse_args()

    if args.seed is not None:
        np.random.seed(args.seed)

    dtype = np.float32 if args.type == "float32" else np.complex64

    signal = np.fromfile(args.input, dtype=dtype)
    if signal.size == 0:
        print(f"Ошибка: файл '{args.input}' пуст или не найден.", file=sys.stderr)
        sys.exit(1)

    noisy_signal = add_awgn(signal, args.snr)
    noisy_signal.astype(dtype).tofile(args.output)

    print(f"Готово: {args.output}")
    print(f"  Отсчётов: {signal.size}")
    print(f"  Тип: {args.type}")
    print(f"  SNR: {args.snr} дБ")


if __name__ == "__main__":
    main()
