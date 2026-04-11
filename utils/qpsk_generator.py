import argparse
import numpy as np


# ---------- БИТЫ ----------
def bytes_to_bits(data):
    return np.unpackbits(np.frombuffer(data, dtype=np.uint8))


# ---------- QPSK ----------
def bits_to_qpsk(bits):
    # дополняем до чётного числа
    if len(bits) % 2 != 0:
        bits = np.append(bits, 0)

    # unpackbits -> uint8, so cast to signed/float before arithmetic.
    # Otherwise 1 - 2*1 underflows as uint8 and becomes 255.
    b0 = bits[0::2].astype(np.float32)
    b1 = bits[1::2].astype(np.float32)

    # Gray coding (векторно!)
    I = 1 - 2 * b0   # 0->+1, 1->-1
    Q = 1 - 2 * b1

    symbols = (I + 1j * Q).astype(np.complex64)

    # нормализация мощности → |s| = 1
    symbols /= np.sqrt(2)

    return symbols


# ---------- UPSAMPLE (потоковый, без полной аллокации) ----------
def upsample_streaming(symbols, sps, out_buffer):
    """Записывает upsample-символы напрямую в готовый буфер. Возвращает длину результата."""
    out_len = len(symbols) * sps
    out_buffer[:out_len].fill(0)
    out_buffer[:out_len:sps] = symbols
    return out_len


