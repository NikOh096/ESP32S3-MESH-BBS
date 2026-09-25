# Bluetooth help

Check the two states in Devices. **Board connected** means your phone has owner
access. **Radio ready** means MESHBBS can use the selected radio's API. Both are
needed to configure and test from the phone; the radio BBS keeps running without it.

## Find the BBS board

For first setup or owner replacement, tap BOOT three times, then hold it for three
seconds and release within twelve seconds. Blue pulses mark the three-minute
window. Use Find my board, tap its unique name, then Use this phone. RST is not part
of owner enrollment. A saved owner reconnects without pressing BOOT.

Check Bluetooth permissions if the list is empty. Outside enrollment the board
filters access to its saved owner; ordinary discovery will not show its setup name.

## Pair a radio

Disconnect its native phone app, keep the radio nearby, then use Find nearby radios.
Tap the result to start pairing. Enter the displayed or configured six-digit PIN
when prompted. A radio with no screen generally uses a fixed PIN. No PIN is a
separate Meshtastic setting, not a synonym for an unknown PIN. MeshCore requires
Companion BLE firmware. See the [radio guide](RADIO-COMPATIBILITY.md).

After a timeout, use Connect and the PIN for that new attempt. Bonded reconnects
may not ask for another PIN. Do not routinely delete keys on just one side; this
creates stale bonds. Explicit pairing can repair the selected radio's stale bond.
The firmware does not evict unrelated bonds automatically.

## Delivered but no reply

DM the radio attached to MESHBBS from a second radio. A relay forwarding packets
is not the BBS address. Start with PING or HELP and leave ten seconds between
requests. Wait for all numbered reply parts.

Activity distinguishes a received command, queued reply, radio acceptance and
recipient acknowledgment. A delivered DM alone does not show that the BBS read it.
For MeshCore, make sure the visitor is a saved contact in the companion radio.

USB diagnostics are read-only: `python scripts/configure.py --port COMx --status`,
`--activity` or `--monitor`. Replace COMx with the observed port. Logs can contain
message text and identifiers; review them before sharing.
