# Security

New owners require physical enrollment. The app uses a nonce-bound HMAC over an
encrypted BLE connection. Visitor commands cannot change configuration, rules,
bans or ownership. PIN submission is tied to the current pairing attempt; normal
logs exclude PINs and owner secrets. The installer checks image SHA-256 values
and verifies flash writes without erasing owner storage.

This release does not provision secure boot or flash encryption. Physical access
can permit reflashing or erasure. Display names are not verified identities.
Bounded queues and rate limits do not prevent radio jamming or every denial of
service. MESHBBS is not an emergency alerting service.

Use the repository's **Security → Report a vulnerability** option if available.
If unavailable, open a minimal issue requesting a private contact channel, without
exploit details or sensitive data. Include affected versions in non-sensitive reports.

Never upload owner keys, Android signing keys, PINs, channel keys or flash dumps.
Private local state, SDK settings and personal attachments are excluded from source
and release packages.
