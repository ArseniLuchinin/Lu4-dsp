<!-- AGENTS.md — Инструкции для AI-агентов -->

# computing_server — Инструкции для AI-агентов

## Обзор проекта

**computing_server** — модульный CUDA-ускоренный сервер обработки цифровых сигналов. Реализует конвейерную (pipeline) архитектуру, где модули обработки объединяются в цепочки, определяемые в `pipeline.json`, с подстановкой переменных из `variables.toml`.

### Основные возможности
- QPSK демодуляция (CarrierRecovery, QPSKDecision)
- FIR-фильтрация (включая RRC — Root Raised Cosine)
- FFT и спектроскопия
- Децимация сигналов
- Визуализация (spectrogram plot)
- Чтение/запись файлов с данными
- Передача данных между конвейерами через VirtualTransmitter

### Технологический стек
- **Язык:** C++17 + CUDA 17
- **Build system:** CMake 3.21+ с генератором Ninja
- **Зависимости:**
  - CUDAToolkit (с поддержкой архитектуры sm_75)
  - Boost (log, log_setup, thread, system, json)
  - tomlplusplus (чтение variables.toml)
  - hiredis + redis++ (реестр модулей)
  - Google Test (тестирование)

---

## Архитектура

### Компоненты

| Компонент | Назначение |
|---|---|
| **Core** | Оркестрация конвейеров (ConveyorOrchestrator, ConveyorFactory, ModuleFactory, Conveyor) |
| **DataContainers** | CPU/GPU контейнеры данных (GpuSignal, CpuFloatSignal, CpuComplexSignal, EmptyContainer) |
| **Module** | Базовые интерфейсы (IModule, IData, VirtualTransmitter, IVirtualRX, Variables) и логгер |
| **Modules** | 20 динамически загружаемых модулей обработки (shared libraries) |
| **tests** | E2E и интеграционные тесты |
| **utils** | Python-скрипты для генерации тестовых сигналов |

### Динамическая загрузка модулей

Модули компилируются как разделяемые библиотеки (`.so`). `ModuleFactory` ищет путь к библиотеке по имени модуля в Redis (ключ вида `"<ModuleName>-module"`), затем загружает через `dlopen`/`dlsym` и вызывает фабричную функцию `createModule()`. Для работы требуется запущенный Redis и переменная окружения `REDISCLI_AUTH`.

### Конвейер выполнения

1. **setParam** — модулям передаются параметры из `pipeline.json` через `fetchParam()` (с разрешением переменных `$VAR` и тегов `@tag`)
2. **init** — инициализация модулей (проверка параметров, подготовка структур, warm-up CUDA-ядра)
3. **run** — запуск первого модуля (он сам формирует данные, например читает из файла)
4. **isValid** — проверка валидности выходных данных
5. **setData** — передача данных следующему модулю через `std::shared_ptr<IData>`
6. **run** — обработка данных модулем
7. Повторение шагов 4-6 для всех модулей цепочки

### Передача данных между конвейерами

`VirtualTransmitter` реализует широковещательную рассылку данных между конвейерами по именованным тегам:
- `txData(data, tag)` — отправка данных из модуля VirtualTX
- `rxData(tag)` / `waitRxData(tag)` — получение данных в модуле VirtualRX
- `registerTx(tag)` / `registerRx(tag)` — регистрация отправителя/получателя
- Механизм гарантирует, что каждый RX получает копию данных

В `pipeline.json` для ссылки на данные из другого конвейера используется синтаксис `@tag_name` в значении параметра.

### Конфигурация

- **`pipeline.json`** — определяет конвейеры, порядок модулей и их параметры. Поддерживает подстановку `$VAR_NAME` (переменные из `variables.toml`) и `@tag_name` (ссылки на VirtualTransmitter).
- **`variables.toml`** — глобальные переменные для подстановки в `pipeline.json`.

### Интерфейс IModule

Каждый модуль реализует:
- `init()` — инициализация (проверка параметров, подготовка)
- `run()` — основная логика обработки
- `setParam(name, value)` — установка параметров
- `setData(data)` — получение входных данных
- `getData()` — возврат обработанных данных
- `getMetaData()` — метаданные модуля

Также экспортируется фабричная функция:
```cpp
extern "C" IModule* createModule();
```

### Иерархия данных

- `IData` — базовый интерфейс (size, availableSize, reserve, isValid, copy)
- `IGpuSignalData` — интерфейс GPU-данных (sampleType, deviceDataRaw)
- `GpuSignal<T>` — шаблонный класс GPU-контейнера (T = float, cuComplex, uint8_t)
  - `GpuFloatSignal` = `GpuSignal<float>`
  - `GpuComplexFloatSignal` = `GpuSignal<cuComplex>`
  - `GpuByteSignal` = `GpuSignal<uint8_t>`
- `CpuFloatSignal`, `CpuComplexSignal` — CPU-контейнеры
- `EmptyContainer` — пустой контейнер-заглушка

