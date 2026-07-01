# Linux setup

The Linux port uses normal POSIX serial devices for steno machines and `/dev/uinput` for keyboard output.

## Serial devices

Most USB serial devices are owned by a system group such as `dialout` or `uucp`.

```sh
DEVICE=/dev/serial/by-id/usb-your-device
ls -l "$(readlink -f "$DEVICE")"
```

If the device group is `dialout`, add your user to it:

```sh
sudo usermod -aG dialout "$USER"
```

Use the group shown by `ls -l` if it is not `dialout`. Log out and back in, or reboot, for the new group membership to apply.

## Keyboard output

Stoin needs write access to `/dev/uinput` to emit translations without `sudo`. One udev rule approach is:

```sh
sudo groupadd -f stoin
sudo usermod -aG stoin "$USER"
printf 'KERNEL=="uinput", GROUP="stoin", MODE="0660", OPTIONS+="static_node=uinput"\n' \
  | sudo tee /etc/udev/rules.d/99-stoin-uinput.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Then log out and back in, or reboot. To temporarily grant access before group membership refreshes:

```sh
sudo chown "$USER":stoin /dev/uinput
sudo chmod 660 /dev/uinput
```

TX Bolt and Gemini PR input only need serial access plus `/dev/uinput` output access.

## USB Pedals

Linux pedal support uses evdev input devices under `/dev/input/event*`. To register a pedal:

```sh
./build/linux/stoin --register-pedal core
```

or:

```sh
./build/linux/stoin --register-pedal nonverb
```

Press the pedal when prompted. Stoin saves the evdev binding to `stoin-pedals.json`. Later runs load that binding automatically.

Pedal registration needs read access to the relevant input event device. Runtime use tries to grab the registered pedal device so its key/button event does not also leak into other applications. The qwerty capture udev rule below is also suitable for many pedal devices; if your pedal is not tagged as a keyboard, use a broader trusted-device rule or your distribution's `input` group.

## Qwerty keyboard capture

The `--input qwerty` mode also needs read/grab access to physical keyboard event devices under `/dev/input/event*`. This is separate from `/dev/uinput`.

Warning: access to keyboard event devices lets a process read raw keyboard input. Only grant this to a group whose members you trust.

One udev rule approach using the same `stoin` group is:

```sh
sudo groupadd -f stoin
sudo usermod -aG stoin "$USER"
printf 'KERNEL=="event*", SUBSYSTEM=="input", ENV{ID_INPUT_KEYBOARD}=="1", GROUP="stoin", MODE="0660"\n' \
  | sudo tee /etc/udev/rules.d/99-stoin-input.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=input
```

Then log out and back in, or reboot. To inspect current permissions:

```sh
ls -l /dev/input/event*
```

Some distributions already use an `input` group for these devices. If your keyboard event devices are group-owned by `input`, adding your user to that group can be enough:

```sh
sudo usermod -aG input "$USER"
```

Log out and back in after changing group membership.
