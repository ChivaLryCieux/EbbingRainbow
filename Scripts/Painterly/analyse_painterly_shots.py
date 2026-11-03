# -*- coding: utf-8 -*-
"""
analyse_painterly_shots.py
==========================

对冒烟测试截出来的图做量化对比，并生成：

    Docs/painterly_compare.png    左原图 / 右厚涂
    Docs/painterly_detail.png     局部 3x 放大对比（看笔触）
    Docs/painterly_diff.png       差异热力图

指标含义（用来确认滤镜真的生效、而且不是单纯变糊）：

    colors  唯一色数（5bit 量化后） -> 厚涂应显著减少（色块化）
    hf      1px 尺度梯度能量        -> 应小幅变化，暴涨说明噪点/锯齿
    dog     4px 尺度带通能量        -> 笔触/堆料纹理，应上升
    mean    平均亮度                -> 不应大幅漂移

另外把 on/off 各打一张 ASCII 缩略图，方便在没有图形界面的环境里判断构图。
"""

import os
import sys

import numpy as np
from PIL import Image

SHOT_DIR = r"D:\EbbingRainbow\Saved\Screenshots"
DOC_DIR = r"D:\EbbingRainbow\Docs"


def find_shot(name):
    for root, _dirs, files in os.walk(SHOT_DIR):
        for f in files:
            if f.lower() == name.lower():
                return os.path.join(root, f)
    return None


def load(path):
    img = Image.open(path).convert("RGB")
    return np.asarray(img).astype(np.float32) / 255.0


def luma(rgb):
    return rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)


def sobel_energy(gray):
    gx = np.zeros_like(gray)
    gy = np.zeros_like(gray)
    gx[:, 1:-1] = gray[:, 2:] - gray[:, :-2]
    gy[1:-1, :] = gray[2:, :] - gray[:-2, :]
    return float(np.mean(np.sqrt(gx * gx + gy * gy)))


def dog_energy(gray, radius=2):
    """4px 尺度带通能量：笔触 / 堆料纹理落在这个频段。"""
    k = radius
    pad = np.pad(gray, k, mode="edge")
    acc = np.zeros_like(gray)
    count = 0
    for dy in range(-k, k + 1):
        for dx in range(-k, k + 1):
            acc += pad[k + dy:k + dy + gray.shape[0], k + dx:k + dx + gray.shape[1]]
            count += 1
    return float(np.mean(np.abs(gray - acc / count)))


def unique_colors(rgb, bits=5):
    q = np.floor(rgb * ((1 << bits) - 1) + 0.5).astype(np.int32)
    packed = (q[..., 0] << (2 * bits)) | (q[..., 1] << bits) | q[..., 2]
    return int(np.unique(packed).size)


def report(name, rgb):
    g = luma(rgb)
    stats = {
        "mean": float(g.mean()),
        "std": float(g.std()),
        "colors": unique_colors(rgb),
        "hf": sobel_energy(g),
        "dog": dog_energy(g),
    }
    print("%-22s mean %.4f  contrast %.4f  colors %6d  hf %.5f  brush %.5f" % (
        name, stats["mean"], stats["std"], stats["colors"], stats["hf"], stats["dog"]))
    return stats


def ascii_preview(gray, cols=64, rows=22):
    ramp = " .:-=+*#%@"
    h, w = gray.shape
    lines = []
    for r in range(rows):
        row = ""
        for c in range(cols):
            block = gray[int(r * h / rows):int((r + 1) * h / rows),
                         int(c * w / cols):int((c + 1) * w / cols)]
            value = float(block.mean())
            row += ramp[min(len(ramp) - 1, int(value * (len(ramp) - 1) + 0.5))]
        lines.append(row)
    return "\n".join(lines)


def main():
    off_path = find_shot("painterly_off.png")
    on_path = find_shot("painterly_on.png")
    if off_path is None or on_path is None:
        print("Screenshots missing - run Scripts/Painterly/smoke_test_painterly.py first.")
        print("off=%s on=%s" % (off_path, on_path))
        return 1

    off = load(off_path)
    on = load(on_path)
    print("off: %s" % off_path)
    print("on : %s" % on_path)
    print("")

    off_stats = report("off (original)", off)
    on_stats = report("on  (painterly)", on)

    # 其它调试视图（存在就一并统计）
    for debug_name in ("painterly_dbg_filter", "painterly_dbg_ink",
                       "painterly_dbg_normal", "painterly_dbg_flow"):
        path = find_shot(debug_name + ".png")
        if path:
            report(debug_name, load(path))

    h = min(off.shape[0], on.shape[0])
    w = min(off.shape[1], on.shape[1])
    off = off[:h, :w]
    on = on[:h, :w]

    os.makedirs(DOC_DIR, exist_ok=True)

    gap = np.ones((h, 8, 3), dtype=np.float32)
    side = np.concatenate([off, gap, on], axis=1)
    Image.fromarray((np.clip(side, 0, 1) * 255).astype(np.uint8)).save(
        os.path.join(DOC_DIR, "painterly_compare.png"))

    cw, ch = w // 3, h // 3
    x0, y0 = (w - cw) // 2, max(0, (h - ch) // 3)
    crop_off = off[y0:y0 + ch, x0:x0 + cw]
    crop_on = on[y0:y0 + ch, x0:x0 + cw]
    zoom = 3
    crop_off = np.asarray(Image.fromarray((crop_off * 255).astype(np.uint8)).resize(
        (cw * zoom, ch * zoom), Image.NEAREST)).astype(np.float32) / 255.0
    crop_on = np.asarray(Image.fromarray((crop_on * 255).astype(np.uint8)).resize(
        (cw * zoom, ch * zoom), Image.NEAREST)).astype(np.float32) / 255.0
    gap2 = np.ones((ch * zoom, 12, 3), dtype=np.float32)
    Image.fromarray((np.clip(np.concatenate([crop_off, gap2, crop_on], axis=1), 0, 1) * 255)
                    .astype(np.uint8)).save(os.path.join(DOC_DIR, "painterly_detail.png"))

    diff = np.abs(on - off).mean(axis=2)
    diff = diff / max(diff.max(), 1e-6)
    heat = np.stack([diff, diff * 0.35, 1.0 - diff], axis=2)
    Image.fromarray((np.clip(heat, 0, 1) * 255).astype(np.uint8)).save(
        os.path.join(DOC_DIR, "painterly_diff.png"))

    print("")
    print("colors  %+d%%" % round((on_stats["colors"] / max(off_stats["colors"], 1) - 1) * 100))
    print("hf      %+d%%" % round((on_stats["hf"] / max(off_stats["hf"], 1e-6) - 1) * 100))
    print("brush   %+d%%" % round((on_stats["dog"] / max(off_stats["dog"], 1e-6) - 1) * 100))
    print("mean    %+d%%" % round((on_stats["mean"] / max(off_stats["mean"], 1e-6) - 1) * 100))
    print("")
    print("--- off preview ---")
    print(ascii_preview(luma(off)))
    print("--- on preview ---")
    print(ascii_preview(luma(on)))
    print("")
    print("wrote painterly_compare.png / painterly_detail.png / painterly_diff.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
