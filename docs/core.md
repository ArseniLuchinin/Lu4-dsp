# Core — оркестрация конвейеров

## Обзор

Компонент **Core** отвечает за создание, инициализацию и выполнение конвейеров обработки. Он читает конфигурацию из `pipeline.json`, создаёт цепочки модулей, запускает каждую цепочку в отдельном потоке и ожидает завершения всех потоков.

Ключевые классы:
- **ConveyorOrchestrator** — верхнеуровневая оркестрация всей системы
- **ConveyorFactory** — создание конвейера из JSON-объекта
- **Conveyor** — выполнение одной цепочки модулей
- **ModuleFactory** — динамическая загрузка модулей (`.so`)

---

## ConveyorOrchestrator

`ConveyorOrchestrator` (`Core/include/ConveyorOrchestrator.hpp`) — точка входа для запуска системы.

### Поля

| Поле | Тип | Назначение |
|---|---|---|
| `m_configPath` | `string` | Путь к `pipeline.json` |
| `m_root` | `boost::json::value` | Распарсенный JSON конфига |
| `m_loaded` | `bool` | Флаг успешной загрузки JSON |
| `m_moduleFactory` | `ModuleFactory` | Фабрика модулей |
| `m_conveyorFactory` | `ConveyorFactory` | Фабрика конвейеров |
| `m_conveyorConfigs` | `vector<ConveyorConfig>` | JSON-объекты конвейеров |
| `m_runtimes` | `vector<Runtime>` | Конвейер + поток + время выполнения |

### Методы

#### `load()`
- Открывает `pipeline.json`, читает содержимое
- Парсит через `boost::json::parse`
- При ошибке — логирует и возвращает `false`

#### `run()` — основной метод

```
1. Проверяет m_loaded
2. loadVariables() → Variables::instance().load("variables.toml")
3. buildConveyors() → извлекает массив "conveyors" из JSON
4. Для каждого конфига:
    a. Создаёт std::thread
    b. ConveyorFactory::createFromJsonObject(config)
    c. conveyor->init()
    d. while (conveyor->run()) {}
    e. Сохраняет elapsed time
5. join() всех потоков
6. Логирует total time каждого конвейера
```

**Важно:** каждый конвейер выполняется в **собственном потоке**. Ошибки внутри потока (исключения или `init() == false`) перехватываются, но основной метод `run()` всё равно дожидается `join()` и возвращает `true`.

---

## ConveyorFactory

`ConveyorFactory` (`Core/include/ConveyorFactory.hpp`) создаёт конвейер из JSON-конфигурации.

### Метод `createFromJsonObject(conveyorObj)`

1. Извлекает `"name"` — имя конвейера (обязательно)
2. Создаёт `Conveyor(name)`
3. Извлекает `"modules"` — массив JSON-объектов
4. Для каждого модуля вызывает `buildModule(conveyor, moduleObj)`
5. Возвращает готовый конвейер

### Метод `buildModule(conveyor, moduleObj)`

1. Извлекает `"name"` — имя модуля
2. `ModuleFactory::createModule(name)` — создание экземпляра
3. Если есть `"params"` — для каждого параметра:
   - `module->fetchParam(name, jsonToAny(value))`
4. `conveyor.addModule(module)`

### Метод `jsonToAny(value)`

Конвертация `boost::json::value` → `std::any`:

| JSON-тип | C++-тип |
|---|---|
| `string` | `std::string` |
| `bool` | `bool` |
| `int64` | `int32_t` |
| `uint64` | `int32_t` (или `double`, если > INT32_MAX) |
| `double` | `double` |
| `null` | пустой `std::any` |

---

## Conveyor

`Conveyor` (`Core/include/Conveyor.hpp`) — исполнитель одной цепочки модулей.

### Поля

| Поле | Тип | Назначение |
|---|---|---|
| `m_conveyorName` | `string` | Имя конвейера |
| `m_isInitialized` | `bool` | Флаг инициализации |
| `m_modules` | `vector<shared_ptr<IModule>>` | Цепочка модулей |

### Метод `init()`
- Проверяет, что `m_modules` не пуст
- Для каждого модуля вызывает `module->init()`
- Если любой вернул `false` → возвращает `false`
- Устанавливает `m_isInitialized = true`

### Метод `run()` — одна итерация

