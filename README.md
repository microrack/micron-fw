# micron-fw

Instructions for flashing the device and using helper scripts.

## Requirements

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (`pio`)
- `ping` (usually already available in the system)
- `nc` / `netcat` for reading logs over TCP

Check:

```bash
pio --version
nc -h
```

## PlatformIO Configurations

The following environments are configured in `platformio.ini`:

- `usb` - flash over USB
- `ota` - flash over OTA (`upload_protocol = espota`)

## Flashing Over USB

1. Connect the device over USB.
2. Verify that PlatformIO can see the port:

```bash
pio device list
```

3. Upload the firmware:

```bash
pio run -e usb -t upload
```

## Flashing Over OTA

If the device is reachable on the network (known IP), you can flash without USB:

```bash
pio run -e ota -t upload --upload-port <ip-address>
```

Example:

```bash
pio run -e ota -t upload --upload-port 192.168.1.42
```

## Scripts

### `start_log.sh`

Connects to the device over TCP (via `nc`) and shows logs.

Usage:

```bash
./start_log.sh <ip-or-hostname> [port]
```

- `<ip-or-hostname>` - device address
- `[port]` - log port (default: `2323`)

What the script does:

1. Waits until the device starts replying to `ping`
2. Then connects to the log port via `nc`

Examples:

```bash
./start_log.sh 192.168.1.42
./start_log.sh micron.local 2323
```

### `upload_ota_and_log.sh`

Flashes the device over OTA and immediately opens logs.

Usage:

```bash
./upload_ota_and_log.sh <ip-address>
```

What the script does:

1. Starts OTA flashing:
   `pio run -e ota -t upload --upload-port "<ip-address>"`
2. If flashing succeeds, runs:
   `./start_log.sh "<ip-address>"`

Example:

```bash
./upload_ota_and_log.sh 192.168.1.42
```

## Quick Workflow

Typical Wi-Fi development loop:

```bash
./upload_ota_and_log.sh 192.168.1.42
```

If OTA is unavailable, flash over USB:

```bash
pio run -e usb -t upload
```

## Common Issues

- `pio: command not found`  
  PlatformIO is not installed or not added to `PATH`.

- `nc: command not found`  
  Install netcat (`nc`).

- OTA upload fails  
  Check that the device and computer are on the same network and the IP is correct.

- Log does not open on `2323`  
  Verify the port and pass it as the second argument to `start_log.sh`.
