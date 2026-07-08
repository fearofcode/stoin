# ProCAT Flash Writer Realtime Setup

This is the known-good setup used to run a ProCAT Flash Writer as a realtime steno input device for stoin on macOS.

## Working Hardware Chain

- ProCAT Flash Writer machine.
- RS-232 to USB adapter with FTDI chipset, tested with the OIKWAN adapter:
  <https://www.amazon.com/dp/B0759HSLP1?th=1>
- Cheap USB-A to USB-C adapter into the Mac.

Two RJ11-to-RS-232 paths have worked:

- Official/proprietary FlashWriter realtime cable and plug set from StenoDoctor:
  <https://stenodoctor.com/shop/ols/products/flashwriter-realtime-cable-and-plug>
- Cheaper tested path: generic RJ11 cable plus Tripp Lite `P450-000` null modem serial RS-232 modular adapter kit:
  <https://www.amazon.com/dp/B08YS39DGC>
  <https://www.amazon.com/dp/B0029L0V48>

The generic RJ11 plus Tripp Lite setup is not an officially supported option; it is only documented here because it worked on the tested hardware. The Tripp Lite null modem modular adapter appears to be the key part of that cheaper setup.

Both tested RS-232-to-USB adapters worked once the machine baud rate and FTDI driver were correct. The confirmed FTDI adapter currently appears as:

```text
/dev/cu.usbserial-A9MYL0OX
USB Product Name: FT232R USB UART
USB Vendor Name: FTDI
USB Serial Number: A9MYL0OX
idVendor: 1027 / 0x0403
idProduct: 24577 / 0x6001
```

## Machine Settings

Confirmed working settings:

- `REAL/TIME`: `Baron`
- Baud rate: `9600`
- Protocol in stoin: TX Bolt

The app defaults to TX Bolt at 9600 baud and auto-detects `/dev/cu.usbserial*`, so this works with the adapter connected:

```sh
./build/stoin
```

The explicit equivalent is:

```sh
./build/stoin --input tx-bolt --serial-port /dev/cu.usbserial-A9MYL0OX --serial-baud 9600
```

## Mac Setup

Confirmed machine:

- M1 Max MacBook Pro
- macOS 26.5.1
- Active FTDI DriverKit extension:

```text
com.ftdi.vcp.dext (1.6/0)
```

Install the current FTDI VCP driver from:

<https://ftdichip.com/drivers/vcp-drivers/>

The version installed for this setup was `1.6.0`, the current ARM Mac driver at the time of testing. The setup was not tested without the FTDI driver installed. FTDI notes that the DEXT installer should be run from `/Applications`, and that old `FTDIUSBSerialVCPDextInstaller` entries should be removed from `/Applications` before installing the new driver.

Approve or manage the driver in:

```text
System Settings -> General -> Login Items & Extensions -> Driver Extensions
```

Useful checks:

```sh
ls -1 /dev/cu.*
systemextensionsctl list
ioreg -p IOUSB -w0 -l | rg -i 'FT232R|FTDI|usbserial|USB Product Name|USB Serial Number'
```

## Diagnostics

Raw serial dump:

```sh
./build/stoin --raw-serial --serial-port /dev/cu.usbserial-A9MYL0OX --serial-baud 9600
```

Healthy TX Bolt output for single keys should vary by key. Example captures from pressing keys in steno order:

```text
01 00
02 00
04 00
08 00
10 00
20 00
41 00
42 00
44 00
48 00
50 00
60 00
81 00
82 00
84 00
88 00
90 00
A0 00
C1 00
C2 00
C4 00
C8 00
```

Some multi-key chords produce multiple nonzero TX Bolt bytes, for example:

```text
15 41 00
```

If raw mode prints repeated `FF` bytes for different keys, the app is receiving bytes but the serial setup is wrong. In testing, that symptom went away after using the FTDI adapter/driver path and setting both the machine and app to 9600 baud.

## Known Bad Paths

These did not work:

- Generic RJ11 to USB adapter.
- Generic RJ11 to RS-232 cable by itself.

The working path is not "RJ11 directly to USB"; it is RJ11 into a real RS-232 path, then RS-232 to USB with a working FTDI VCP driver. That RJ11-to-RS-232 segment can be the StenoDoctor realtime cable/plug set or the tested generic RJ11 plus Tripp Lite null modem modular adapter setup above.
