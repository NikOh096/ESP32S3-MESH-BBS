# Using MESHBBS

A bulletin board stores discussions for people to read later. A bulletin is the
opening message; comments belong to that thread. Radio participants use ordinary
Meshtastic or MeshCore direct messages. They do not need the MESHBBS owner app.

The node connected to MESHBBS by Bluetooth is the BBS address. DM that attached radio from a different radio. A tower or relay forwarding
packets does not become the BBS address.

## Example

    CREATE Trail conditions | North trail is muddy. Any updates?
    UPDATE
    A000042
    !A000042 South trail is clear now.
    A000042

Use the actual ID returned by CREATE/UPDATE. A list line such as
`1. A000042 Trail conditions (2 replies)` means position 1, permanent ID A000042.
Send A000042 to read it. The position is not a command. IDs are case-insensitive;
optional # before the ID is accepted. IDs are unique within this board.

UPDATE lists up to nine active bulletins from oldest to newest. CREATE makes a
24-hour thread. If full, the request is saved in a FIFO queue and its reply gives
the queue position and the next scheduled expiry. QUEUE checks your current place.
That next opening is not a guaranteed publication time for every queued request;
requests ahead of yours go first and the owner may change expiry dates. If all
nine active threads have no expiry, there is no scheduled opening.

The waiting limit is 32. If that is full, the board explicitly says the new
bulletin was NOT saved. Waiting bulletins do not consume their 24 hours until
published. Expiry deletes a thread and its comments, moves the list positions up,
and publishes the next waiting item. A permanent ID is never reassigned.

A000001 to A999999 are followed by B000000, and eventually Z999999. The finite ID
space holds 25,999,999 publications; exhaustion rejects new publications instead
of reusing IDs. Lifetime thread/reply/message totals are separate from active
counts and survive deletion and restart. Queued items count as published threads
when promoted. Every publication consumes an ID, including a test post.

## Commands and limits

- HELP (or ?) returns instructions; PING tests the round trip.
- RULES returns this board's owner-configured community rules.
- UPDATE (or LIST) lists threads.
- CREATE title | text publishes or queues a bulletin.
- The seven-character ID reads the opening message and all comments.
- !ID text comments on that exact ID.
- QUEUE reports your pending requests.

Titles: 48 UTF-8 bytes. Bulletin bodies and comments: 160 UTF-8 bytes. The phone's
own message limit also applies to the whole command. A thread holds 32 comments;
when full, the board keeps them and rejects additional comments. Node labels and
IDs are shown with timestamps; names are not verified real-world identities.

Replies are split into numbered messages of at most 200 bytes for Meshtastic or 160 for MeshCore, at least five
seconds apart. Wait for all parts before another long request. Leave ten seconds
between commands from the same node. Duplicate radio packet IDs are suppressed.
The board queues two complete response jobs; requests arriving while both are
occupied may need a retry. Expired/deleted thread responses stop sending.

Unknown valid DMs return a HELP prompt. Channel broadcasts, malformed payloads,
ACKs and reactions are not treated as commands. There are no radio commands to
delete/moderate threads, ban nodes, change expiry or change configuration.

## Owner app

Home creates/deletes threads, opens comments, edits expiry, shows lifetime totals
and manages the waiting queue. A new owner's thread defaults to 24 hours; choose
no expiry, a duration, or a date/time before saving. All radio dates are UTC;
the app displays local phone time. Expired/deleted material is not an archive.

Opening a thread marks the comments successfully loaded by the app as read. A
comment arriving during that read stays unread. New visitor comments drive the
red reminder; deleting/expiry also removes that thread's unread state.

Settings offers sound+vibration, vibration only, silent notifications, or mute,
plus background monitoring. Notifications use Bluetooth, not Internet push, and
Android system settings can override the app's sound/vibration choices. Alerts
cover new visitor bulletins and new comments while the phone can monitor the board.

Ban full eight-digit hexadecimal node IDs, for example !1234abcd. MeshCore uses a saved local alias for each full
public key; use the alias shown by MESHBBS, not a guessed key prefix. Banned senders
receive your saved ban reply instead of using the board, subject to rate limits.
Banning also removes their waiting bulletins; already published discussions are
left for the owner to moderate. Node-ID bans are not proof of a person's identity.

## LEDs and time

- Normal running: green double heartbeat every five seconds.
- Physical owner enrollment: quick blue pulse.
- Connecting: rapid blue flashes.
- Radio API ready: dim solid blue, interrupted by status reminders.
- Unread visitor comments: half-brightness red flash every five seconds until read
  or that thread is deleted/expires. Pairing/connecting animations take priority.

After power loss the board awaits a valid clock from the owner phone or a
radio clock. The MeshCore adapter reads the companion clock during setup;
it does not set time from visitor messages. Timed threads stay unavailable until then; new timed
publications cannot start. The app automatically synchronizes time on connection.

Activity shows observed live activity and the newest saved board events. USB
ACTIVITY prints the complete 32-event saved ring. Events are checkpointed every
30 seconds; abrupt power loss can omit recent activity. Logs contain short public
message previews, not owner keys or pairing PINs. Thread contents commit immediately.

## Design basis

TC2-BBS, TinyBBS and Meshbbs use differing menus/commands; there is no single
universal Meshtastic BBS command standard. This implementation follows the requested
public-thread workflow, persistent IDs and bounded LoRa traffic.

- https://github.com/TheCommsChannel/TC2-BBS-mesh
- https://github.com/GoatsAndMonkeys/TinyBBS
- https://github.com/martinbogo/meshbbs

## Community rules

Send RULES before posting. Default rules ask visitors to be respectful, protect
personal information, use emergency services for urgent needs, verify claims,
conserve airtime, and follow owner moderation. Delivery is not guaranteed and
names/IDs do not authenticate people. The owner can edit these defaults under
Settings > Edit board rules. Up to six 160-byte rules are saved on the BBS, survive
reboot, and are shared with visitors. Blank entries are omitted. Radio users
cannot change the rules.
