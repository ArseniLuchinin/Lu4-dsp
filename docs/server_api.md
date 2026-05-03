# HTTP API управляющего сервера

Документация к `server.py` — управляющему Flask-серверу, который предоставляет REST API для клиентского приложения и запускает вычислительный бэкенд (`computing_server`).

> **Базовый URL:** `http://<host>:<SERVER_PORT>` (порт задаётся переменной окружения `SERVER_PORT`, по умолчанию `5000`).

---

## Общие сведения

- Формат обмена: **JSON**.
- Все ответы содержат поле `status`: `"ok"` или `"error"`.
- При ошибках возвращается соответствующий HTTP-код (`4xx`, `5xx`) и поле `message` с описанием.
- Для работы эндпоинтов `/modules/list` и `/modules/<name>` требуется доступ к Redis (см. переменные окружения `REDIS_HOST`, `REDIS_PORT`, `REDISCLI_AUTH`).
- Сервер поддерживает **Socket.IO** для real-time передачи логов запущенных расчётов.

### Переменные окружения

| Переменная | Назначение | Значение по умолчанию |
|---|---|---|
| `SERVER_PORT` | Порт HTTP-/Socket.IO-сервера | `5000` |
| `FLASK_SECRET_KEY` | Секретный ключ Flask (используется Socket.IO) | `dev-secret-key` |
| `COMPUTING_SERVER_PATH` | Путь к исполняемому файлу `computing_server` | `./build/computing_server` |
| `SESSIONS_DIR` | Директория для хранения сессий | `./sessions` |
| `REDIS_HOST` | Хост Redis | `127.0.0.1` |
| `REDIS_PORT` | Порт Redis | `6379` |
| `REDISCLI_AUTH` | Пароль Redis (если требуется) | `None` |
| `MODULES_DIR` | Корневая директория с модулями | `Modules` |

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

Возвращает список всех модулей, зарегистрированных в Redis.

**Логика работы:**
1. Проверяет соединение с Redis (`PING`).
2. Запрашивает все ключи вида `*-module`.
3. Для каждого ключа проверяет наличие `module.json` и `README.md` в поддиректории `Modules/<module_name>/`.

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

**Возможные ошибки:**
- `503 Service Unavailable` — не удалось подключиться к Redis или ошибка аутентификации.

---

### `GET /modules/<name>`

Возвращает детальную информацию о конкретном модуле.

**Параметры URL:**
- `name` — имя модуля (как в Redis, без суффикса `-module`).

**Логика работы:**
1. Проверяет соединение с Redis.
2. Проверяет существование ключа `<name>-module` в Redis.
3. Читает и парсит `Modules/<name>/module.json`.
4. Читает `Modules/<name>/README.md` (если есть).

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
- `503 Service Unavailable` — проблема с подключением к Redis.
- `404 Not Found` — модуль не найден в Redis.
- `500 Internal Server Error` — ошибка чтения или парсинга `module.json` / `README.md`.

---

### `POST /start`

Запускает вычислительный сервер (`computing_server`) с заданным конвейером и переменными.

**Тело запроса (JSON):**

| Поле | Тип | Обязательное | Описание |
|---|---|---|---|
| `pipeline` | `object` или `string` | Да | Конфигурация конвейера (JSON-объект или JSON-строка). |
| `variables` | `string` | Да | Содержимое файла `variables.toml` (сырой текст). |

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
2. Создаёт директорию сессии: `./sessions/session_YYYYMMDD/`.
3. Записывает `variables` в файл `variables.toml` внутри директории сессии.
4. Если `pipeline` передан как строка — парсит её в JSON-объект.
5. Дополняет JSON-объект конвейера полем `variables` с абсолютным путём к сохранённому `variables.toml`.
6. Записывает итоговый JSON в файл `pipeline.json` внутри директории сессии.
7. Запускает `./build/computing_server <путь_k_pipeline.json>` в фоновом процессе (`subprocess.Popen`).
8. STDOUT и STDERR вычислительного сервера транслируются в реальном времени через Socket.IO в **room** с именем `session_id`, а также сохраняются в лог-файл `logs_YYYYMMDD_HHMMSS.txt`.

**Ответ (`200 OK`):**
```json
{
  "status": "ok",
  "session_id": "session_20260501",
  "pid": 12345
}
```

**Возможные ошибки:**
- `400 Bad Request` — отсутствуют обязательные поля `pipeline` / `variables`, или `pipeline` содержит невалидный JSON (если передан как строка).
- `500 Internal Server Error` — ошибка создания директории сессии, записи файлов или запуска вычислительного сервера.

---

## Socket.IO события

Сервер использует `flask-socketio`. Базовый URL тот же, что и для HTTP (`ws://<host>:<SERVER_PORT>`).

### `join`

Подписывает клиента на логи указанной сессии.

**Клиент → Сервер:**
```json
{
  "session_id": "session_20260501"
}
```

**Сервер → Клиент (`joined`):**
```json
{
  "session_id": "session_20260501"
}
```

### `log`

Сервер рассылает строки stdout/stderr вычислительного процесса всем клиентам, подписанным на room `session_id`.

**Сервер → Клиент:**
```json
{
  "session_id": "session_20260501",
  "line": "[2025-05-01 12:00:00] INFO: Module FileSrc initialized\n"
}
```

---

## Замечания и ограничения

- **Нет endpoint'а остановки:** запущенный `computing_server` работает до самостоятельного завершения. Управление процессом (kill / graceful shutdown) не реализовано.
- **Нет endpoint'а статуса сессии:** после запуска клиент не может запросить состояние запущенного процесса через HTTP API.
- **Проверка существования `computing_server`:** перед `subprocess.Popen` не выполняется проверка наличия исполняемого файла по пути `COMPUTING_SERVER_PATH`.
- **Абсолютные пути:** пути к `variables.toml` и `pipeline.json` внутри сессии преобразуются в абсолютные перед записью в JSON и передачей вычислительному серверу.
