"""Project/environment configuration.

Resolution order (first hit wins):
  STEER_* environment variable -> steeragents.toml next to the .uproject
  (or ./steeragents.toml) -> built-in defaults.

A `.uproject` file is auto-detected by walking up from the current working
directory, so project paths never need to be hardcoded in scripts.
"""
import os
from pathlib import Path

try:
    import tomllib
except ImportError:  # Python < 3.11
    tomllib = None


def _find_uproject(start=None):
    d = Path(start or os.getcwd()).resolve()
    for p in [d, *d.parents]:
        hits = list(p.glob("*.uproject"))
        if hits:
            return hits[0]
    return None


class Config:
    def __init__(self):
        self.uproject = _find_uproject()
        self.project_dir = self.uproject.parent if self.uproject else Path(os.getcwd()).resolve()
        self._toml = self._load_toml()

    def _load_toml(self):
        candidates = [self.project_dir / "steeragents.toml",
                      Path(os.getcwd()) / "steeragents.toml"]
        if tomllib is None:
            return {}
        for c in candidates:
            if c.is_file():
                try:
                    return tomllib.loads(c.read_text(encoding="utf-8"))
                except Exception:
                    return {}
        return {}

    def get(self, env_name, toml_key, default=None):
        v = os.environ.get(env_name)
        if v:
            return v
        node = self._toml
        for part in toml_key.split("."):
            if not isinstance(node, dict) or part not in node:
                return default
            node = node[part]
        return node

    @property
    def mcp_url(self):
        return self.get("STEER_MCP_URL", "mcp.url", "http://127.0.0.1:8000/mcp")

    @property
    def shot_dir(self):
        return Path(self.get("STEER_SHOT_DIR", "shots.dir",
                             str(self.project_dir / "Saved" / "Screenshots")))

    @property
    def editor_cmd(self):
        # e.g. D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe
        return self.get("STEER_EDITOR_CMD", "engine.editor_cmd", None)

    @property
    def build_bat(self):
        # e.g. D:/UE_5.8/Engine/Build/BatchFiles/Build.bat
        return self.get("STEER_BUILD_BAT", "engine.build_bat", None)


config = Config()
