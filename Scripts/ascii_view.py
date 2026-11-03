"""把截图降采样成 ASCII 色块，便于在纯文本环境里检查 UI 布局。"""
import sys
from PIL import Image


def classify(r, g, b):
    mx, mn = max(r, g, b), min(r, g, b)
    v = mx
    if v < 26:
        return " "
    if v < 55 and mx - mn < 25:
        return "."
    if b > 90 and b >= r + 20 and g >= r:
        return "~" if v < 120 else "≈"
    if r > 140 and g < 110 and b < 90:
        return "R"
    if r > 150 and 90 <= g <= 190 and b < 110:
        return "O"
    if g > 140 and b > 140 and r < 130:
        return "C"
    if r > 200 and g > 200 and b > 200:
        return "#"
    if v > 130:
        return "+"
    if v > 80:
        return ":"
    return ","


def main(path, x0, y0, x1, y1, cols=110):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    box = (int(w * x0), int(h * y0), int(w * x1), int(h * y1))
    crop = im.crop(box)
    cw, ch = crop.size
    rows = max(1, int(cols * ch / cw * 0.5))
    small = crop.resize((cols, rows), Image.BOX)
    px = small.load()
    print(f"image={w}x{h} box={box} crop={cw}x{ch} grid={cols}x{rows}")
    for y in range(rows):
        print("".join(classify(*px[x, y]) for x in range(cols)))


if __name__ == "__main__":
    a = sys.argv
    main(a[1], float(a[2]), float(a[3]), float(a[4]), float(a[5]),
         int(a[6]) if len(a) > 6 else 110)
