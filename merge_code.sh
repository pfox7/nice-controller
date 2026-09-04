#!/bin/bash
# Скрипт для объединения файлов .html, .h, .cpp, .ino в один текстовый файл с метками

# Использование: ./merge_code.sh [директория] [выходной_файл]
# По умолчанию: текущая директория, combined.txt

set -e  # прервать выполнение при ошибке

SEARCH_DIR="${1:-.}"
OUTPUT_FILE="${2:-combined.txt}"

# Преобразуем выходной файл в абсолютный путь, чтобы сохранить его после смены директории
if [[ "$OUTPUT_FILE" != /* ]]; then
    OUTPUT_FILE="$(pwd)/$OUTPUT_FILE"
fi

# Проверяем существование директории поиска
if [ ! -d "$SEARCH_DIR" ]; then
    echo "Ошибка: директория '$SEARCH_DIR' не найдена." >&2
    exit 1
fi

# Переходим в директорию поиска
cd "$SEARCH_DIR"

# Очищаем (или создаём) выходной файл
> "$OUTPUT_FILE" || { echo "Ошибка: не могу записать в '$OUTPUT_FILE'." >&2; exit 1; }

# Рекурсивный поиск и объединение
find . -type f \( -name "*.html" -o -name "*.h" -o -name "*.cpp" -o -name "*.ino" \) -print0 | while IFS= read -r -d '' file; do
    # Относительный путь без начального "./"
    rel_path="${file#./}"
    echo "=== $rel_path ===" >> "$OUTPUT_FILE"
    cat "$file" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"   # добавляем пустую строку между файлами
done

echo "Объединение завершено. Выходной файл: $OUTPUT_FILE"