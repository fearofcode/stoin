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

If qwerty capture is used later, `/dev/input/event*` permissions may need a similar rule or group policy. TX Bolt and Gemini PR input only need serial access plus `/dev/uinput` output access.
