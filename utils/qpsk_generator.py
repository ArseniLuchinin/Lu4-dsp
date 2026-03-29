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


# ---------- UPSAMPLE ----------
def upsample(symbols, sps):
    out = np.zeros(len(symbols) * sps, dtype=np.complex64)
    out[::sps] = symbols
    return out


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


# ---------- IQ SAVE ----------
def save_iq(f, signal):
    iq = np.empty(signal.size * 2, dtype=np.float32)
    iq[0::2] = signal.real
    iq[1::2] = signal.imag
    iq.tofile(f)


# ---------- MAIN ----------
def main():
    parser = argparse.ArgumentParser(description="Clean QPSK generator")

    parser.add_argument("--input", required=True, help="Text file (UTF-8)")
    parser.add_argument("--fs", type=float, required=True)

    parser.add_argument("--sps", type=int, default=4)
    parser.add_argument("--mode", choices=["ideal", "rrc"], default="ideal")

    parser.add_argument("--beta", type=float, default=0.35)
    parser.add_argument("--span", type=int, default=8)

    parser.add_argument("--chunk", type=int, default=1024*1024)
    parser.add_argument("--out", default="signal.bin")

    args = parser.parse_args()

    # фильтр (если нужен)
    if args.mode == "rrc":
        h = rrc(args.beta, args.sps, args.span)
        tail = np.zeros(len(h)-1, dtype=np.complex64)

    total = 0

    with open(args.input, "r", encoding="utf-8") as fin, open(args.out, "wb") as fout:

        while True:
            text = fin.read(args.chunk)
            if not text:
                break

            data = text.encode("utf-8")

            bits = bytes_to_bits(data)
            symbols = bits_to_qpsk(bits)
            up = upsample(symbols, args.sps)

            if args.mode == "ideal":
                y = up

            else:  # RRC
                x = np.concatenate([tail, up])
                y = np.convolve(x, h, mode="valid")
                tail = x[-(len(h)-1):]

            save_iq(fout, y)
            total += len(y)

        if args.mode == "rrc":
            # Flush filter state with trailing zeros so RX can recover tail symbols.
            x_flush = np.concatenate([tail, np.zeros(len(h)-1, dtype=np.complex64)])
            y_flush = np.convolve(x_flush, h, mode="valid")
            save_iq(fout, y_flush)
            total += len(y_flush)

    print("Done")
    print(f"Samples: {total}")
    print(f"Mode: {args.mode}")
    print(f"Output: {args.out}")


if __name__ == "__main__":
    main()
