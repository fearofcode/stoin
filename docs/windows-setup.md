# Windows setup

The Windows port uses Win32 `SendInput` for keyboard output, a low-level keyboard hook for qwerty capture, and normal COM ports for serial steno machines.

## Build

Install MSYS2, open a **MinGW64** shell, then install the C toolchain:

```sh
pacman -S --needed mingw-w64-x86_64-gcc make
```

From the repo:

```sh
make PLATFORM=windows
make PLATFORM=windows test
```

The binaries are written under `build/windows/`, for example:

```sh
./build/windows/stoin.exe --input stentura
```

## Serial devices

Stoin scans `COM1` through `COM256` and opens Windows serial paths such as `\\.\COM3`. You can let it auto-scan or pass a port explicitly:

```sh
./build/windows/stoin.exe --input stentura --serial-port COM3
```

The same serial defaults are used as on macOS and Linux: 9600 baud, 8 data bits, no parity, 1 stop bit.

## Qwerty keyboard capture

Qwerty mode uses a low-level keyboard hook:

```sh
./build/windows/stoin.exe --input qwerty
```

Windows may block simulated input into elevated applications when Stoin itself is not elevated. If translations work in normal apps but not in an administrator window, run Stoin with matching privileges.

## Pedals

USB pedal registration is not implemented on Windows yet. Serial machine input and qwerty input do not require pedal support.
