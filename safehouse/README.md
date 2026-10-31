# SAFEHOUSE relay nodes

ESP32 firmware for the Wi-Fi "relay nodes" in a SAFEHOUSE location game. Each
node is an open access point (e.g. `SAFEHOUSE_NODE_01`) with a captive portal.
When a player joins, their phone opens the chapter page: an intro animation,
the handler briefing, the local heritage note and whether it is verified,
objectives, safety notes and the puzzle. The right password reveals the intel,
an unlock code, a field question and an interview lead. On the final node the
player also makes the season's ending choice.

```
campaigns/wayanad-east.json        campaign content (source of truth)
tools/build_campaign.py            validates the JSON, generates the header
SafehouseNode/campaign_data.h      generated: all chapters, HTML-escaped
SafehouseNode/node_config.h        which chapter this board serves, AP settings
SafehouseNode/SafehouseNode.ino    firmware
platformio.ini                     envs node1..node6
```

## Build and flash

```sh
python3 tools/build_campaign.py campaigns/wayanad-east.json   # validate + regenerate header
pio run -e node3 -t upload                                     # flash chapter 3
```

In the Arduino IDE, open `SafehouseNode/SafehouseNode.ino`, set `NODE_INDEX` in
`node_config.h`, and upload to an "ESP32 Dev Module".

Before deploying, change `OPS_KEY` in `node_config.h`. Operators can read
visit, attempt, unlock and ending counters at
`http://10.7.0.1/ops?key=<OPS_KEY>` while joined to the node. The counters are
kept in NVS, so they survive power cuts.

## How answers are checked

The node computes `md5(lowercase(trim(input)))` and compares it with the
chapter's `answer_hash`. After 5 wrong tries, a client is locked out for 60
seconds. These answers are short numbers, so the hash only keeps them from
showing in plain text. Anyone with the firmware can recover them. Run
`--reveal` to print an operator sheet with each answer and unlock code.

Unlock codes are `CG-` + the first 8 hex characters of
`md5("<slug>:<node_id>:<answer_hash>")`, so a companion app or a game master
can check them offline.

## Validator checks

`build_campaign.py` stops on structural errors (missing or duplicate SSIDs,
SSIDs over 32 bytes, bad hashes) and warns about likely content mistakes:

- A hashed answer that doesn't fit its prompt (for example "count × 101" but
  the answer isn't a multiple of 101).
- An answer hash it can't reverse as a number.
- Coordinates more than 8 km from a rough reference point for the named town.

Use `--strict` in CI to make warnings fail the build.
