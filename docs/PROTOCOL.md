# Meshtastic transport

The target is firmware 2.7.26.54e0d8d, pinned commit
54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb. See DEPENDENCIES.md for protobuf provenance.

- BLE service: 6ba1b218-15a8-461f-9fa8-5dcae273eafd
- ToRadio: f75c76d2-129e-4dad-a1dd-7866124401e7
- FromRadio: 2c55e69e-4993-11ed-b878-0242ac120002
- FromNum: ed9da18c-a800-4f66-a670-aa7547e34453

The client discovers handles, subscribes to FromNum, drains FromRadio using long
reads, and falls back to polling. It requests configuration with a random nonce
and waits for matching config_complete_id and a valid own-node ID. Its local API
heartbeat uses nonce zero to avoid requesting a nodeinfo radio broadcast.

Only valid UTF-8 TEXT_MESSAGE_APP DMs addressed to that own-node ID become commands.
Replies target the original sender, preserve channel and reply_id, and use a fresh
packet ID and want_ack. When the incoming DM is PKI encrypted with a 32-byte public
key, replies preserve PKI addressing/key fields. Node queue acceptance and recipient
ACK are distinct states; an absent ACK is not proof that the message was not delivered.

BBS commands, stable IDs, expiry and queue semantics are in BBS-USER-GUIDE.md.
Owner-only operations use the separate authenticated BLE protocol. Legacy bbs.c /
setup_protocol.c tests remain regression fixtures; main.c dispatches visitor text
only to bulletins_command, not the older POST/DEL command handler.
