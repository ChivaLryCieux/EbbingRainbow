"""Run UE console commands / python scripts via the Output-Log Cmd textbox
(SlateInspector toolset). Works on the live editor — the same thing as
typing into the editor's own Cmd box."""
import re

from .mcp import MCP
from .toolsets import text_of

TS = "SlateInspectorToolset.SlateInspectorToolset"

_TEXTBOX_RE = re.compile(
    r'textbox(?:\s+"[^"]*")?(?:\s+\[focused\])?\s+\[pos=([-\d]+),([-\d]+)\s+'
    r'size=([-\d]+),([-\d]+)\]\s+\[ref=([^\]]+)\]')


def _call(m, tool, args):
    r = m.call("call_tool", {"toolset_name": TS, "tool_name": tool, "arguments": args})
    return text_of(r)


def find_cmd_box(m):
    """Locate the status-bar Cmd textbox: the widest textbox in the bottom
    quarter of the UI."""
    snap = _call(m, "Snapshot", {"ref": "", "maxDepth": 30})
    boxes = []
    for x, y, w, h, ref in _TEXTBOX_RE.findall(snap):
        boxes.append((int(x), int(y), int(w), int(h), ref))
    if not boxes:
        return None
    max_y = max(b[1] for b in boxes)
    cand = [b for b in boxes if b[2] > 80 and b[1] > max_y * 0.75]
    if not cand:
        return None
    return max(cand, key=lambda b: b[1])[4]


def run_cmd(m, cmd):
    ref = find_cmd_box(m)
    if not ref:
        print("[steeragents.console] cmd textbox not found")
        return ""
    return _call(m, "Type", {"ref": ref, "text": cmd, "submit": True}).strip()


def run_py(m, script):
    """`py <script>` — script must be a forward-slash absolute path the
    in-editor Python interpreter can open."""
    return run_cmd(m, "py " + script.replace("\\", "/"))
