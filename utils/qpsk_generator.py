import argparse
import numpy as np

def bits_from_bytes(byte_array):
    return np.unpackbits(np.frombuffer(byte_array, dtype=np.uint8))

def bits_to_qpsk(bits):
    if len(bits) % 2 != 0:
        bits = np.append(bits, 0)

    bits = bits.reshape(-1, 2)

    # Gray mapping
    mapping = {
        (0, 0): 1 + 1j,
        (0, 1): -1 + 1j,
        (1, 1): -1 - 1j,
        (1, 0): 1 - 1j,
    }

    symbols = np.array([mapping[tuple(b)] for b in bits], dtype=np.complex64)
    symbols /= np.sqrt(2)

    return symbols

def upsample(symbols, sps):
    up = np.zeros(len(symbols) * sps, dtype=np.complex64)
    up[::sps] = symbols
    return up

def rrc_filter(beta, sps, span):
    N = span * sps
    t = np.arange(-N/2, N/2 + 1) / sps

    h = np.zeros_like(t, dtype=np.float32)

    for i, ti in enumerate(t):
        if ti == 0.0:
            h[i] = 1.0 - beta + 4 * beta / np.pi
        elif abs(ti) == 1 / (4 * beta):
            h[i] = (beta / np.sqrt(2)) * (
                ((1 + 2/np.pi) * np.sin(np.pi/(4*beta))) +
                ((1 - 2/np.pi) * np.cos(np.pi/(4*beta)))
            )
        else:
            h[i] = (
                np.sin(np.pi * ti * (1 - beta)) +
                4 * beta * ti * np.cos(np.pi * ti * (1 + beta))
            ) / (
                np.pi * ti * (1 - (4 * beta * ti)**2)
            )

    h /= np.sqrt(np.sum(h**2))
    return h

def save_iq(f, signal):
    iq = np.empty(signal.size * 2, dtype=np.float32)
    iq[0::2] = signal.real
    iq[1::2] = signal.imag
    iq.tofile(f)

def main():
    parser = argparse.ArgumentParser(description="Streaming QPSK generator (text file)")

    parser.add_argument("--input", required=True, help="Input TEXT file (UTF-8)")
    parser.add_argument("--fs", type=float, required=True, help="Sample rate")

    parser.add_argument("--sps", type=int, default=4)
    parser.add_argument("--beta", type=float, default=0.35)
    parser.add_argument("--span", type=int, default=8)
    parser.add_argument("--chunk", type=int, default=1024*1024, help="Chars per chunk")
    parser.add_argument("--out", default="qpsk.bin")

    args = parser.parse_args()

    h = rrc_filter(args.beta, args.sps, args.span)
    fir_tail = np.zeros(len(h)-1, dtype=np.complex64)

    total_samples = 0

    with open(args.input, "r", encoding="utf-8") as fin, open(args.out, "wb") as fout:
        while True:
            text_chunk = fin.read(args.chunk)
            if not text_chunk:
                break

            # текст → байты
            data = text_chunk.encode("utf-8")

            bits = bits_from_bytes(data)
            symbols = bits_to_qpsk(bits)
            up = upsample(symbols, args.sps)

            # FIR с сохранением состояния
            x = np.concatenate([fir_tail, up])
            y = np.convolve(x, h, mode='valid')

            fir_tail = x[-(len(h)-1):]

            save_iq(fout, y)
            total_samples += len(y)

    print(f"Done. Total samples: {total_samples}")
    print(f"Output file: {args.out}")
    print(f"Sample rate: {args.fs}")

if __name__ == "__main__":
    main()