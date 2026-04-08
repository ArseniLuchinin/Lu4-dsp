# QWEN.md — Computing Server Project Context

## Project Overview

**computing_server** — это модульный CUDA-ускоренный сервер обработки цифровых сигналов. Проект реализует конвейерную (pipeline) архитектуру, где модули обработки chaining-ятся в цепочки, определяемые в `pipeline.json`, с подстановкой переменных из `variables.toml`.

### Основные возможности
- QPSK демодуляция (CarrierRecovery, QPSKDecision)
- FIR-фильтрация (включая RRC — Root Raised Cosine)
- FFT и спектроскопия
- Децимация сигналов
- Визуализация (spectrogram plot)
- Чтение/запись файлов с данными

### Технологии
- **Язык:** C++17 + CUDA 17
- **Build system:** CMake 3.21+ с Ninja
- **Зависимости:** CUDAToolkit, hiredis, redis++, Boost (log, thread, system, json), tomlplusplus, GTest
- **Тестирование:** Google Test

---

## Building and Running

### Конфигурация
```bash
cmake --preset debug
```

### Сборка
```bash
cmake --build --preset debug          # все таргеты
cmake --build --preset debug -j 6     # явное указание параллелизма
```

### Запуск
```bash
./build/computing_server [path/to/pipeline.json]
# По умолчанию: /home/luchinin/my_source/Course_poject/Server/pipeline.json
```

### Тесты
```bash
ctest --preset all                    # все тесты
ctest --preset module                 # только модульные тесты
ctest --preset e2e                    # только end-to-end тесты

# Один конкретный тест:
ctest --preset all -R "TestNamePattern" -V
```

### Включение тестов (первый раз)
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j 6
```

---

## Project Structure

```
CMakeLists.txt              — корневой конфиг сборки
CMakePresets.json           — пресеты configure/build/test
main.cpp                    — точка входа
pipeline.json               — конфигурация конвейеров
variables.toml              — переменные для подстановки в pipeline.json

Core/                       — оркестрация конвейеров (ConveyorOrchestrator)
DataContainers/             — CPU/GPU контейнеры данных
Module/                     — базовый интерфейс IModule
Modules/                    — 19 модулей обработки
  <ModuleName>/
    include/<ModuleName>.hpp
    src/<ModuleName>.cpp
    CMakeLists.txt
    tests/                  — опциональные тесты модуля
