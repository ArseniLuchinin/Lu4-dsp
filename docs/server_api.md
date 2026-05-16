# HTTP API управляющего сервера

Документация к `server/server.py` — управляющему Flask-серверу, который предоставляет REST API для клиентского приложения и запускает вычислительный бэкенд (`computing_server`).

> **Базовый URL:** `http://<host>:<SERVER_PORT>` (порт задаётся переменной окружения `SERVER_PORT`, по умолчанию `5000`).

---

## Общие сведения

- Формат обмена: **JSON**.
- Все ответы содержат поле `status`: `"ok"` или `"error"`.
- При ошибках возвращается соответствующий HTTP-код (`4xx`, `5xx`) и поле `message` с описанием.
- Сервер поддерживает **Socket.IO** для real-time передачи логов запущенных расчётов.

### Переменные окружения

| Переменная | Назначение | Значение по умолчанию |
|---|---|---|
| `SERVER_PORT` | Порт HTTP-/Socket.IO-сервера | `5000` |
| `FLASK_SECRET_KEY` | Секретный ключ Flask (используется Socket.IO) | `dev-secret-key` |
| `COMPUTING_SERVER_PATH` | Путь к исполняемому файлу `computing_server` | `../build/computing_server` |
| `SESSIONS_DIR` | Директория для хранения сессий | `./sessions` |
| `MODULES_DIR` | Корневая директория с модулями | `build/Modules` |

---

## Эндпоинты

### `GET /`

Проверка работоспособности API.

**Ответ (`200 OK`):**
```json
{
  "status": "ok",
  "message": "Computing server API is running"
}
```

---

### `GET /modules/list`

Возвращает список всех модулей, найденных в директории `MODULES_DIR`.

**Логика работы:**
1. Сканирует директорию `MODULES_DIR`.
2. Для каждой поддиректории `<name>` проверяет наличие `module.json` и `README.md`.

**Ответ (`200 OK`):**
```json
{
  "status": "ok",
  "count": 2,
  "modules": [
    {
      "name": "FFT",
      "hasModuleJson": true,
      "hasReadme": true
    },
    {
      "name": "FileSrc",
      "hasModuleJson": true,
      "hasReadme": false
    }
  ]
}
```

---

### `GET /modules/<name>`

Возвращает детальную информацию о конкретном модуле.

**Параметры URL:**
- `name` — имя модуля (имя поддиректории в `MODULES_DIR`).

**Логика работы:**
1. Проверяет существование поддиректории `<name>` в `MODULES_DIR`.
2. Читает и парсит `MODULES_DIR/<name>/module.json`.
3. Читает `MODULES_DIR/<name>/README.md` (если есть).

**Ответ (`200 OK`):**
```json
{
  "status": "ok",
  "name": "FFT",
  "moduleJson": { /* содержимое module.json */ },
  "readme": "# FFT\nОписание модуля..."
}
```

**Возможные ошибки:**
- `404 Not Found` — модуль не найден в `MODULES_DIR`.
- `500 Internal Server Error` — ошибка чтения или парсинга `module.json` / `README.md`.

---

### `POST /start`

Запускает вычислительный сервер (`computing_server`) с заданным конвейером и переменными.

**Тело запроса (JSON):**

| Поле | Тип | Обязательное | Описание |
|---|---|---|---|
| `pipeline` | `object` или `string` | Да | Конфигурация конвейера (JSON-объект или JSON-строка). |
| `variables` | `string` | Да | Содержимое файла `variables.toml` (сырой текст). |
| `returnDirectory` | `string` | Нет | Относительный путь к директории для сохранения результатов внутри сессии. По умолчанию: `results`. |

**Пример тела запроса:**
```json
{
  "pipeline": {
    "conveyors": [
      {
        "name": "Дорожка 1",
        "modules": [
          {
            "name": "FileSrc",
            "params": { "file name": "/data/input.bin" }
          }
        ]
      }
    ]
  },
  "variables": "sampleRate = 1000000\ncarrierFreq = 250000"
}
```

