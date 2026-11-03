"""High-level toolset helpers: connect, describe, call."""
import json

from .mcp import MCP


def connect(url=None):
    m = MCP(url)
    if not m.init():
        raise RuntimeError("MCP init failed — is the UE editor running?")
    return m


def text_of(res):
    """Flatten an MCP tool result's content blocks into plain text."""
    if not res:
        return ""
    out = []
    for c in res.get("content", []):
        if c.get("type") == "text":
            out.append(c.get("text", ""))
    return "\n".join(out)


def call(m, toolset, tool, args=None):
    """call_tool shorthand; returns decoded text (JSON string)."""
    res = m.call("call_tool", {"toolset_name": toolset, "tool_name": tool,
                               "arguments": args or {}})
    return text_of(res)


def list_toolsets(m):
    return text_of(m.call("list_toolsets"))


def describe(m, toolset):
    txt = text_of(m.call("describe_toolset", {"toolset_name": toolset}))
    try:
        return json.loads(txt)
    except Exception:
        return {"raw": txt}