Правило: память выделяется только в конструкторе/методе `reserve()`. Метод `copy()` создаёт глубокую копию (через `cudaMemcpy` для GPU).

---

## Структура проекта

```
CMakeLists.txt              — корневой конфиг сборки
CMakePresets.json           — пресеты configure/build/test
main.cpp                    — точка входа, инициализация логирования
pipeline.json               — конфигурация конвейеров
variables.toml              — переменные для подстановки

Core/
  include/                  — ConveyorOrchestrator, Conveyor, ConveyorFactory, ModuleFactory
  src/
  CMakeLists.txt

DataContainers/
  include/                  — IData, IGpuSignalData, GpuSignal, Cpu*Signal, EmptyContainer
  src/
  CMakeLists.txt

Module/
  include/                  — IModule, IVirtualRX, VirtualTransmitter, Variables, logger
  src/
  CMakeLists.txt

Modules/
  module.hpp                — макрос PLUGIN_API и объявление createModule
  <ModuleName>/
    include/<ModuleName>.hpp
    src/<ModuleName>.cpp    — (опционально <ModuleName>Cuda.cu)
    CMakeLists.txt
    module.json             — метаданные модуля (имя, описание, поля с типами)
    README.md               — пользовательская документация модуля
    tests/                  — опциональные модульные тесты (CMakeLists.txt + *.cpp)

server/                     — управляющий сервер
  server.py                 — HTTP API + Socket.IO (стриминг stdout/stderr сессий)

tests/                      — E2E/интеграционные тесты
  qpsk_mvp_tests.cpp        — E2E-тест QPSK-цепочки
  data_container_copy_tests.cpp
  virtual_tx_rx_tests.cpp
  fetch_param_tests.cpp
  benchmark.py              — скрипт бенчмаркинга (ctest + JUnit → CSV)
  benchmark_results.csv     — результаты бенчмарков
  CMakeLists.txt

utils/                      — Python-скрипты генерации сигналов
  qpsk_generator.py
  AMgenerate.py
  FMgenerate.py
  AMCgenerate.py
  signal_viewer.py
  CMakeLists.template       — шаблон CMakeLists.txt для нового модуля
  CreateModuleDir.sh        — скрипт создания директории модуля
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
| `SpectrogramPlot` | Генерация спектрограммы |
| `BandPassCompute` | Вычисление полосового фильтра (real) |
| `BandPassComputeComplex` | Вычисление полосового фильтра (complex) |
| `BitPacker` | Упаковка битов |
| `CS2AS` | Конвертер Complex → Array of Structures |
| `PhaseRotator` | Фазовое вращение |
| `FileSrc` | Чтение данных из файла (mmap, блочное) |
| `FileWriter` | Запись бинарных данных в файл |
| `TextFileWriter` | Запись текста в файл |
| `VirtualTX` | Виртуальная передача данных между конвейерами |
| `VirtualRX` | Виртуальный приём данных между конвейерами |
| `SumReduce` | Суммирование/редукция |

---

## Сборка и запуск

### Конфигурация
```bash
cmake --preset debug
```
Пресет `debug` настраивает Debug-сборку с `BUILD_TESTING=ON`, генератором Ninja и `CMAKE_EXPORT_COMPILE_COMMANDS=ON`.

### Сборка
```bash
cmake --build --preset debug          # все таргеты
cmake --build --preset debug -j 6     # явное указание параллелизма
```

### Запуск сервера
```bash
./build/computing_server [path/to/pipeline.json]
# По умолчанию: pipeline.json в корне проекта
```

### Зависимости runtime
- Доступный GPU с поддержкой CUDA architecture 75 (sm_75)
- Запущенный Redis сервер (127.0.0.1:6379)
- Переменная окружения `REDISCLI_AUTH` с паролем Redis

---

## Тестирование

### Запуск тестов
```bash
ctest --preset all                    # все тесты
ctest --preset module                 # только модульные тесты (LABEL "module")
ctest --preset e2e                    # только end-to-end тесты (LABEL "e2e")
ctest --preset all -R "Pattern" -V    # конкретный тест с выводом
```

### Структура тестов
- **Модульные тесты** — находятся в `Modules/<Name>/tests/` (например, Decimator, QPSKDecision, CarrierRecovery)
- **E2E/интеграционные** — в `tests/`:
  - `qpsk_mvp_tests.cpp` — полная QPSK-цепочка
  - `data_container_copy_tests.cpp` — копирование GPU/CPU контейнеров
  - `virtual_tx_rx_tests.cpp` — VirtualTransmitter
  - `fetch_param_tests.cpp` — разрешение переменных и тегов

### Стиль тестов
- Фреймворк: Google Test
- Формат: `TEST(TestSuiteName, TestName)` — без fixtures
- Именование: `CamelCase_DescriptiveName_Scenario_ExpectedResult[_Red]`
- Ассерты: `ASSERT_TRUE()`, `EXPECT_EQ()`, `ASSERT_NE()`, `ASSERT_FALSE()`
- Пропуск при отсутствии CUDA:
  ```cpp
  if (!isCudaAvailable()) {
      GTEST_SKIP() << "CUDA device is unavailable.";
  }
  ```

### Первый запуск (если тесты выключены)
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j 6
```