**Логика работы:**
1. Проверяет наличие полей `pipeline` и `variables`.
2. Создаёт директорию сессии: `./server/sessions/session_<uuid>/`.
3. Создаёт директорию для результатов (`returnDirectory`, по умолчанию `results`) внутри директории сессии. Защищена от path traversal.
4. Записывает `variables` в файл `variables.toml` внутри директории сессии.
5. Если `pipeline` передан как строка — парсит её в JSON-объект.
6. Дополняет JSON-объект конвейера полем `variables` с абсолютным путём к сохранённому `variables.toml`.
7. Записывает итоговый JSON в файл `pipeline.json` внутри директории сессии.
8. Запускает `../build/computing_server <путь_k_pipeline.json>` в фоновом процессе (`subprocess.Popen`).
9. STDOUT и STDERR вычислительного сервера транслируются в реальном времени через Socket.IO в **room** с именем `session_id`, а также сохраняются в лог-файл `logs_YYYYMMDD_HHMMSS.txt`.

**Ответ (`200 OK`):**
```json
{
  "status": "ok",
  "session_id": "session_a1b2c3d4e5f67890",
  "pid": 12345
}
```

**Возможные ошибки:**
- `400 Bad Request` — отсутствуют обязательные поля `pipeline` / `variables`, или `pipeline` содержит невалидный JSON (если передан как строка), или `returnDirectory` выходит за пределы директории сессии (path traversal).
- `500 Internal Server Error` — ошибка создания директории сессии, записи файлов или запуска вычислительного сервера.

---

### `GET /sessions/<session_id>/results/<filename>`

Возвращает конкретный файл из директории результатов (`returnDirectory`) указанной сессии.

**Параметры URL:**
- `session_id` — идентификатор сессии, возвращённый `POST /start`.
- `filename` — имя файла относительно директории результатов сессии (может содержать `/` для вложенных файлов).

**Логика работы:**
1. Проверяет существование сессии в `session_meta`.
2. Проверяет существование директории результатов (`returnDirectory`) для данной сессии.
3. Строит абсолютный путь к файлу и проверяет, что он находится внутри `returnDirectory` (защита от path traversal).
4. Проверяет, что путь является файлом.
5. Отправляет файл клиенту с заголовком `Content-Disposition: attachment`.

**Ответ (`200 OK`):**
Бинарное содержимое файла с MIME-типом `application/octet-stream`.

**Возможные ошибки:**
- `400 Bad Request` — `filename` выходит за пределы директории результатов (path traversal).
- `404 Not Found` — сессия не найдена, директория результатов не существует, или файл не найден.
- `500 Internal Server Error` — ошибка чтения файла с диска.

---

## Socket.IO события

Сервер использует `flask-socketio`. Базовый URL тот же, что и для HTTP (`ws://<host>:<SERVER_PORT>`).

### `join`

Подписывает клиента на логи указанной сессии.

**Клиент → Сервер:**
```json
{
  "session_id": "session_a1b2c3d4e5f67890"
}
```

**Сервер → Клиент (`joined`):**
```json
{
  "session_id": "session_a1b2c3d4e5f67890"
}
```

### `log`

Сервер рассылает строки stdout/stderr вычислительного процесса всем клиентам, подписанным на room `session_id`.

**Сервер → Клиент:**
```json
{
  "session_id": "session_a1b2c3d4e5f67890",
  "line": "[2025-05-01 12:00:00] INFO: Module FileSrc initialized\n"
}
```

### `finished`

Сервер отправляет после завершения вычислительного процесса (когда `computing_server` завершился). Содержит список файлов, созданных в директории результатов (`returnDirectory`).

**Сервер → Клиент:**
```json
{
  "session_id": "session_a1b2c3d4e5f67890",
  "status": "finished",
  "files": [
    {
      "name": "output.bin",
      "size": 1048576
    },
    {
      "name": "report.txt",
      "size": 256
    }
  ]
}
```

- `files` — массив объектов с полями `name` (имя файла) и `size` (размер в байтах).
- Если директория результатов пуста или недоступна, `files` будет пустым массивом `[]`.

---

## Замечания и ограничения

- **Нет endpoint'а остановки:** запущенный `computing_server` работает до самостоятельного завершения. Управление процессом (kill / graceful shutdown) не реализовано.
- **Нет endpoint'а статуса сессии:** после запуска клиент не может запросить состояние запущенного процесса через HTTP API.
- **Проверка существования `computing_server`:** перед `subprocess.Popen` не выполняется проверка наличия исполняемого файла по пути `COMPUTING_SERVER_PATH`.
- **Абсолютные пути:** пути к `variables.toml` и `pipeline.json` внутри сессии преобразуются в абсолютные перед записью в JSON и передачей вычислительному серверу.
