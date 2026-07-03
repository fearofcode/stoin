# Windows setup

The Windows port uses Win32 `SendInput` for keyboard output, a low-level keyboard hook for qwerty capture, and normal COM ports for serial steno machines.

## Build With MSVC

Open a **Visual Studio Developer Command Prompt** so `cl.exe` is on `PATH`.

From the repo:

```bat
build.bat
build.bat test
build.bat release
```

The binaries are written under `build\windows\`, for example:

```bat
build\windows\stoin.exe --input stentura
```

`build.bat test` runs the C test binary. If `go.exe` is available, it also runs the Go tests.

## Build With Make

```sh
make PLATFORM=windows
make PLATFORM=windows test
```

This path is intended for a make-capable Windows shell. `build.bat` is the simpler option for a plain MSVC setup.

## Serial devices

Stoin scans `COM1` through `COM256` and opens Windows serial paths such as `\\.\COM3`. When Windows exposes port metadata, Stoin prefers ports whose friendly name, description, or manufacturer look like a USB serial adapter, such as `USB Serial Port` or `FTDI`. You can let it auto-scan or pass a port explicitly:

```sh
build\windows\stoin.exe --input stentura --serial-port COM3
```

The same serial defaults are used as on macOS and Linux: 9600 baud, 8 data bits, no parity, 1 stop bit.

## Qwerty keyboard capture

Qwerty mode uses a low-level keyboard hook:

```sh
build\windows\stoin.exe --input qwerty
```

Windows may block simulated input into elevated applications when Stoin itself is not elevated. If translations work in normal apps but not in an administrator window, run Stoin with matching privileges.

## USB Pedals

Windows pedal support uses Raw Input for USB pedals. Keyboard-style pedals are learned from their key event. Some non-keyboard HID pedals, such as vendor/button devices, are learned from the first nonzero raw HID report they send during registration.

To register a pedal:

```bat
build\windows\stoin.exe --register-pedal initial-verb
```

To register the final-verb pedal:

```bat
build\windows\stoin.exe --register-pedal final-verb
```

The old spelling still works as a compatibility alias:

```bat
build\windows\stoin.exe --register-pedal core
```

To register the non-verb pedal:

```bat
build\windows\stoin.exe --register-pedal nonverb
```

Press the pedal when prompted. Stoin saves the Raw Input device binding to `stoin-pedals.json`. Later runs load that binding automatically:

```bat
build\windows\stoin.exe --input stentura
```

If a programmable pedal has multiple buttons, register the specific button you want to use and avoid pressing the other buttons until registration finishes.
