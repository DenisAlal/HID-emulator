#pragma once
#include <Arduino.h>

// Одна кнопка (приходит с сервера через GET /keys).
struct KeyDef {
    String id;     // идентификатор для POST /press?id=...
    String label;  // название
    String combo;  // комбинация (справочно, сама плата её не шлёт)
};

// Загружает список кнопок с сервера в массив out.
// Возвращает true при успехе, заполняет count.
bool netFetchKeys(KeyDef* out, int maxCount, int& count);

// Отправляет нажатие кнопки по id. Возвращает true при успехе.
bool netPressKey(const String& id);