### Бенчмаркинг

Скрипт `tests/benchmark.py` запускает тесты через `ctest --output-junit`, парсит XML для извлечения времени выполнения и сохраняет результаты в `tests/benchmark_results.csv`.

```bash
python tests/benchmark.py --preset all --repeat 5 --tests "Pattern"
```

- `--preset` — пресет ctest (по умолчанию `all`)
- `--repeat` — количество прогонов каждого теста
- `--tests` — regex-фильтр имён тестов

---

## Конвенции разработки

### Код
- **C++17**, без исключений и RTTI
- **Возврат ошибок:** `true` = успех, `false` = ошибка
- **Умные указатели:** `std::shared_ptr` для данных, `std::make_shared<T>()` для создания
- **Без `std::unique_ptr`**
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
| Свободные функции (`.cpp`) | `camelCase` | `downloadGpuBytes()`, `readAllBytes()` |

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

### Error Handling
- **Boolean returns**: `true` = успех, `false` = ошибка
- **Guard checks** в начале методов с early return
- **Логирование** через Boost.Log:
  ```cpp
  ERROR << "ClassName::method failed: description." << std::endl;
  DEBUG << "message" << std::endl;
  ```
- **CUDA errors**: проверка каждого вызова на `cudaSuccess`, `cudaGetErrorString()`
- **Без исключений** в коде модулей

### Комментарии и документация
- **Интерфейсные методы**: Doxygen-стиль на русском
  ```cpp
  /*!\n   * @brief init инициализирует модуль из json\n   * @return true если модуль успешно инициализирован\n   */
  ```
- **Реализация**: English, обычные `//` комментарии
- **Краткие методы**: `/// @brief run запускает модуль`

### Форматирование
- Отступ 4 пробела (без табуляции)
- Фигурные скобки на одной строке для функций/контроля потока
- `override` на всех виртуальных методах
- `#ifndef`/`#define`/`#endif` header guards

---

## Добавление нового модуля

1. Создать `Modules/<ModuleName>/` с `include/`, `src/`, `CMakeLists.txt`
2. Создать `Modules/<ModuleName>/module.json` — метаданные модуля: имя, описание и список полей с типами (`string`, `int`, `real`, `bool`, `enum`)
3. Реализовать интерфейс `IModule`: `init()`, `run()`, `setParam()`, `setData()`, `getData()`, `getMetaData()`
4. Экспортировать `extern "C" IModule* createModule()` factory-функцию
5. Зарегистрировать модуль в `Modules/module.hpp`
6. Добавить тесты в `Modules/<ModuleName>/tests/` (опционально)
7. Создать `Modules/<ModuleName>/README.md` — описание для пользователя + таблица параметров

Шаблоны: `utils/CMakeLists.template`, `utils/CreateModuleDir.sh`.

### Правило документации модулей

Каждый модуль имеет `README.md` в своей директории — пользовательская документация (что делает, параметры, алгоритм).

**При изменении модуля обновлять его `README.md` в том же коммите:**
- Добавление/удаление/переименование параметров
- Изменение типов или правил валидации параметров
- Изменение алгоритма или логики обработки
- Изменение типов входных/выходных данных

---

## Безопасность

- **Redis:** пароль передаётся через переменную окружения `REDISCLI_AUTH`, не хардкодится
- **Динамическая загрузка:** модули загружаются через `dlopen` по путям из Redis — валидируйте пути при изменении логики ModuleFactory
- **Файловый доступ:** модули FileSrc/FileWriter работают с произвольными путями из конфига — в `pipeline.json` указываются абсолютные пути
- **CUDA:** проверяйте возвращаемые значения всех CUDA-вызовов; используйте `cudaDeviceSynchronize()` при необходимости
- **Исключения:** запрещены в коде модулей — используйте boolean-возвраты и логирование

---

## Ключевые файлы

| Файл | Описание |
|---|---|
| `pipeline.json` | Конфигурация конвейеров обработки |
| `variables.toml` | Глобальные переменные для подстановки |
| `CMakePresets.json` | Пресеты сборки и тестирования |
| `docs/server_api.md` | Документация HTTP API управляющего сервера (`server/server.py`) |
| `ARCHITECTURE.md` | Описание архитектуры приложения |
| `QWEN.md` | Документация проекта (контекст для разработки) |
| `main.cpp` | Точка входа + инициализация логирования |
| `Module/include/IModule.hpp` | Базовый интерфейс модуля |
| `Modules/module.hpp` | Регистрация фабричной функции createModule |
| `Module/include/VirtualTransmitter.hpp` | Механизм межконвейерной передачи данных |
| `DataContainers/include/IData.hpp` | Базовый интерфейс данных |
| `DataContainers/include/GpuSignal.hpp` | Шаблонный GPU-контейнер |
