#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Signal Window Viewer — просмотр окна сигнала из бинарного файла.

Поддерживаемые форматы:
  - float32 (real signal)
  - complex64 (IQ signal, interleaved)

Использует mmap для потокового чтения — не загружает весь файл в память.

Управление:
  - ← / → — прокрутка влево/вправо на половину окна
  - Page Up / Page Down — прокрутка на полный размер окна
  - ↑ — приблизить (уменьшить окно в 2 раза)
  - ↓ — отдалить (увеличить окно в 2 раза)
  - Home — перейти к началу файла
  - End — перейти к концу файла
  - q / Esc — выход
"""

import argparse
import sys
import os
import mmap

import numpy as np
import matplotlib.pyplot as plt


class SignalViewer:
    """Интерактивный просмотрщик сигнала с mmap-чтением."""

    def __init__(self, filepath, sample_rate, window_size, is_complex):
        self.filepath = filepath
        self.sample_rate = sample_rate
        self.is_complex = is_complex

        # Параметры файла
        self.file_size = os.path.getsize(filepath)
        self.bytes_per_sample = 8 if is_complex else 4  # complex64=8, float32=4
        self.total_samples = self.file_size // self.bytes_per_sample

        # Параметры окна
        self.window_size = min(window_size, self.total_samples)
        self.offset = 0  # текущее смещение в отсчётах

        # mmap — файл отображается в память, но читаем только нужные чанки
        self._fd = open(filepath, 'rb')
        self._mm = mmap.mmap(self._fd.fileno(), 0, access=mmap.ACCESS_READ)

        # Настройка графика
        self.fig, self.axes = self._create_figure()
        self._update_plot()

        # Подключение обработчиков клавиш
        self.fig.canvas.mpl_connect('key_press_event', self._on_key)
        self.fig.canvas.mpl_connect('close_event', self._on_close)

        self._running = True

    def _read_window(self, start, count):
        """Читает окно данных из mmap файла."""
        byte_offset = start * self.bytes_per_sample
        byte_count = count * self.bytes_per_sample

        # Ограничиваем по размеру файла
        if byte_offset + byte_count > self.file_size:
            byte_count = self.file_size - byte_offset
            count = byte_count // self.bytes_per_sample

        if count <= 0:
            return np.array([], dtype=np.complex64 if self.is_complex else np.float32)

        # Читаем байты напрямую из mmap
        self._mm.seek(byte_offset)
        raw_bytes = self._mm.read(byte_count)

        if self.is_complex:
            return np.frombuffer(raw_bytes, dtype=np.complex64, count=count)
        else:
            return np.frombuffer(raw_bytes, dtype=np.float32, count=count)

    def _create_figure(self):
        """Создаёт фигуру с осями и информационной панелью."""
        fig = plt.figure(figsize=(12, 7))

        # Основной график
        ax_signal = fig.add_axes([0.1, 0.25, 0.85, 0.65])
        ax_signal.set_title("Signal Viewer")
        ax_signal.set_xlabel("Samples")
        ax_signal.set_ylabel("Amplitude")
        ax_signal.grid(True, alpha=0.3)

        # Информационная панель
        ax_info = fig.add_axes([0.1, 0.05, 0.85, 0.15])
        ax_info.axis('off')
        self.info_text = ax_info.text(0.0, 0.5, '', va='center', fontsize=10,
                                       fontfamily='monospace')

        return fig, ax_signal

    def _update_plot(self):
        """Обновляет график текущим окном данных."""
        self.axes.clear()

        # Читаем окно из файла (потоково, без загрузки всего файла)
        window = self._read_window(self.offset, self.window_size)
        actual_count = len(window)

        if actual_count == 0:
            return

        # Временная ось
        start = self.offset
        end = start + actual_count
        t = np.arange(actual_count) + start

        if self.is_complex:
            # IQ сигнал: рисуем I и Q компоненты
            self.axes.plot(t, window.real, 'b-', label='I (In-phase)', alpha=0.8)
            self.axes.plot(t, window.imag, 'r-', label='Q (Quadrature)', alpha=0.8)
            self.axes.plot(t, np.abs(window), 'g-', label='|s| (Envelope)', alpha=0.5)
            self.axes.legend(loc='upper right', fontsize=8)
        else:
            # Real сигнал
            self.axes.plot(t, window, 'b-', linewidth=0.8)

        self.axes.set_title(
            f"Signal Viewer — [{start}:{end}] / {self.total_samples} samples"
        )
        self.axes.set_xlabel("Sample index")
        self.axes.set_ylabel("Amplitude")
        self.axes.grid(True, alpha=0.3)

        # Обновляем информацию
        self._update_info(start, end, window)

        self.fig.canvas.draw()

    def _update_info(self, start, end, window):
        """Обновляет информационную панель."""
        # Метаданные файла
        size_kb = self.file_size / 1024
        size_mb = self.file_size / (1024 * 1024)
        if size_mb >= 1:
            size_str = f"{size_mb:.2f} MB"
        else:
            size_str = f"{size_kb:.2f} KB"
        total_duration = self.total_samples / self.sample_rate
        dtype_str = "complex64 (IQ)" if self.is_complex else "float32 (real)"

        if self.is_complex:
            power_db = 10 * np.log10(np.mean(np.abs(window) ** 2) + 1e-12)
            info = (
                f"File: {os.path.basename(self.filepath)}  |  "
                f"Size: {size_str}  |  "
                f"Format: {dtype_str}  |  "
                f"Fs: {self.sample_rate:.0f} Hz  |  "
                f"Samples: {self.total_samples}  |  "
                f"Duration: {total_duration:.4f} s\n"
                f"Window: {self.window_size} samples ({self.window_size / self.sample_rate * 1000:.2f} ms)  |  "
                f"Position: {start} — {end}\n"
                f"Min: {np.min(window.real):.4f}  |  "
                f"Max: {np.max(window.real):.4f}  |  "
                f"Mean: {np.mean(window.real):.4f}  |  "
                f"Power: {power_db:.2f} dB\n"
                f"←/→: scroll  |  PgUp/PgDn: fast scroll  |  "
                f"Up/Down: zoom  |  Home/End: jump  |  q: quit"
            )
        else:
            info = (
                f"File: {os.path.basename(self.filepath)}  |  "
                f"Size: {size_str}  |  "
                f"Format: {dtype_str}  |  "
                f"Fs: {self.sample_rate:.0f} Hz  |  "
                f"Samples: {self.total_samples}  |  "
                f"Duration: {total_duration:.4f} s\n"
                f"Window: {self.window_size} samples ({self.window_size / self.sample_rate * 1000:.2f} ms)  |  "
                f"Position: {start} — {end}\n"
                f"Min: {np.min(window):.4f}  |  "
                f"Max: {np.max(window):.4f}  |  "
                f"Mean: {np.mean(window):.4f}  |  "
                f"RMS: {np.sqrt(np.mean(window**2)):.4f}\n"
                f"←/→: scroll  |  PgUp/PgDn: fast scroll  |  "
                f"Up/Down: zoom  |  Home/End: jump  |  q: quit"
            )
        self.info_text.set_text(info)

    def _on_key(self, event):
        """Обработчик нажатий клавиш."""
        key = event.key
        half_window = self.window_size // 2

        if key == 'q' or key == 'escape':
            self._running = False
            plt.close(self.fig)
            return

        elif key == 'left':
            # Прокрутка влево на половину окна
            self.offset = max(0, self.offset - half_window)
            self._update_plot()

        elif key == 'right':
            # Прокрутка вправо на половину окна
            self.offset = min(self.total_samples - self.window_size,
                              self.offset + half_window)
            self._update_plot()

        elif key == 'pageup':
            # Быстрая прокрутка влево
            self.offset = max(0, self.offset - self.window_size)
            self._update_plot()

        elif key == 'pagedown':
            # Быстрая прокрутка вправо
            self.offset = min(self.total_samples - self.window_size,
                              self.offset + self.window_size)
            self._update_plot()

        elif key == 'up':
            # Приближение — уменьшить окно в 2 раза
            new_window = max(64, self.window_size // 2)
            if new_window != self.window_size:
                self.window_size = new_window
                self.offset = min(self.offset, self.total_samples - self.window_size)
                self._update_plot()

        elif key == 'down':
            # Отдаление — увеличить окно в 2 раза
            new_window = min(self.total_samples, self.window_size * 2)
            if new_window != self.window_size:
                self.window_size = new_window
                self.offset = min(self.offset, self.total_samples - self.window_size)
                self._update_plot()

        elif key == '+' or key == '=':
            # Увеличить масштаб (уменьшить окно в 2 раза)
            new_window = max(64, self.window_size // 2)
            if new_window != self.window_size:
                self.window_size = new_window
                # Корректируем offset, чтобы не выйти за границы
                self.offset = min(self.offset, self.total_samples - self.window_size)
                self._update_plot()

        elif key == '-' or key == '_':
            # Уменьшить масштаб (увеличить окно в 2 раза)
            new_window = min(self.total_samples, self.window_size * 2)
            if new_window != self.window_size:
                self.window_size = new_window
                # Корректируем offset, чтобы не выйти за границы
                self.offset = min(self.offset, self.total_samples - self.window_size)
                self._update_plot()

        elif key == 'home':
            # Перейти к началу
            self.offset = 0
            self._update_plot()

        elif key == 'end':
            # Перейти к концу
            self.offset = max(0, self.total_samples - self.window_size)
            self._update_plot()

    def _on_close(self, event):
        """Обработчик закрытия окна."""
        self._running = False
        self._cleanup()

    def _cleanup(self):
        """Освобождает ресурсы mmap."""
        try:
            self._mm.close()
            self._fd.close()
        except Exception:
            pass

    def show(self):
        """Запускает интерактивный просмотр."""
        try:
            plt.show()
        finally:
            self._cleanup()


def main():
    parser = argparse.ArgumentParser(
        description="Signal Window Viewer — интерактивный просмотр сигнала (mmap, без загрузки в память)"
    )

    parser.add_argument("input", help="Бинарный файл с сигналом (float32 или complex64)")
    parser.add_argument("--fs", type=float, required=True,
                        help="Частота дискретизации (Гц)")
    parser.add_argument("--window", type=int, default=1024,
                        help="Размер окна просмотра (по умолчанию: 1024)")
    parser.add_argument("--complex", action="store_true",
                        help="Сигнал в формате complex64 (IQ)")

    args = parser.parse_args()

    # Проверка аргументов
    if args.fs <= 0:
        print("Ошибка: частота дискретизации должна быть положительной")
        sys.exit(1)

    if args.window <= 0:
        print("Ошибка: размер окна должен быть положительным")
        sys.exit(1)

    if not os.path.exists(args.input):
        print(f"Ошибка: файл не найден: {args.input}")
        sys.exit(1)

    file_size = os.path.getsize(args.input)
    bytes_per_sample = 8 if args.complex else 4
    total_samples = file_size // bytes_per_sample

    if total_samples == 0:
        print("Ошибка: файл пуст или имеет неверный формат")
        sys.exit(1)

    dtype_name = "complex64 (IQ)" if args.complex else "float32 (real)"
    print(f"Файл: {args.input}")
    print(f"Формат: {dtype_name}")
    print(f"Размер: {file_size} байт")
    print(f"Отсчётов: {total_samples}")
    print(f"Длительность: {total_samples / args.fs:.4f} с")

    # Запуск просмотрщика
    viewer = SignalViewer(
        filepath=args.input,
        sample_rate=args.fs,
        window_size=args.window,
        is_complex=args.complex
    )

    print("\n=== Управление ===")
    print("← / →        — прокрутка влево/вправо (половина окна)")
    print("PgUp / PgDn  — быстрая прокрутка (полное окно)")
    print("↑ / ↓        — приблизить / отдалить")
    print("Home / End   — перейти к началу/концу")
    print("q / Esc      — выход")
    print("==================\n")

    viewer.show()


if __name__ == "__main__":
    main()
