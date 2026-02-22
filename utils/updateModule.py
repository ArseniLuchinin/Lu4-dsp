#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os
import sys
from pathlib import Path

DEFAULT_MODULES_DIR = "/home/luchinin/my_source/Course_poject/Server/build/Modules"


def module_name_from_so(so_path: Path) -> str:
    name = so_path.stem
    if name.startswith("lib"):
        name = name[3:]
    return name


def collect_shared_libs(modules_dir: Path) -> list[Path]:
    return sorted(p.resolve() for p in modules_dir.rglob("*.so") if p.is_file())


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Обход папки модулей и загрузка записей <имя_модуля>:<путь_к_so> в Redis."
    )
    parser.add_argument(
        "--modules-dir",
        default=DEFAULT_MODULES_DIR,
        help=f"Папка с собранными модулями (по умолчанию: {DEFAULT_MODULES_DIR})",
    )
    parser.add_argument("--host", default="127.0.0.1", help="Redis host (по умолчанию: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=6379, help="Redis port (по умолчанию: 6379)")
    parser.add_argument("--db", type=int, default=0, help="Redis DB (по умолчанию: 0)")
    parser.add_argument(
        "--username",
        default=os.getenv("REDISCLI_USER"),
        help="Имя пользователя Redis ACL (по умолчанию из REDISCLI_USER)",
    )
    parser.add_argument(
        "--password",
        default=os.getenv("REDISCLI_AUTH"),
        help="Пароль Redis (по умолчанию из REDISCLI_AUTH)",
    )
    parser.add_argument(
        "--prefix",
        default="",
        help="Префикс ключей (например modules:). По умолчанию без префикса.",
    )
    args = parser.parse_args()

    modules_dir = Path(args.modules_dir)
    if not modules_dir.exists() or not modules_dir.is_dir():
        print(f"Ошибка: папка не найдена или не является директорией: {modules_dir}")
        return 1

    so_files = collect_shared_libs(modules_dir)
    if not so_files:
        print(f"В папке {modules_dir} не найдено .so файлов")
        return 1

    try:
        import redis
    except ImportError:
        print("Ошибка: не установлен пакет redis. Установите его: pip install redis")
        return 1

    client = redis.Redis(
        host=args.host,
        port=args.port,
        db=args.db,
        username=args.username,
        password=args.password,
        decode_responses=True,
    )

    try:
        client.ping()
    except redis.RedisError as exc:
        if not args.password:
            print("Подсказка: пароль не задан. Установите REDISCLI_AUTH или передайте --password.")
        elif args.username is None:
            print("Подсказка: если у Redis включен ACL, передайте --username или установите REDISCLI_USER.")
        print(f"Ошибка подключения к Redis: {exc}")
        return 1

    written = 0
    for so_path in so_files:
        module_name = module_name_from_so(so_path)
        key = f"{args.prefix}{module_name}"
        value = str(so_path)
        try:
            client.set(key, value)
            written += 1
            print(f"{key} : {value}")
        except redis.RedisError as exc:
            print(f"Ошибка записи ключа {key}: {exc}")

    print(f"Готово. Записано {written} ключ(ей) в Redis.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
