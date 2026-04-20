# micron-fw

Инструкция по прошивке устройства и использованию вспомогательных скриптов.

## Что нужно установить

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (`pio`)
- `ping` (обычно уже есть в системе)
- `nc` / `netcat` для чтения логов по TCP

Проверка:

```bash
pio --version
nc -h
```

## Конфигурации PlatformIO

В `platformio.ini` настроены окружения:

- `usb` — прошивка по USB
- `ota` — прошивка по OTA (`upload_protocol = espota`)

## Прошивка по USB

1. Подключи устройство по USB.
2. Проверь, что PlatformIO видит порт:

```bash
pio device list
```

3. Залей прошивку:

```bash
pio run -e usb -t upload
```

## Прошивка по OTA

Если устройство доступно по сети (IP известен), можно прошить без USB:

```bash
pio run -e ota -t upload --upload-port <ip-адрес>
```

Пример:

```bash
pio run -e ota -t upload --upload-port 192.168.1.42
```

## Скрипты

### `start_log.sh`

Подключается к устройству по TCP (через `nc`) и показывает лог.

Использование:

```bash
./start_log.sh <ip-or-hostname> [port]
```

- `<ip-or-hostname>` — адрес устройства
- `[port]` — порт логов (по умолчанию `2323`)

Что делает скрипт:

1. Ждёт, пока устройство начнёт отвечать на `ping`
2. После этого подключается к порту логов через `nc`

Примеры:

```bash
./start_log.sh 192.168.1.42
./start_log.sh micron.local 2323
```

### `upload_ota_and_log.sh`

Прошивает устройство по OTA и сразу открывает лог.

Использование:

```bash
./upload_ota_and_log.sh <ip-address>
```

Что делает скрипт:

1. Запускает OTA-прошивку:
   `pio run -e ota -t upload --upload-port "<ip-address>"`
2. Если прошивка успешна, запускает:
   `./start_log.sh "<ip-address>"`

Пример:

```bash
./upload_ota_and_log.sh 192.168.1.42
```

## Быстрый сценарий

Обычный цикл разработки по Wi-Fi:

```bash
./upload_ota_and_log.sh 192.168.1.42
```

Если OTA недоступна, прошей по USB:

```bash
pio run -e usb -t upload
```

## Типичные проблемы

- `pio: command not found`  
  PlatformIO не установлен или не добавлен в `PATH`.

- `nc: command not found`  
  Установи netcat (`nc`).

- OTA не проходит  
  Проверь, что устройство и компьютер в одной сети и IP указан верно.

- Лог не открывается на `2323`  
  Уточни порт и передай его вторым аргументом в `start_log.sh`.
