# -*- coding: utf-8 -*-
"""
smoke_test_painterly.py
=======================

一次运行只验证「一种状态」，避免状态切换与截图之间的竞态：

  1. 第一帧就把 Paint.Style / Paint.DebugMode 设好
  2. 等 WARMUP 秒（Lumen / 着色器 / 曝光稳定）
  3. Paint.Shot 截一张
  4. 再等 2 秒退出

状态通过环境变量传入：

    PAINT_STYLE   0 = 原样，1 = 厚涂（默认 1）
    PAINT_DEBUG   0 final / 1 flow / 2 ink / 3 normal / 4 raw filter / 5 confidence
    PAINT_SHOT    截图文件名（默认 painterly_capture）
    PAINT_WARMUP  预热秒数（默认 22）

用法：
    $env:PAINT_STYLE=0; $env:PAINT_SHOT="painterly_off"
    UnrealEditor.exe <uproject> -game -windowed -ResX=1600 -ResY=900 \
        -ExecCmds="py D:/EbbingRainbow/Scripts/Painterly/smoke_test_painterly.py"
"""

import os

import unreal

STYLE = int(os.environ.get("PAINT_STYLE", "1"))
DEBUG_MODE = int(os.environ.get("PAINT_DEBUG", "0"))
SHOT_NAME = os.environ.get("PAINT_SHOT", "painterly_capture")
WARMUP_SECONDS = float(os.environ.get("PAINT_WARMUP", "22"))

# "P_InkStrength=0,P_ImpastoStrength=0" -> override parameters for this run
PARAM_OVERRIDES = []
for _item in os.environ.get("PAINT_PARAMS", "").split(","):
    _item = _item.strip()
    if "=" in _item:
        _name, _value = _item.split("=", 1)
        PARAM_OVERRIDES.append((_name.strip(), _value.strip()))

_phase = [0]
_elapsed = [0.0]


def _log(msg):
    unreal.log("[SmokeTest] " + str(msg))


def _exec(command):
    try:
        unreal.SystemLibrary.execute_console_command(None, command)
    except Exception as exc:  # noqa: BLE001
        _log("exec failed (%s): %s" % (command, exc))


def _cvar(name):
    try:
        return unreal.SystemLibrary.get_console_variable_int_value(name)
    except Exception:  # noqa: BLE001
        return -999


def _tick(delta_seconds):
    # A loading hitch can hand us a multi-second delta; clamp it so the timeline
    # below means what it says.
    delta = min(float(delta_seconds), 0.25)

    if _phase[0] == 0:
        _exec("Paint.Style %d" % STYLE)
        _exec("Paint.DebugMode %d" % DEBUG_MODE)
        for name, value in PARAM_OVERRIDES:
            _exec("Paint.Param %s %s" % (name, value))
        _log("state applied: Paint.Style=%d Paint.DebugMode=%d params=%s (readback %d/%d)" % (
            STYLE, DEBUG_MODE, PARAM_OVERRIDES, _cvar("Paint.Style"), _cvar("Paint.DebugMode")))
        _phase[0] = 1
        return

    _elapsed[0] += delta

    if _phase[0] == 1 and _elapsed[0] >= WARMUP_SECONDS:
        _exec("Paint.Shot %s" % SHOT_NAME)
        _log("screenshot requested: %s (style=%d debug=%d)" % (
            SHOT_NAME, _cvar("Paint.Style"), _cvar("Paint.DebugMode")))
        _phase[0] = 2
        _elapsed[0] = 0.0
    elif _phase[0] == 2 and _elapsed[0] >= 2.0:
        _log("done, quitting")
        _phase[0] = 3
        _exec("quit")


unreal.register_slate_post_tick_callback(_tick)
_log("tick callback registered (style=%d debug=%d shot=%s)" % (STYLE, DEBUG_MODE, SHOT_NAME))
