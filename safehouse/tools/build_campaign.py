#!/usr/bin/env python3
"""Validate a SAFEHOUSE campaign JSON and generate the ESP32 node header.

Usage:
    python3 tools/build_campaign.py campaigns/wayanad-east.json
    python3 tools/build_campaign.py campaigns/wayanad-east.json --reveal   # print recovered answers
    python3 tools/build_campaign.py campaigns/wayanad-east.json --strict   # exit 1 on warnings

Writes SafehouseNode/campaign_data.h (override with --out).

The firmware checks answers as md5(lowercase(trim(input))), so the hashes in
the JSON must be produced the same way.
"""

import argparse
import hashlib
import html
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "SafehouseNode" / "campaign_data.h"

# Rough reference points used only as a sanity check on campaign coordinates.
# They are approximate town/site centres, not the exact host locations; the
# campaign's own verification checklist still requires on-the-ground checks.
REFERENCE_POINTS = {
    "sulthan bathery": (11.6660, 76.2610),
    "edakkal": (11.6245, 76.2355),
    "ambalavayal": (11.6170, 76.2130),
    "kolagappara": (11.6400, 76.2050),
    "pulpally": (11.7930, 76.1600),
    "lakkidi": (11.5170, 76.0220),
}
REFERENCE_TOLERANCE_KM = 8.0
NUMERIC_SEARCH_LIMIT = 100000

MAX_SSID_LEN = 32


