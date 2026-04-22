# Модули и интерфейс IModule

## Обзор

Проект `computing_server` построен по модульной архитектуре: каждый этап обработки сигнала реализован как отдельный динамически загружаемый модуль (shared library `.so`). Модули объединяются в цепочки — **конвейеры** — через конфигурацию `pipeline.json`.

Ключевые особенности:
- **20+ модулей** обработки (FIR-фильтрация, FFT, демодуляция, декимация и др.)
- **Динамическая загрузка** через `dlopen`/`dlsym` с разрешением путей через Redis
- **Единый интерфейс** `IModule` для всех модулей
- **Полиморфная передача данных** через `std::shared_ptr<IData>`
- **Три режима параметризации**: прямые значения, подстановка `$VAR` из `variables.toml`, ссылка `@tag` на данные из другого конвейера

---

## Интерфейс IModule

Каждый модуль реализует интерфейс `IModule` (`Module/include/IModule.hpp`):

| Метод | Назначение |
|---|---|
| `init()` | Инициализация: проверка параметров, подготовка структур, warm-up CUDA-ядра. Возвращает `true` при успехе. |
| `run()` | Основная логика обработки. Возвращает `true` при успехе. |
| `setParam(name, value)` | Установка параметра модуля. Каждый модуль сам знает, какие параметры он принимает. |
| `fetchParam(name, value)` | **Невиртуальный** метод из `IModule.cpp`. Разрешает переменные (`$VAR`, `@tag`) перед вызовом `setParam`. |
| `setData(data)` | Получение входных данных от предыдущего модуля в цепочке. |
| `getData()` | Возврат обработанных данных следующему модулю. |
| `getMetaData()` | Возврат метаданных модуля (имя, версия, описание). |

### Жизненный цикл модуля

```
ModuleFactory::createModule(name)
    ↓
Для каждого параметра из pipeline.json:
    module->fetchParam(name, jsonToAny(value))
        ├── $VAR  → Variables::get() → setParam()
        ├── @tag  → VirtualTransmitter::waitRxData() → setParam()
        └── прямое значение → setParam()
    ↓
Conveyor::init() → module->init()
    ↓
Цикл Conveyor::run():
    module[0]->run() → getData()
    data->isValid() ?
    module[1]->setData(data) → run() → getData()
    ...
```

---

## Динамическая загрузка модулей

### ModuleFactory

`ModuleFactory` (`Core/include/ModuleFactory.hpp`) отвечает за создание экземпляров модулей:

1. **Поиск пути** — по имени модуля запрашивает Redis по ключу `"<ModuleName>-module"` (например, `"FileSrc-module"`). Возвращает абсолютный путь к `.so`.
2. **Загрузка библиотеки** — `dlopen(libPath, RTLD_LAZY)`.
3. **Получение фабрики** — `dlsym(moduleHandle, "createModule")`.
4. **Создание модуля** — вызов фабричной функции `createModule()` → возвращает `IModule*`.

**Требования runtime:**
- Запущенный Redis (`127.0.0.1:6379`)
- Переменная окружения `REDISCLI_AUTH` с паролем

### Plugin API

Каждый модуль экспортирует фабричную функцию (`Modules/module.hpp`):

```cpp
#define PLUGIN_API extern "C" __attribute__((visibility("default")))

PLUGIN_API IModule* createModule();
```

Макрос `PLUGIN_API` гарантирует, что символ будет виден при динамической линковке.

---

## Разрешение параметров: fetchParam

Метод `fetchParam` реализован в `Module/src/IModule.cpp` и предоставляет три механизма подстановки значений:

### 1. Прямое значение
```json
{ "name": "FIR-filter", "params": { "decimation": 4 } }
```
Значение передаётся в `setParam` как есть.

### 2. Подстановка переменной `$VAR`
```json
{ "name": "Decimator", "params": { "factor": "$DECIMATION_FACTOR" } }
```
Строка начинается с `$`. Значение ищется в синглтоне `Variables`, который загружает `variables.toml`.

### 3. Ссылка на данные `@tag`
```json
{ "name": "FIR-filter", "params": { "taps": "@fir_rrc_coeff" } }
```
Строка начинается с `@`. Модуль регистрируется как получатель (`registerRx`) и **блокируется** в `waitRxData`, ожидая данные от другого конвейера. Полученные данные (`shared_ptr<IData>`) передаются в `setParam`.

---

## Примеры модулей

### FileSrc
- **Назначение:** Чтение бинарных данных из файла (mmap или блочное чтение)
- **Параметры:** `filePath` (string), `blockSize` (int)
- **Логика:** При `run()` читает очередной блок данных из файла и упаковывает в `GpuSignal`. Когда файл кончился — `getData()` возвращает контейнер с `size() == 0`, что сигнализирует конвейеру об остановке.

### FIR-filter
- **Назначение:** FIR-фильтрация сигнала (включая RRC)
- **Параметры:** `taps` (shared_ptr<IData> — коэффициенты фильтра), `decimation` (int)
- **Логика:** Получает входной сигнал и коэффициенты, запускает CUDA-ядро свёртки. Поддерживает буфер истории через `ensureHistoryLike`.

### CarrierRecovery
- **Назначение:** Восстановление несущей частоты (фазовая синхронизация)
- **Параметры:** `sampleRate` (float), `loopBandwidth` (float)
- **Логика:** Оценивает и компенсирует частотный и фазовый сдвиг во входном комплексном сигнале.

### QPSKDecision
- **Назначение:** Принятие решений по символам QPSK
- **Параметры:** `threshold` (float)
- **Логика:** По восстановленным символам определяет переданные биты.

---

## Передача данных внутри конвейера

Модули в одном конвейере обмениваются данными через `std::shared_ptr<IData>`:

1. Модуль `A` после `run()` возвращает `getData()` → `shared_ptr<IData>`
2. `Conveyor` проверяет `data->isValid()`
3. Передаёт в модуль `B` через `setData(data)`
4. Модуль `B` может привести тип: `asGpuSignal(data)` → `validateGpuInput(...)` → работа с `deviceDataRaw()`

Так как используется `shared_ptr`, данные передаются по ссылке (без копирования) внутри одного конвейера. Копирование происходит только при межконвейерной передаче через `VirtualTransmitter`.

---

## Диаграмма классов

![Диаграмма классов модулей](images/modules_class.png)

---

## Ключевые файлы

| Файл | Описание |
|---|---|
| `Module/include/IModule.hpp` | Базовый интерфейс модуля |
| `Module/src/IModule.cpp` | Реализация `fetchParam` |
| `Core/include/ModuleFactory.hpp` | Фабрика динамической загрузки |
| `Core/src/ModuleFactory.cpp` | Загрузка через Redis + dlopen |
| `Modules/module.hpp` | Макрос `PLUGIN_API` |
| `Module/include/Variables.hpp` | Синглтон для variables.toml |