tests/                      — E2E/integration тесты
utils/                      — Python скрипты для генерации сигналов
```

### Доступные модули
| Модуль | Описание |
|---|---|
| `RRCCompute` | Вычисление коэффициентов RRC-фильтра |
| `FIR-filter` | FIR-фильтрация сигнала |
| `Decimator` | Децимация (понижение частоты дискретизации) |
| `CarrierRecovery` | Восстановление несущей (фазовая синхронизация) |
| `CarrierPhaseEstimator` | Оценка фазы несущей |
| `QPSKDecision` | Принятие решений по символам QPSK |
| `FFT` | Быстрое преобразование Фурье |
| `FFT_Shift` | Сдвиг спектра FFT |
| `SpectrogramPlot` | Генерация спектроскопии |
| `BandPassCompute` | Вычисление полосового фильтра (real) |
| `BandPassComputeComplex` | Вычисление полосового фильтра (complex) |
| `BitPacker` | Упаковка битов |
| `CS2AS` | Конвертер Complex → Array of Structures |
| `PhaseRotator` | Фазовое вращение |
| `FileSrc` | Чтение данных из файла |
| `TextFileWriter` | Запись текста в файл |
| `VirtualTX` | Виртуальная передача данных между конвейерами |
| `VirtualRX` | Виртуальный приём данных между конвейерами |
| `SumReduce` | Суммирование/редукция |

---

## Architecture

### Конвейер выполнения
1. **setParam** — модулям передаются параметры из `pipeline.json`
2. **init** — инициализация модулей (проверка параметров, подготовка структур)
3. **run** — запуск первого модуля (он сам формирует данные)
4. **isValid** — проверка валидности выходных данных
5. **setData** — передача данных следующему модулю
6. **run** — обработка данных модулем
7. Повторение шагов 4-6 для всех модулей цепочки

### Конфигурация
- **pipeline.json** — определяет конвейеры, порядок модулей и их параметры. Поддерживает подстановку `$VAR_NAME`.
- **variables.toml** — глобальные переменные для подстановки.

### Интерфейс IModule
Каждый модуль реализует:
- `init()` — инициализация (проверка параметров, подготовка)
- `run()` — основная логика обработки
- `setParam(name, value)` — установка параметров
- `setData(data)` — получение входных данных
- `getData()` — возврат обработанных данных
- `getMetaData()` — метаданные модуля

### VirtualTransmitter
Механизм передачи данных между конвейерами через именованные теги:
- `txData(data, tag)` — отправка
- `rxData(tag)` — получение (без ожидания)
- `waitRxData(tag)` — получение с ожиданием
- `checkData(tag)` — проверка наличия

---

## Development Conventions

### Код
- **C++17**, без исключений и RTTI
- **Возврат ошибок:** `true` = успех, `false` = ошибка
- **Умные указатели:** `std::shared_ptr` для данных, `std::make_shared<T>()` для создания
- **CUDA:** сырые указатели только для device memory (`cudaMalloc`/`cudaFree`)

### Именование
| Элемент | Стиль | Пример |
|---|---|---|
| Классы | `PascalCase` | `Decimator`, `GpuComplexFloatSignal` |
| Интерфейсы | `I` + `PascalCase` | `IModule`, `IData` |
| Методы | `camelCase` | `init()`, `setParam()` |
| Члены класса | `m_` + `camelCase` | `m_samplesPerSymbol` |
| Локальные переменные | `camelCase` | `inputSize` |
| Константы | `k` + `PascalCase` | `kQpskSps` |
| CUDA ядра | `camelCase` + `Kernel` | `runDecimatorKernel()` |

### Includes (упорядоченные)
```cpp
// 1. Заголовки проекта (алфавитный порядок)
#include <IModule.hpp>

// 2. Внешние библиотеки
#include <cuda_runtime.h>
#include <boost/log/trivial.hpp>

// 3. Стандартная библиотека (алфавитный порядок)
#include <memory>
#include <vector>
```

### Логирование (Boost.Log)
```cpp
ERROR << "ClassName::method failed: description." << std::endl;
DEBUG << "message" << std::endl;
```

### Тесты
- **Фреймворк:** Google Test
- **Стиль:** `TEST(TestSuiteName, TestName)` — без fixtures
- **Именование:** `CamelCase_DescriptiveName_Scenario_ExpectedResult[_Red]`
- **Ассерты:** `ASSERT_TRUE()`, `EXPECT_EQ()`, `ASSERT_NE()`, `ASSERT_FALSE()`
- `GTEST_SKIP()` для пропусков (например, нет CUDA)

### Форматирование
- Отступ 4 пробела (без табуляции)
- Фигурные скобки на одной строке для функций/контроля потока
- `override` на всех виртуальных методах
- `#ifndef`/`#define`/`#endif` header guards

---

## Adding a New Module

1. Создать `Modules/<ModuleName>/` с `include/`, `src/`, `CMakeLists.txt`
2. Реализовать интерфейс `IModule`: `init()`, `run()`, `setParam()`, `setData()`, `getData()`, `getMetaData()`
3. Экспортировать `extern "C" IModule* createModule()` factory-функцию
4. Зарегистрировать модуль в `Modules/module.hpp`
5. Добавить тесты в `Modules/<ModuleName>/tests/` (опционально)

---

## Key Files

| Файл | Описание |
|---|---|
| `pipeline.json` | Конфигурация конвейеров обработки |
| `variables.toml` | Глобальные переменные для подстановки |
| `CMakePresets.json` | Пресеты сборки и тестирования |
| `AGENTS.md` | Guidelines для coding agents |
| `ARCHITECTURE.md` | Описание архитектуры приложения |
| `main.cpp` | Точка входа + инициализация логирования |
