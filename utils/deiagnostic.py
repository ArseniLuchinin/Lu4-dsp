import numpy as np
import argparse
import matplotlib.pyplot as plt
import os

def main():
    parser = argparse.ArgumentParser(description="IQ file diagnostics (limited samples)")
    parser.add_argument("--file", required=True, help="IQ file path (float32 interleaved I/Q)")
    parser.add_argument("--plot", action="store_true", help="Show constellation plot")
    parser.add_argument("--max_samples", type=int, default=100000, help="Max IQ samples to analyze")
    args = parser.parse_args()

    # Получаем размер файла
    file_size = os.path.getsize(args.file)
    max_bytes = args.max_samples * 2 * 4  # I + Q, float32 = 8 байт на сэмпл
    n_bytes = min(max_bytes, file_size)

    # Делаем количество байт кратным 8 (размер I+Q)
    n_bytes -= n_bytes % 8

    if n_bytes <= 0:
        print("File too small or max_samples=0")
        return

    # Чтение ограниченного количества байт
    with open(args.file, "rb") as f:
        iq_bytes = f.read(n_bytes)

    iq = np.frombuffer(iq_bytes, dtype=np.float32)

    if len(iq) < 2:
        print("File too short to contain IQ samples")
        return

    # Разделение на I и Q
    I = iq[0::2]
    Q = iq[1::2]
    x = I + 1j * Q

    print("===== Diagnostics (limited) =====")
    print(f"Analyzed IQ samples: {len(x)}")
    print(f"I: min={I.min():.4f}, max={I.max():.4f}, mean={I.mean():.4f}")
    print(f"Q: min={Q.min():.4f}, max={Q.max():.4f}, mean={Q.mean():.4f}")
    print(f"Magnitude: min={np.abs(x).min():.4f}, max={np.abs(x).max():.4f}, mean={np.abs(x).mean():.4f}")

    unique_I = np.unique(np.round(I, 3))
    unique_Q = np.unique(np.round(Q, 3))
    print(f"Unique rounded I levels: {unique_I}")
    print(f"Unique rounded Q levels: {unique_Q}")

    if all(u in [-0.707, 0.707] for u in unique_I) and all(u in [-0.707, 0.707] for u in unique_Q):
        print("Looks like an ideal QPSK signal with ±0.707 levels")

    # Опциональный график констеляции
    if args.plot:
        plt.scatter(I, Q, s=1)
        plt.xlabel("I")
        plt.ylabel("Q")
        plt.title("IQ Constellation")
        plt.grid(True)
        plt.axis('equal')
        plt.show()


if __name__ == "__main__":
    main()