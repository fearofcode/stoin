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

TX Bolt and Gemini PR input without phrase keys only need serial access plus `/dev/uinput` output access.

## Keyboard capture and phrase keys

The `--input qwerty` mode and `--phrase-key` options need read/grab access to physical keyboard event devices under `/dev/input/event*`. This is separate from `/dev/uinput`.

For example, these options make both F13 and F14 activate the phrase layer:

```sh
stoin --phrase-key F13 --phrase-key F14
```

`--phrase-key` is repeatable, so either pedal can activate the same phrase layer.

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
