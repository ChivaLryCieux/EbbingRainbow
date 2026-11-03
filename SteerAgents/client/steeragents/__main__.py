"""steeragents CLI.

    python -m steeragents toolsets
    python -m steeragents list <toolset>
    python -m steeragents desc <toolset> [tool ...]
    python -m steeragents call <toolset> <tool> [json | @file | k=v ...]
    python -m steeragents cmd <console command>
    python -m steeragents shot <name.png>
"""
import json
import sys

from .console import run_cmd
from .toolsets import call, connect, describe, list_toolsets, text_of


def _args(argv):
    if not argv:
        return {}
    raw = argv[0]
    if raw.startswith("@"):
        with open(raw[1:], encoding="utf-8-sig") as f:
            return json.load(f)
    if raw.startswith("{"):
        return json.loads(raw)
    out = {}
    for kv in argv:
        k, v = kv.split("=", 1)
        out[k] = v
    return out


def main():
    m = connect()
    cmd = sys.argv[1] if len(sys.argv) > 1 else "toolsets"
    if cmd == "toolsets":
        print(list_toolsets(m)[:12000])
    elif cmd == "list":
        for t in describe(m, sys.argv[2]).get("tools", []):
            print(t["name"].split(".")[-1])
    elif cmd == "desc":
        wanted = set(sys.argv[3:])
        for t in describe(m, sys.argv[2]).get("tools", []):
            short = t["name"].split(".")[-1]
            if wanted and short not in wanted:
                continue
            print("==", short)
            print(json.dumps(t.get("inputSchema", {}), ensure_ascii=False))
    elif cmd == "call":
        print(call(m, sys.argv[2], sys.argv[3], _args(sys.argv[4:])))
    elif cmd == "cmd":
        print(run_cmd(m, " ".join(sys.argv[2:])))
    elif cmd == "shot":
        from .capture import capture, stats
        p = capture(m, sys.argv[2] if len(sys.argv) > 2 else "shot.png")
        print(p)
        stats(p)
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
