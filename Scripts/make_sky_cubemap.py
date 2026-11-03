"""Generate a 6-face DDS cubemap of the sky gradient for the SkyLight ambient.

The colours are written sRGB-encoded so that the imported TextureCube samples
back to the same linear values the sky dome material emits.
"""
import struct
import sys

SIZE = 256
ZENITH = (0.050, 0.080, 0.160)
HORIZON = (0.720, 0.750, 0.735)
GROUND = (0.090, 0.100, 0.105)
POWER = 0.80


def enc(lin):
    return max(0, min(255, int(round((max(lin, 0.0) ** (1.0 / 2.2)) * 255.0))))


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def shade(dx, dy, dz):
    z = max(-1.0, min(1.0, dz))
    if z >= 0.0:
        c = lerp(HORIZON, ZENITH, z ** POWER)
    else:
        c = lerp(HORIZON, GROUND, (-z) ** 0.55)
    return (enc(c[0]), enc(c[1]), enc(c[2]), 255)


def face_pixels(face):
    px = bytearray()
    for y in range(SIZE):
        t = 2.0 * (y + 0.5) / SIZE - 1.0
        for x in range(SIZE):
            s = 2.0 * (x + 0.5) / SIZE - 1.0
            if face == 0:
                d = (1.0, -t, -s)
            elif face == 1:
                d = (-1.0, -t, s)
            elif face == 2:
                d = (s, 1.0, t)
            elif face == 3:
                d = (s, -1.0, -t)
            elif face == 4:
                d = (s, -t, 1.0)
            else:
                d = (-s, -t, -1.0)
            n = (d[0] ** 2 + d[1] ** 2 + d[2] ** 2) ** 0.5
            r, g, b, a = shade(d[0] / n, d[1] / n, d[2] / n)
            px += bytes((b, g, r, a))  # BGRA order for A8R8G8B8
    return px


def build(path):
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000  # CAPS|HEIGHT|WIDTH|PIXELFORMAT|LINEARSIZE
    header = bytearray()
    header += b"DDS "
    header += struct.pack("<7I", 124, flags, SIZE, SIZE, SIZE * SIZE * 4, 0, 1)
    header += b"\x00" * 44  # reserved1[11]
    header += struct.pack("<2I", 32, 0x41)  # pf size, RGB|ALPHAPIXELS
    header += struct.pack("<4I", 0, 32, 0x00FF0000, 0x0000FF00)
    header += struct.pack("<2I", 0x000000FF, 0xFF000000)
    header += struct.pack("<5I", 0x1000 | 0x8, 0x200 | 0xFC00, 0, 0, 0)
    assert len(header) == 128, len(header)
    with open(path, "wb") as f:
        f.write(header)
        for face in range(6):
            f.write(face_pixels(face))
    print("wrote", path)


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else r"D:\EbbingRainbow\Scripts\T_SkyAmbient.dds")
