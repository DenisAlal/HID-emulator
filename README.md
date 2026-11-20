# HID Emulator

Локальный пульт для Soundpad: ESP32 CYD и Android-приложение шлют HTTP-запросы
на ПК, а сервер на Go нажимает заданные клавиши через `SendInput`. Soundpad
ловит их как обычные глобальные хоткеи — Bluetooth не нужен.

```
[ESP32 CYD] ──Wi-Fi──┐
                     ├──► [server (Go) на ПК] ──SendInput──► Windows ──► Soundpad
[Android] ───────────┘
```

## Что внутри

| Папка | Что это |
|-------|---------|
| `server/` | Go-сервер: `GET /keys`, `POST /press`, трей, `config.json` с кнопками |
| `esp32/` | Прошивка ESP32-2432S028 (CYD): LVGL, тач, Wi-Fi, настройки на устройстве |
| `android/` | Android-приложение на Kotlin + Compose: Home и Settings |

## Как работает

Клавиши нажимает только сервер. В `server/config.json` для каждой кнопки
заданы `label` и `combo` (например, `numpad1`). ESP32 и Android запрашивают
список кнопок через `GET /keys` и отправляют id нажатой через `POST /press`.

## Быстрый старт

1. **Сервер:** `cd server`, `build.bat`, запустить `server-tray.exe`.
2. **Кнопки:** отредактировать `server/config.json` и перезапустить сервер.
3. **ESP32:** `cd esp32`, `pio run -t upload`.
4. **Android:** установить `android/app/build/outputs/apk/debug/app-debug.apk`.
5. В настройках ESP32 или Android указать IP ПК, порт и токен.

Подробности — в `server/README.md` и `esp32/README.md`.

## Требования

- Windows (сервер использует `SendInput`).
- Go 1.22+ для сборки сервера.
- PlatformIO для прошивки ESP32.
- Android Studio / JDK для сборки APK.

## Безопасность

В репозитории лежат только заглушки (`SSID`, `PASS`, `secret`). Реальные
Wi-Fi-данные и токен задавай на устройстве (NVS на ESP32 / настройки Android)
и не коммить их в `config.h` или `config.json`.