```
1. Берёт m_modules.front() — первый модуль (source)
2. module->run(). При ошибке → false
3. Замеряет время выполнения (логирует в ms)
4. auto data = module->getData()
5. Если data->size() == 0:
    → логирует "finished"
    → возвращает false (сигнал остановки конвейера)
6. Для каждого последующего модуля (i = 1..N-1):
    a. data->isValid() ? иначе ошибка
    b. module->setData(data) ? иначе ошибка (неверный тип)
    c. module->run() ? иначе ошибка
    d. Замеряет время
    e. data = module->getData() — новые данные для следующего
7. Возвращает true (итерация успешна)
```

**Ключевой момент:** первый модуль (например, `FileSrc`) сам формирует данные. Когда данных больше нет (`size() == 0`), это означает **нормальное завершение** конвейера.

---

## ModuleFactory

`ModuleFactory` (`Core/include/ModuleFactory.hpp`) загружает модули как shared libraries.

### Метод `findModule(name)`
- Ключ в Redis: `name + "-module"` (например, `"CarrierRecovery-module"`)
- Возвращает путь к `.so` или пустую строку

### Метод `createModule(name)`
```
1. findModule(name) → путь к .so
2. dlopen(path, RTLD_LAZY) → moduleHandle
3. dlsym(moduleHandle, "createModule") → factory function
4. Вызывает factory() → IModule*
```

**Примечание:** `moduleHandle` от `dlopen` нигде не сохраняется — библиотека остаётся загруженной до завершения процесса.

---

## Обработка ошибок

| Этап | Поведение при ошибке |
|---|---|
| `load()` | Возвращает `false` |
| `loadVariables()` | Возвращает `false` |
| `buildConveyors()` | Возвращает `false` |
| Создание конвейера в потоке | Исключение перехватывается, поток завершается |
| `Conveyor::init()` | Возвращает `false` → Orchestrator логирует ошибку |
| `Conveyor::run()` | Возвращает `false` → цикл `while` прерывается |
| `data->isValid() == false` | Конвейер останавливается |
| `setData()` вернул `false` | Конвейер останавливается |
| `data->size() == 0` | **Нормальное завершение** — конец данных |

---

## Последовательность: от pipeline.json до работающей цепочки

```
main()
  └── ConveyorOrchestrator orchestrator(configPath)
        └── load()
              └── читает pipeline.json → m_root
        └── run()
              ├── loadVariables(root) → Variables::instance().load("variables.toml")
              ├── buildConveyors() → m_conveyorConfigs
              ├── Создаёт std::thread для каждого конфига:
              │     └── ConveyorFactory::createFromJsonObject(config)
              │           └── создаёт Conveyor(name)
              │           └── для каждого модуля в JSON:
              │                 ├── ModuleFactory::createModule(name)
              │                 │     └── findModule(name) → Redis
              │                 │     └── dlopen(path) → dlsym("createModule") → IModule*
              │                 ├── module->fetchParam(key, jsonToAny(value))
              │                 │     └── разрешает $VAR → Variables::get()
              │                 │     └── разрешает @tag → VirtualTransmitter::waitRxData()
              │                 └── conveyor.addModule(module)
              │     └── conveyor->init()
              │           └── для каждого module: module->init()
              │     └── while (conveyor->run()) {}
              │           └── module[0]->run() → getData()
              │           └── если size()==0 → finished
              │           └── для module[i>0]: setData(data) → run() → getData()
              └── join() всех потоков
              └── логирует elapsed time
```

---

## Диаграмма классов

![Диаграмма классов Core](images/core_class.png)

---

## Диаграмма последовательности

![Диаграмма последовательности запуска](images/core_sequence.png)

---

## Ключевые файлы

| Файл | Описание |
|---|---|
| `Core/include/ConveyorOrchestrator.hpp` | Верхнеуровневая оркестрация |
| `Core/src/ConveyorOrchestrator.cpp` | Загрузка JSON, потоки, join |
| `Core/include/ConveyorFactory.hpp` | Создание конвейера из JSON |
| `Core/src/ConveyorFactory.cpp` | buildModule, jsonToAny |
| `Core/include/Conveyor.hpp` | Цепочка модулей |
| `Core/src/Conveyor.cpp` | init(), run() |
| `Core/include/ModuleFactory.hpp` | Динамическая загрузка .so |
| `Core/src/ModuleFactory.cpp` | Redis, dlopen, dlsym |
