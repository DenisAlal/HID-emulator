# HID Emulator

Замена глючному BLE-HID у ESP32 CYD: плата или телефон шлют короткий HTTP-запрос
в локальную сеть, а сервер на ПК сам «давит» комбинацию клавиш через `SendInput`.
Soundpad (или любая программа с глобальными хоткеями) ловит нажатие как обычно.
Никакого Bluetooth-pairing — переподключаться нечему.

```
[ESP32 CYD] ──WiFi──┐
                    ├──► [server (Go) на ПК] ──SendInput──► Windows ──► Soundpad
[Android (Kotlin)] ─┘
```

## Структура проекта

- `server/` — HTTP-сервер на Go (инжект клавиш + API + трей).
- `android/` — приложение на Kotlin (в планах).
- `esp32/` — прошивка ESP32 CYD (в планах).

## Сервер

Требуется [Go](https://go.dev/dl/) 1.22+ (64-битный). Первая сборка скачает
зависимость `fyne.io/systray` (нужен интернет).

```
cd server
build.bat        # собирает оба бинарника
```

Сборка даёт два файла (один и тот же исходник):

| Файл | Режим | Когда использовать |
|------|-------|--------------------|
| `server.exe` | консоль | отладка: логи в консоль + файл, `Ctrl+C` — стоп |
| `server-tray.exe` | тихий, в трее | повседневный запуск: без окна консоли, иконка в трее |

Запуск:

- **Трей (рекомендуется для постоянной работы):** двойной клик `start-tray.bat`
  или `server-tray.exe`. В трее появится иконка → ПКМ: открыть веб-интерфейс,
  открыть лог, выход.
- **Консоль (для отладки):** `server.exe` в терминале.

## Логирование

Всё пишется в `server.log` (рядом с exe, дописывается):

- запуск/останов сервера;
- каждый запрос `/press` (комбинация + IP клиента);
- список кнопок по запросу `/keys`;
- неавторизованные запросы и ошибки.

В консольном режиме те же логи дублируются в терминал. Файл можно удалять —
при следующем старте создастся заново.

## Настройка (config.json)

```json
{
  "host": "0.0.0.0",
  "port": 45678,
  "token": "change-me-to-a-secret",
  "numlock": true,
  "keys": {
    "sound1": { "label": "Sound 1", "combo": "ctrl+alt+f1" },
    "stop":   { "label": "Stop",   "combo": "ctrl+alt+f4" }
  }
}
```

- `token` — общий ключ доступа. Пустая строка (`""`) отключает проверку.
- `numlock` — включать NumLock перед отправкой комбинации с numpad.
  Нужно, если на клавиатуре нет клавиши NumLock: иначе numpad работает как
  стрелки/Home/End.
- `keys` — `id -> { label, combo }`. Меняй свободно, без пересборки.
- `order` (необязательно) — позиция кнопки в `/keys`, меньше = выше.
  Без него порядок естественный: `sound2` идёт раньше `sound10`.

После правки `config.json` сервер нужно перезапустить — конфиг читается один раз
при старте.

## Формат комбинаций

`combo` — модификаторы + одна основная клавиша через `+`:

```
ctrl+alt+f1        shift+win+s       ctrl+shift+enter
f13                media_next         volume_up
```

Модификаторы: `ctrl`, `alt`, `shift`, `win`. Основные клавиши: буквы `a-z`,
цифры `0-9`, `f1-f24`, `enter`, `space`, `tab`, `esc`, стрелки, `home`, `end`,
`pageup`, `pagedown`, `insert`, `delete`, `printscreen`, медиа-клавиши
(`volume_up/down/mute`, `media_next/prev/play_pause/stop`), символы пунктуации.
Список расширяется в `main.go` (функция `buildVkMap`).

### Псевдонимы `num10`…`num29`

| Псевдоним | Реальная комбинация |
|-----------|---------------------|
| `num10`…`num19` | `numpad0`…`numpad9` |
| `num20`…`num29` | `ctrl+numpad0`…`ctrl+numpad9` |

То есть `"combo": "num11"` — это `Num1`. Numpad-клавиши требуют включённого
NumLock. При `"numlock": true` сервер сам проверяет состояние и включает
NumLock перед отправкой такой комбинации (полезно, если клавиши NumLock на
клавиатуре нет). В Soundpad этому звуку нужно назначить ровно такую же
комбинацию; вручную её надо нажать только один раз — при назначении хоткея.

## Импорт списка из Soundpad

Soundpad умеет сохранять список звуков в `.spl` — это обычный XML:

```xml
<Soundlist>
  <Sound title="ba dum tss" url="...\ba dum tss.mp3" .../>
  ...
</Soundlist>
```

Скрипт `gen-config.ps1` читает такой файл и собирает `config.json`:
первая кнопка — `Stop` на `numpad0`, дальше звуки по порядку на `numpad1`,
`numpad2`, … (после 9 звуков — `ctrl+numpad0`, `ctrl+numpad1`, …).
Имена кнопок берутся из `title`.

```
cd server
powershell -ExecutionPolicy Bypass -File gen-config.ps1
```

По умолчанию берётся живой список `%APPDATA%\Leppsoft\soundlist.spl`.
Другой файл:

```
powershell -ExecutionPolicy Bypass -File gen-config.ps1 -SplPath "E:\...\backup.spl"
```

`host`, `port` и `token` из существующего `config.json` сохраняются. После
генерации перезапусти сервер.

## API

Авторизация: заголовок `X-Key: <token>` (или `?key=<token>`).

| Метод | Путь | Что делает |
|-------|------|------------|
| `GET /keys` | список кнопок `{id,label,combo}` |
| `POST /press` | нажать. Тело JSON `{"id":"sound1"}` или `{"combo":"ctrl+alt+f1"}` |
| `POST /press?id=sound1` | нажать по id |
| `POST /press?combo=ctrl%2Balt%2Bf1` | нажать произвольную комбинацию |
| `GET /` | тестовая веб-страница |

> В query-строке `+` кодируется как `%2B`. Проще использовать `id` или JSON-тело.

## Проверка

- `server.exe --test` — самопроверка инжекта (F13, `sizeof(INPUT)=40`).
- Postman: импортируй `Soundpad-Remote.postman_collection.json`.

## Брандмауэр Windows

Разреши входящие на порт (первый запуск Windows обычно спрашивает сам). Либо
вручную (от админа):

```
netsh advfirewall firewall add rule name="HID Emulator" dir=in action=allow protocol=TCP localport=45678
```

## Автозапуск

Чтобы сервер стартовал вместе с Windows в фоне: положи ярлык `server-tray.exe`
(или `start-tray.bat`) в папку автозагрузки (`Win+R` → `shell:startup`). Трей-
версия не открывает окно консоли, так что при закрытии консоли/окна сервер не
остановится — выключается только из меню трея («Выход»).
