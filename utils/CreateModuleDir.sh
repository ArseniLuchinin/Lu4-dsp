#!/bin/bash

# Скрипт для создания структуры проекта с папками src, include и копированием CMakeLists.txt

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Функция для вывода справки
usage() {
    echo "Использование: $0 [путь_к_директории]"
    echo "  Если путь не указан, используется текущая директория"
    exit 1
}

# Функция для проверки существования директории
check_directory() {
    if [ ! -d "$1" ]; then
        echo -e "${RED}Ошибка: Директория $1 не существует${NC}"
        exit 1
    fi
}

# Получаем путь назначения из первого аргумента или используем текущую директорию
TARGET_DIR="${1:-.}"

# Проверка на запрос справки
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    usage
fi

# Проверка существования целевой директории
check_directory "$TARGET_DIR"

echo -e "${GREEN}Начинаем создание структуры проекта в директории: $TARGET_DIR${NC}"

# Создание папки src
if [ ! -d "$TARGET_DIR/src" ]; then
    mkdir -p "$TARGET_DIR/src"
    echo -e "${GREEN}✓${NC} Создана папка: src"
else
    echo -e "${YELLOW}!${NC} Папка src уже существует"
fi

# Создание папки include
if [ ! -d "$TARGET_DIR/include" ]; then
    mkdir -p "$TARGET_DIR/include"
    echo -e "${GREEN}✓${NC} Создана папка: include"
else
    echo -e "${YELLOW}!${NC} Папка include уже существует"
fi
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Здесь вы можете указать свой путь к CMakeLists.txt
CMAKE_SOURCE="${SCRIPT_DIR}/CMakeLists.template"

# Проверка существования файла CMakeLists.txt
if [ -f "$CMAKE_SOURCE" ]; then
    # Получаем имя файла из пути
    CMAKE_FILENAME=$(basename "$CMAKE_SOURCE")
    
    # Копирование CMakeLists.txt
    if [ -f "$TARGET_DIR/$CMAKE_FILENAME" ]; then
        echo -e "${YELLOW}!${NC} Файл $CMAKE_FILENAME уже существует в целевой директории"
        
        # Спрашиваем, перезаписать ли файл
        echo -n "Хотите перезаписать его? (y/n): "
        read -r answer
        if [[ "$answer" =~ ^[Yy]$ ]]; then
            cp "$CMAKE_SOURCE" "$TARGET_DIR/CMakeLists.txt"
            echo -e "${GREEN}✓${NC} Файл $CMAKE_FILENAME перезаписан"
        else
            echo -e "${YELLOW}!${NC} Файл $CMAKE_FILENAME не был перезаписан"
        fi
    else
        cp "$CMAKE_SOURCE" "$TARGET_DIR/CMakeLists.txt"
        echo -e "${GREEN}✓${NC} Файл $CMAKE_FILENAME скопирован в $TARGET_DIR/"
    fi
else
    echo -e "${RED}Ошибка: Файл CMakeLists.txt не найден по пути: $CMAKE_SOURCE${NC}"
    echo -e "${YELLON}Пожалуйста, укажите правильный путь к файлу CMakeLists.txt в скрипте${NC}"
    echo -e "${YELLOW}Строка для изменения: CMAKE_SOURCE=\"/путь/к/вашему/CMakeLists.txt\"${NC}"
fi

echo -e "${GREEN}Готово!${NC}"
echo -e "\nСтруктура проекта в $TARGET_DIR:"
if command -v tree &> /dev/null; then
    tree "$TARGET_DIR" -L 1
else
    ls -la "$TARGET_DIR"
fi