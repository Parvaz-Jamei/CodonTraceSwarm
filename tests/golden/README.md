# Shared golden wire fixtures

- `messages.jsonl`: the 7 `genesis_swarm_v2` envelopes the C++ native fixture
  (`tests/native.cpp`) emits. C++ output must stay byte-identical to this file.
- `commands.jsonl`: the 7 `genesis_swarm_command_v1` commands the Genesis host
  (`SwarmCoordinator.command`) generated at commit
  a928ee4ebeff20dbda721937cfad8046de435cba, one compact sorted-key JSON object per line.

`tools/verify_golden.py` checks both directions against a Genesis checkout, plus a
nesting-depth-64 case. Synthetic fixtures only: no hardware, broker, or ALife run.
The same files are proposed for the Genesis repository so both sides test the same
bytes; until that lands, only this repository holds them.