def haversine_km(a, b):
    lat1, lng1 = map(math.radians, a)
    lat2, lng2 = map(math.radians, b)
    d = math.sin((lat2 - lat1) / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin((lng2 - lng1) / 2) ** 2
    return 6371.0 * 2 * math.asin(math.sqrt(d))


def md5(s):
    return hashlib.md5(s.encode("utf-8")).hexdigest()


def recover_numeric(answer_hash):
    """Answers here are short numbers, so unsalted MD5 is trivially reversible."""
    for i in range(NUMERIC_SEARCH_LIMIT):
        if md5(str(i)) == answer_hash:
            return str(i)
        if i < 10 and md5("0" + str(i)) == answer_hash:
            return "0" + str(i)
    return None


class Report:
    def __init__(self):
        self.errors = []
        self.warnings = []

    def error(self, where, msg):
        self.errors.append(f"[{where}] {msg}")

    def warn(self, where, msg):
        self.warnings.append(f"[{where}] {msg}")


def validate(c, rep, reveal):
    chapters = c.get("chapters", [])
    answers = {}
    if not chapters:
        rep.error("campaign", "no chapters")
        return answers

    orders = [ch.get("order") for ch in chapters]
    if orders != list(range(1, len(chapters) + 1)):
        rep.warn("campaign", f"chapter order values are {orders}, expected 1..{len(chapters)}")

    ssids = set()
    for ch in chapters:
        cid = ch.get("id", "?")
        node = ch.get("node", {})
        ssid = node.get("ssid", "")
        if not ssid:
            rep.error(cid, "node.ssid missing")
        elif len(ssid.encode()) > MAX_SSID_LEN:
            rep.error(cid, f"SSID '{ssid}' longer than {MAX_SSID_LEN} bytes")
        elif ssid in ssids:
            rep.error(cid, f"duplicate SSID '{ssid}'")
        ssids.add(ssid)

        puzzles = [t for t in ch.get("tasks", []) if t.get("type") == "puzzle"]
        if len(puzzles) != 1:
            rep.error(cid, f"expected exactly one puzzle task, found {len(puzzles)}")
            continue
        p = puzzles[0]
        h = p.get("answer_hash", "").lower()
        if not re.fullmatch(r"[0-9a-f]{32}", h):
            rep.error(cid, "answer_hash is not a 32-char hex MD5")
            continue

        ans = recover_numeric(h)
        answers[cid] = ans
        prompt = p.get("prompt", "").lower()
        if ans is None:
            rep.warn(cid, f"answer_hash is not a number 0-{NUMERIC_SEARCH_LIMIT - 1}; "
                          "prompt asks for a number, confirm the expected answer string")
        else:
            if "multiplied by 101" in prompt and int(ans) % 101 != 0:
                rep.warn(cid, f"prompt asks for a count x 101 but the hashed answer is {'%s' % ans if reveal else 'not a multiple of 101'}")
            if ("2-digit" in prompt or "two digits" in prompt) and len(ans) != 2:
                rep.warn(cid, f"prompt asks for 2 digits but the hashed answer has {len(ans)} digit(s)")

        loc = ch.get("location", {})
        name = loc.get("name", "").lower()
        for key, ref in REFERENCE_POINTS.items():
            if key in name:
                d = haversine_km((loc.get("lat", 0), loc.get("lng", 0)), ref)
                if d > REFERENCE_TOLERANCE_KM:
                    rep.warn(cid, f"location '{loc.get('name')}' is {d:.0f} km from the reference point for "
                                  f"{key.title()} ({ref[0]:.4f}, {ref[1]:.4f}); check lat/lng")

        if ch.get("requires_event") and ch.get("self_serve"):
            rep.warn(cid, "requires_event and self_serve are both true")

    if any(a is not None for a in answers.values()):
        rep.warn("campaign", "answers are short numbers hashed with unsalted MD5; anyone with the firmware "
                             "can recover them in milliseconds (fine for a game, not a secret)")

    prompts = c.get("interview_prompts", [])
    if prompts and len(prompts) != len(chapters):
        rep.warn("campaign", f"{len(prompts)} interview prompts for {len(chapters)} chapters")

    base = c.get("region", {}).get("base")
    if base:
        for key, ref in REFERENCE_POINTS.items():
            if key in c["region"].get("name", "").lower():
                d = haversine_km((base["lat"], base["lng"]), ref)
                if d > REFERENCE_TOLERANCE_KM:
                    rep.warn("region", f"base is {d:.0f} km from the reference point for {key.title()}")
    return answers


# ---------------------------------------------------------------- codegen

def cstr(s):
    """HTML-escape then emit as a C string literal."""
    s = html.escape(s or "", quote=True)
    out = []
    for ch in s:
        o = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif o < 0x20:
            out.append("\\x%02x" % o)
        elif o < 0x80:
            out.append(ch)
        else:
            # UTF-8 bytes as octal escapes (cannot be followed-by-hex ambiguity)
            out.extend("\\%03o" % b for b in ch.encode("utf-8"))
    return '"' + "".join(out) + '"'


def unlock_code(slug, node_id, answer_hash):
    return "CG-" + md5(f"{slug}:{node_id}:{answer_hash}")[:8].upper()


def generate(c):
    slug = c["region"]["slug"]
    meta, handler, ending = c["meta"], c["handler"], c.get("ending", {})
    interviews = c.get("interview_prompts", [])
    L = []
    w = L.append
    w("// GENERATED by tools/build_campaign.py from campaigns/%s.json - do not edit by hand." % slug)
    w("// All strings are pre-escaped for HTML.")
    w("#pragma once")
    w("#include <stdint.h>")
    w("")
    w("#define CAMPAIGN_SLUG %s" % cstr(slug))
    w("#define CAMPAIGN_TITLE %s" % cstr(meta["title"]))
    w("#define CAMPAIGN_TAGLINE %s" % cstr(meta.get("tagline", "")))
    w("#define CAMPAIGN_FICTION_NOTICE %s" % cstr(meta.get("fiction_notice", "")))
    w("#define CAMPAIGN_HANDLER %s" % cstr(handler.get("codename", "")))
    w("#define CAMPAIGN_ANTAGONIST %s" % cstr(c.get("antagonist", {}).get("name", "")))
    w("#define CAMPAIGN_ENDING_A %s" % cstr(ending.get("choice_a", "")))
    w("#define CAMPAIGN_ENDING_B %s" % cstr(ending.get("choice_b", "")))
    w("#define CAMPAIGN_SEASON2_HOOK %s" % cstr(ending.get("season2_hook", "")))
    w("")
    w("struct TaskContent { const char* type; const char* prompt; };")
    w("")
    w("struct NodeContent {")
    w("  uint8_t order;")
    w("  const char* chapterId;")
    w("  const char* title;")
    w("  const char* locationName;")
    w("  const char* briefing;")
    w("  const char* mythText;")
    w("  const char* mythStatus;")
    w("  const char* mythSource;")
    w("  const TaskContent* tasks;")
    w("  uint8_t taskCount;")
    w("  const char* puzzlePrompt;")
    w("  const char* answerHash;   // md5(lowercase(trim(answer)))")
    w("  const char* intel;")
    w("  const char* nodeId;")
    w("  const char* ssid;         // raw, not escaped")
    w("  const char* portalScene;")
    w("  const char* fieldQuestion;")
    w("  const char* interview;")
    w("  const char* const* safety;")
    w("  uint8_t safetyCount;")
    w("  bool selfServe;")
    w("  bool requiresEvent;")
    w("  bool finale;")
    w("  const char* unlockCode;")
    w("};")
    w("")

    chapters = c["chapters"]
    for i, ch in enumerate(chapters):
        n = i + 1
        tasks = [t for t in ch["tasks"] if t["type"] != "puzzle"]
        w("static const TaskContent CH%d_TASKS[] = {" % n)
        for t in tasks:
            w("  {%s, %s}," % (cstr(t["type"]), cstr(t["prompt"])))
        if not tasks:
            w("  {\"\", \"\"},")
        w("};")
        w("static const char* const CH%d_SAFETY[] = {" % n)
        for s in ch.get("safety_notes", []) or [""]:
            w("  %s," % cstr(s))
        w("};")
    w("")
    w("static const NodeContent NODES[] = {")
    for i, ch in enumerate(chapters):
        n = i + 1
        node, myth = ch["node"], ch.get("myth", {})
        puzzle = next(t for t in ch["tasks"] if t["type"] == "puzzle")
        tasks = [t for t in ch["tasks"] if t["type"] != "puzzle"]
        ssid = node["ssid"].replace("\\", "\\\\").replace('"', '\\"')
        w("  {")
        w("    %d, %s, %s," % (ch["order"], cstr(ch["id"]), cstr(ch["title"])))
        w("    %s," % cstr(ch["location"]["name"]))
        w("    %s," % cstr(ch["briefing"]))
        w("    %s, %s, %s," % (cstr(myth.get("text")), cstr(myth.get("status")), cstr(myth.get("source"))))
        w("    CH%d_TASKS, %d," % (n, len(tasks)))
        w("    %s," % cstr(puzzle["prompt"]))
        w("    \"%s\"," % puzzle["answer_hash"].lower())
        w("    %s," % cstr(ch.get("intel_unlocked")))
        w("    %s, \"%s\"," % (cstr(node["id"]), ssid))
        w("    %s," % cstr(node.get("portal_scene")))
        w("    %s," % cstr(node.get("puzzle")))
        w("    %s," % cstr(interviews[i] if i < len(interviews) else ""))
        w("    CH%d_SAFETY, %d," % (n, len(ch.get("safety_notes", []))))
        w("    %s, %s, %s," % (str(bool(ch.get("self_serve"))).lower(),
                                str(bool(ch.get("requires_event"))).lower(),
                                "true" if i == len(chapters) - 1 else "false"))
        w("    \"%s\"," % unlock_code(c["region"]["slug"], node["id"], puzzle["answer_hash"].lower()))
        w("  },")
    w("};")
    w("")
    w("static const uint8_t NODE_COUNT = %d;" % len(chapters))
    w("")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("campaign", type=Path)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--reveal", action="store_true", help="print recovered answers and unlock codes (spoilers)")
    ap.add_argument("--strict", action="store_true", help="treat warnings as errors")
    ap.add_argument("--check", action="store_true", help="validate only, do not write the header")
    args = ap.parse_args()

    c = json.loads(args.campaign.read_text(encoding="utf-8"))
    rep = Report()
    answers = validate(c, rep, args.reveal)

    for e in rep.errors:
        print("ERROR  ", e)
    for wmsg in rep.warnings:
        print("WARN   ", wmsg)
    if args.reveal:
        print("\nOperator sheet (spoilers):")
        for ch in c["chapters"]:
            p = next((t for t in ch["tasks"] if t["type"] == "puzzle"), None)
            if p:
                code = unlock_code(c["region"]["slug"], ch["node"]["id"], p["answer_hash"].lower())
                print(f"  {ch['id']}  {ch['node']['ssid']:<20} answer={answers.get(ch['id']) or '?':<6} unlock={code}")

    if rep.errors:
        print(f"\n{len(rep.errors)} error(s); header not written.")
        return 1
    if not args.check:
        args.out.write_text(generate(c), encoding="utf-8")
        print(f"\nWrote {args.out.relative_to(ROOT) if args.out.is_relative_to(ROOT) else args.out}")
    if args.strict and rep.warnings:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
