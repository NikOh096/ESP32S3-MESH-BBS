# Interface decisions

The app should help someone run a community board without learning Bluetooth
internals. Confidence comes from clear feedback and a useful next step after an error.

Home opens on conversations. Devices has two steps: connect MESHBBS, then choose
its radio. Tapping a discovered radio starts pairing; a PIN prompt follows a real
security request. Connect becomes Connecting and Connected, and is available again
after failure. Cancel appears only during an active attempt. Reported model details
come after pairing; editable names are not treated as proof of hardware.

Settings holds moderation and notifications. Activity is available for delivery
problems. Infrequent options are behind Connection options, and Home has one
creation action rather than nine empty slots.

These choices apply status visibility, familiar language and recognition from
[Nielsen's usability heuristics](https://www.nngroup.com/articles/ten-usability-heuristics/),
[progressive disclosure](https://www.nngroup.com/articles/progressive-disclosure/)
for advanced controls, and
[actionable errors](https://www.nngroup.com/articles/error-message-guidelines/).

Four stable tabs follow
[Android navigation guidance](https://developer.android.com/design/ui/mobile/guides/layout-and-content/layout-and-nav-patterns).
Buttons have at least 48 dp targets, text scales with the system, form labels remain
visible, selected tabs have explicit state, and connection status is not color-only.
[Android accessibility guidance](https://developer.android.com/guide/topics/ui/accessibility/apps)

The installer asks for physical facts it cannot infer safely: memory markings and
LED wiring. It checks the chip and memory after USB selection, shows the selected
image, then waits for Install. Details stay available without dominating the page.

Acceptance tasks: observe a new user install, enroll, pair display and screenless
radios, recover from a wrong PIN, post/read, mute alerts, and investigate a missing
reply. Repeat with large text, TalkBack, denied permissions, weak BLE and interrupted
installation. Record hesitation and errors. These are planned usability checks,
not sessions already performed.