# ---------- RRC ----------
def rrc(beta, sps, span):
    N = span * sps
    t = np.arange(-N//2, N//2 + 1) / sps

    h = np.zeros_like(t, dtype=np.float32)

    for i, ti in enumerate(t):
        if ti == 0:
            h[i] = 1 - beta + 4 * beta / np.pi
        elif abs(ti) == 1 / (4 * beta):
            h[i] = (beta / np.sqrt(2)) * (
                (1 + 2/np.pi) * np.sin(np.pi/(4*beta)) +
                (1 - 2/np.pi) * np.cos(np.pi/(4*beta))
            )
        else:
            h[i] = (
                np.sin(np.pi * ti * (1 - beta)) +
                4 * beta * ti * np.cos(np.pi * ti * (1 + beta))
            ) / (
                np.pi * ti * (1 - (4 * beta * ti)**2)
            )

    # нормализация энергии
    h /= np.sqrt(np.sum(h**2))
    return h


# ---------- RRC ФИЛЬТРАЦИЯ (потоковая, блоками) ----------
def rrc_filter_streaming(upsampled, h, tail, out_buffer, block_size=65536):
    """
    Потоковая RRC фильтрация.
    Обрабатывает свёртку блоками по block_size отсчётов для экономии памяти.
    """
    h_len = len(h)
    up_len = len(upsampled)

    # формируем входной блок: tail + upsampled
    out_buffer[:h_len - 1] = tail
    out_buffer[h_len - 1:h_len - 1 + up_len] = upsampled

    # обновляем tail для следующего блока
    tail[:] = out_buffer[up_len:up_len + h_len - 1]

    # свёртка блоками
    out_len = up_len
    for start in range(0, out_len, block_size):
        end = min(start + block_size, out_len)
        window_end = end + h_len - 1
        block_input = out_buffer[start:window_end]
        out_buffer[start:end] = np.convolve(block_input, h, mode="valid")

    return out_len


# ---------- ДОБАВЛЕНИЕ ШУМА ----------
def add_awgn(signal, snr_db):
    """
    Добавляет AWGN шум с заданным SNR (дБ).
    SNR = 10 * log10(P_signal / P_noise)
    """
    # мощность сигнала
    signal_power = np.mean(np.abs(signal) ** 2)

    # линейное SNR
    snr_linear = 10 ** (snr_db / 10.0)

    # мощность шума
    noise_power = signal_power / snr_linear

    # стандартное отклонение шума (на компоненту)
    noise_std = np.sqrt(noise_power / 2.0)

    # генерация шума
    noise = (np.random.normal(0, noise_std, len(signal)) +
             1j * np.random.normal(0, noise_std, len(signal))).astype(np.complex64)

    return signal + noise


# ---------- ФАЗОВЫЙ СДВИГ ----------
def apply_phase_offset(signal, phase_deg):
    """Применяет фазовый сдвиг (в градусах)."""
    phase_rad = np.deg2rad(phase_deg)
    rotation = np.exp(1j * phase_rad).astype(np.complex64)
    return signal * rotation


# ---------- IQ SAVE (потоковый, без полного массива) ----------
def save_iq_chunk(f, signal):
    """Записывает IQ сигнал напрямую в файл без промежуточного массива."""
    # чередуем I и Q компоненты
    iq_interleaved = np.empty(signal.size * 2, dtype=np.float32)
    iq_interleaved[0::2] = signal.real
    iq_interleaved[1::2] = signal.imag
    iq_interleaved.tofile(f)
    del iq_interleaved


# ---------- MAIN ----------
def main():
    parser = argparse.ArgumentParser(
        description="QPSK generator with memory optimization and noise support"
    )

    parser.add_argument("--input", required=True, help="Text file (UTF-8)")
    parser.add_argument("--fs", type=float, required=True, help="Sample rate (Hz)")

    parser.add_argument("--sps", type=int, default=4, help="Samples per symbol")
    parser.add_argument("--mode", choices=["ideal", "rrc"], default="ideal",
                        help="Filtering mode")

    parser.add_argument("--beta", type=float, default=0.35, help="RRC rolloff factor")
    parser.add_argument("--span", type=int, default=8, help="RRC span in symbols")

    parser.add_argument("--chunk", type=int, default=1024,
                        help="Input text chunk size in bytes (default 1KB)")
    parser.add_argument("--out", default="signal.bin", help="Output file")

    # Новые параметры для реалистичности сигнала
    parser.add_argument("--snr", type=float, default=None,
                        help="Add AWGN noise with specified SNR (dB)")
    parser.add_argument("--phase-offset", type=float, default=0.0,
                        help="Carrier phase offset in degrees")
    parser.add_argument("--freq-offset", type=float, default=0.0,
                        help="Carrier frequency offset in Hz")

    args = parser.parse_args()

    # фильтр (если нужен)
    h = None
    tail = None
    if args.mode == "rrc":
        h = rrc(args.beta, args.sps, args.span)
        tail = np.zeros(len(h) - 1, dtype=np.complex64)

    # предварительный расчёт размеров буферов
    # 1 чанк текста → ~chunk * 8 бит → chunk * 8 / 2 символов → chunk * 8 / 2 * sps отсчётов
    max_symbols_per_chunk = (args.chunk * 8 + 1) // 2
    max_samples_per_chunk = max_symbols_per_chunk * args.sps

    # выделяем буферы один раз
    upsampled_buffer = np.zeros(max_samples_per_chunk, dtype=np.complex64)
    filter_size = max_samples_per_chunk + (len(h) if h is not None else 0)
    filter_buffer = np.zeros(filter_size, dtype=np.complex64)

    total = 0
    sample_index = 0  # глобальный индекс для частотного сдвига

    with open(args.input, "r", encoding="utf-8") as fin, open(args.out, "wb") as fout:

        while True:
            text = fin.read(args.chunk)
            if not text:
                break

            data = text.encode("utf-8")

            bits = bytes_to_bits(data)
            symbols = bits_to_qpsk(bits)

            # upsample в готовый буфер
            up_len = upsample_streaming(symbols, args.sps, upsampled_buffer)
            upsampled = upsampled_buffer[:up_len]

            if args.mode == "ideal":
                y = upsampled.copy()
            else:  # RRC
                out_len = rrc_filter_streaming(upsampled, h, tail, filter_buffer)
                y = filter_buffer[:out_len].copy()

            # фазовый сдвиг
            if args.phase_offset != 0.0:
                y = apply_phase_offset(y, args.phase_offset)

            # частотный сдвиг
            if args.freq_offset != 0.0:
                t = np.arange(len(y), dtype=np.float32) / args.fs
                freq_rotation = np.exp(1j * 2.0 * np.pi * args.freq_offset * t).astype(np.complex64)
                y = y * freq_rotation

            # добавление шума
            if args.snr is not None:
                y = add_awgn(y, args.snr)

            save_iq_chunk(fout, y)
            total += len(y)
            sample_index += len(y)

            # освобождаем память
            del bits, symbols, upsampled, y

        if args.mode == "rrc":
            # Flush filter state with trailing zeros
            flush_zeros = np.zeros(len(h) - 1, dtype=np.complex64)
            out_len = rrc_filter_streaming(flush_zeros, h, tail, filter_buffer)
            y_flush = filter_buffer[:out_len].copy()

            if args.phase_offset != 0.0:
                y_flush = apply_phase_offset(y_flush, args.phase_offset)

            if args.freq_offset != 0.0:
                t = np.arange(len(y_flush), dtype=np.float32) / args.fs
                freq_rotation = np.exp(1j * 2.0 * np.pi * args.freq_offset * t).astype(np.complex64)
                y_flush = y_flush * freq_rotation

            if args.snr is not None:
                y_flush = add_awgn(y_flush, args.snr)

            save_iq_chunk(fout, y_flush)
            total += len(y_flush)

    print("Done")
    print(f"Samples: {total}")
    print(f"Mode: {args.mode}")
    print(f"Output: {args.out}")
    if args.snr is not None:
        print(f"SNR: {args.snr} dB")
    if args.phase_offset != 0.0:
        print(f"Phase offset: {args.phase_offset}°")
    if args.freq_offset != 0.0:
        print(f"Freq offset: {args.freq_offset} Hz")


if __name__ == "__main__":
    main()
