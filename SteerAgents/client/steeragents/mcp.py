"""Minimal HTTP JSON-RPC client for the UE Model Context Protocol server."""
import json
import os
import urllib.request

from .config import config

QUIET = bool(os.environ.get("STEER_QUIET") or os.environ.get("MCP_QUIET"))


def _log(*parts):
    if not QUIET:
        print(*parts)


class MCP:
    def __init__(self, url=None):
        self.url = url or config.mcp_url
        self.sid = None
        self.version = None

    def post(self, payload, session=True, timeout=120):
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(self.url, data=data, method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("Accept", "application/json, text/event-stream")
        if session and self.sid:
            req.add_header("Mcp-Session-Id", self.sid)
        if session and self.version:
            req.add_header("Mcp-Protocol-Version", self.version)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                body = resp.read().decode("utf-8", "replace")
                sid = resp.headers.get("Mcp-Session-Id")
                if sid:
                    self.sid = sid
                return resp.status, body
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode("utf-8", "replace")

    def init(self):
        status, body = self.post({
            "jsonrpc": "2.0", "id": 1, "method": "initialize",
            "params": {
                "protocolVersion": "2025-06-18",
                "capabilities": {},
                "clientInfo": {"name": "steeragents", "version": "0.1"},
            },
        }, session=False)
        _log("INIT", status, body[:500])
        if status >= 400:
            return False
        obj = json.loads(body)
        self.version = obj.get("result", {}).get("protocolVersion")
        status2, body2 = self.post({
            "jsonrpc": "2.0", "method": "notifications/initialized", "params": {},
        })
        _log("INITIALIZED", status2, body2[:200])
        return True

    def rpc(self, method, params=None, id=2):
        status, body = self.post({
            "jsonrpc": "2.0", "id": id, "method": method,
            "params": params or {},
        })
        if status >= 400:
            print("HTTP", status, body[:1000])
            return None
        try:
            obj = json.loads(body)
        except Exception:
            print("BAD BODY", body[:1000])
            return None
        if "error" in obj:
            print("RPC ERROR", json.dumps(obj["error"], ensure_ascii=False)[:1000])
            return None
        return obj.get("result")

    def tools(self):
        res = self.rpc("tools/list", {}, 2)
        return res.get("tools", []) if res else []

    def call(self, name, args=None):
        return self.rpc("tools/call", {"name": name, "arguments": args or {}}, 3)
