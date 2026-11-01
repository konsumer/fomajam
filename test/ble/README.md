# Bluetooth MIDI test tools (macOS)

Two small Swift programs that use Apple's CoreBluetooth, to test the FM-1's Bluetooth MIDI from a Mac without other gear.
The terminal app needs Bluetooth permission the first time.

```sh
swiftc -O test/ble/bletest.swift -o build/bletest
swiftc -O test/ble/leader.swift -o build/leader
```

- `build/bletest NAME SECONDS [listen]`: the Mac as the central. Finds the first device whose name starts with NAME
  (`FOMAJAM`), connects, turns on notifications, sends four notes and a clock start / 24 ticks / stop (not with
  `listen`), and prints every notification that comes in (`in: 9E 93 90 48 64 ...`, raw BLE-MIDI packets).
- `build/leader BPM SECONDS`: the Mac as a peripheral. Advertises as `BLETEST` with the BLE-MIDI service and, once a
  central subscribes, sends start and a MIDI clock at BPM; prints what the central writes. For testing an FM-1 that
  connects out (the follower: `MIDI IN` on `BT`, `SYNC` on `EXT`).
