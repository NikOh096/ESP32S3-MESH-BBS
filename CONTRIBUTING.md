# Contributing

Start with an issue explaining the user-visible problem. For a radio result,
include exact model/revision, radio firmware, BBS profile, Android version and
whether pairing used a displayed or fixed PIN. Omit the PIN itself.

Keep changes focused. Comments should explain non-obvious decisions, especially
packet bounds, persistence order, security state and timing. Add tests for changed
protocol or storage behavior. Record physical results separately from software
tests: a successful build does not prove a radio works.

Run relevant checks from [BUILDING](docs/BUILDING.md) before a pull request.
Preserve upstream notices. Project contributions use GPL-3.0; third-party components
retain their licenses. Be respectful, protect privacy and make reports useful to
someone encountering the problem for the first time.
