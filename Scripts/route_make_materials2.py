"""Route scene shader set (all unlit, built procedurally so no texture assets are needed).

  M_Route_Nebula    additive fbm cloud with slow drift (haze / aurora pools)
  M_Route_Rainbow   one-plane spectral arc with shimmer (the "ebbing rainbow")
  M_Route_Rays      god rays: soft across width, noisy streaks along length, slow drift
  M_Route_Halo      node halo: glow + thin ring + rotating rays (current / available / final nodes)

Run inside the editor:  python -m steeragents py D:/EbbingRainbow/Scripts/route_make_materials2.py
(or UnrealEditor-Cmd -run=pythonscript -script=...)
"""
import unreal

PKG = "/Game/Run/Materials"
mel = unreal.MaterialEditingLibrary
E = unreal.MaterialExpression


def get_or_create(name):
    path = f"{PKG}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, PKG, unreal.Material, unreal.MaterialFactoryNew())


class Graph:
    def __init__(self, name, blend, two_sided=True, niagara=False):
        self.m = get_or_create(name)
        self.name = name
        self.x = -1400
        self.y = 0
        self.m.set_editor_property("blend_mode", blend)
        self.m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        self.m.set_editor_property("two_sided", two_sided)
        self.m.set_editor_property("used_with_instanced_static_meshes", True)
        if niagara:
            self.m.set_editor_property("used_with_niagara_sprites", True)

    def _place(self):
        self.y += 90
        if self.y > 1400:
            self.y = 0
            self.x += 260
        return self.x, self.y

    def node(self, cls, **props):
        x, y = self._place()
        n = mel.create_material_expression(self.m, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, a, b, pin="", out=""):
        ok = mel.connect_material_expressions(a, out, b, pin)
        if not ok:
            print("LINK FAIL", self.name, type(a).__name__, "->", type(b).__name__, pin)

    def wire(self, cls, inputs, **props):
        n = self.node(cls, **props)
        for pin, src in inputs.items():
            if isinstance(src, (int, float)):
                src = self.c(src)
            if isinstance(src, tuple):
                self.link(src[0], n, pin, src[1])
            else:
                self.link(src, n, pin)
        return n

    # --- leaves
    def c(self, v):
        return self.node(unreal.MaterialExpressionConstant, r=float(v))

    def sp(self, name, default):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=float(default))

    def vp(self, name, rgba):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(*rgba))

    def uv(self):
        return self.node(unreal.MaterialExpressionTextureCoordinate)

    def time(self):
        return self.node(unreal.MaterialExpressionTime)

    # --- ops (single-input ones use the unnamed pin)
    def add(self, a, b): return self.wire(unreal.MaterialExpressionAdd, {"A": a, "B": b})
    def sub(self, a, b): return self.wire(unreal.MaterialExpressionSubtract, {"A": a, "B": b})
    def mul(self, a, b): return self.wire(unreal.MaterialExpressionMultiply, {"A": a, "B": b})
    def div(self, a, b): return self.wire(unreal.MaterialExpressionDivide, {"A": a, "B": b})
    def pw(self, a, e): return self.wire(unreal.MaterialExpressionPower, {"Base": a, "Exp": e})
    def lerp(self, a, b, t): return self.wire(unreal.MaterialExpressionLinearInterpolate, {"A": a, "B": b, "Alpha": t})
    def dot(self, a, b): return self.wire(unreal.MaterialExpressionDotProduct, {"A": a, "B": b})
    def app(self, a, b): return self.wire(unreal.MaterialExpressionAppendVector, {"A": a, "B": b})
    def one(self, a): return self.wire(unreal.MaterialExpressionOneMinus, {"": a})
    def sat(self, a): return self.wire(unreal.MaterialExpressionSaturate, {"": a})
    def abs(self, a): return self.wire(unreal.MaterialExpressionAbs, {"": a})
    def sqrt(self, a): return self.wire(unreal.MaterialExpressionSquareRoot, {"": a})
    def sin(self, a): return self.wire(unreal.MaterialExpressionSine, {"": a}, period=6.2831853)
    def cos(self, a): return self.wire(unreal.MaterialExpressionCosine, {"": a}, period=6.2831853)
    def frac(self, a): return self.wire(unreal.MaterialExpressionFrac, {"": a})
    def atan2(self, y, x): return self.wire(unreal.MaterialExpressionArctangent2, {"Y": y, "X": x})
    def smooth(self, a, lo, hi):
        t = self.sat(self.div(self.sub(a, lo), self.sub(hi, lo)))
        return self.mul(t, self.mul(t, self.sub(3.0, self.mul(2.0, t))))

    def mask(self, a, r=False, g=False, b=False, al=False):
        return self.wire(unreal.MaterialExpressionComponentMask, {"": a}, r=r, g=g, b=b, a=al)

    def length2(self, p):
        return self.sqrt(self.dot(p, p))

    def noise(self, pos, scale=1.0, levels=4, turbulence=True, lo=0.0, hi=1.0, dim3=False):
        if not dim3:
            pos = self.app(pos, self.c(0.0))
        return self.wire(unreal.MaterialExpressionNoise, {"World Position": pos},
                         scale=scale, levels=levels, turbulence=turbulence, output_min=lo, output_max=hi,
                         noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU,
                         level_scale=2.0, quality=1)

    def finish(self, emissive, opacity=None):
        mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if opacity is not None:
            mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
        mel.recompile_material(self.m)
        unreal.EditorAssetLibrary.save_asset(f"{PKG}/{self.name}")
        print("built", self.name)

    # --- shared pieces
    def centered_uv(self):
        """(uv-0.5)*2 as float2"""
        return self.mul(self.sub(self.uv(), 0.5), 2.0)


ADD = unreal.BlendMode.BLEND_ADDITIVE
OPQ = unreal.BlendMode.BLEND_OPAQUE


def build_nebula():
    g = Graph("M_Route_Nebula", ADD)
    p = g.centered_uv()
    t = g.time()
    drift = g.app(g.mul(t, g.sp("SpeedX", 0.010)), g.mul(t, g.sp("SpeedY", 0.004)))
    pos = g.app(g.add(g.mul(g.uv(), g.sp("NoiseScale", 3.0)), drift), g.sp("Seed", 0.0))
    n1 = g.noise(pos, scale=1.0, levels=5)
    pos2 = g.app(g.add(g.mul(g.uv(), g.sp("NoiseScale", 3.0)), g.mul(drift, -1.7)), g.add(g.sp("Seed", 0.0), 7.3))
    n2 = g.noise(pos2, scale=1.7, levels=4)
    cloud = g.pw(g.sat(g.add(g.mul(n1, 1.25), g.mul(n2, 0.35))), g.sp("Contrast", 2.2))
    # elliptical falloff (ScaleU/ScaleV let one plane become a wide band)
    q = g.mul(p, g.app(g.sp("ScaleU", 1.0), g.sp("ScaleV", 1.0)))
    fall = g.pw(g.sat(g.one(g.length2(q))), g.sp("Power", 1.6))
    col = g.lerp(g.vp("ColorA", (0.10, 0.22, 0.50, 1)), g.vp("ColorB", (0.45, 0.30, 0.75, 1)), g.sat(g.mul(n2, 1.4)))
    base = g.lerp(1.0, cloud, g.sp("CloudAmount", 0.85))
    g.finish(g.mul(g.mul(col, g.mul(fall, base)), g.sp("Intensity", 1.0)))


def build_rainbow():
    g = Graph("M_Route_Rainbow", ADD)
    t = g.time()
    u = g.mask(g.uv(), r=True)
    v = g.mask(g.uv(), g=True)
    px = g.mul(g.sub(u, 0.5), g.sp("Aspect", 1.78))
    py = g.sub(v, g.sp("CenterV", 1.6))
    d = g.length2(g.app(px, py))
    w = g.sp("Width", 0.075)
    tt = g.add(g.div(g.sub(d, g.sp("Radius", 1.0)), w), 0.5)          # 0..1 across the band (0 = inner)
    band = g.pw(g.sat(g.one(g.abs(g.sub(g.mul(tt, 2.0), 1.0)))), g.sp("Softness", 1.4))
    # cosine palette: violet inside -> red outside, pastel
    phase = g.wire(unreal.MaterialExpressionAppendVector, {"A": g.app(g.c(0.0), g.c(0.33)), "B": g.c(0.67)})
    ph = g.mul(g.add(g.mul(g.sat(tt), 0.78), phase), 6.2831853)
    spec = g.add(g.mul(g.cos(g.sub(0.0, ph)), 0.5), 0.5)
    spec = g.lerp(spec, g.vp("Tint", (1.0, 0.97, 0.9, 1)), g.sp("Pastel", 0.30))
    # fade along the arc (stronger near the apex) + slow shimmer
    arc = g.pw(g.sat(g.one(g.abs(g.div(px, g.sp("ArcHalf", 1.6))))), g.sp("ArcPower", 0.7))
    shim_pos = g.app(g.add(g.mul(u, 5.0), g.mul(t, 0.02)), g.add(g.mul(d, 9.0), g.mul(t, -0.03)))
    shim = g.lerp(0.7, 1.15, g.noise(shim_pos, scale=1.0, levels=3))
    g.finish(g.mul(g.mul(spec, g.mul(band, g.mul(arc, shim))), g.sp("Intensity", 0.12)))


def build_rays():
    g = Graph("M_Route_Rays", ADD)
    t = g.time()
    u = g.mask(g.uv(), r=True)
    v = g.mask(g.uv(), g=True)
    across = g.pw(g.sat(g.one(g.abs(g.sub(g.mul(u, 2.0), 1.0)))), g.sp("Power", 1.6))
    along = g.pw(g.sat(g.one(g.abs(g.sub(g.mul(v, 2.0), 1.0)))), 0.8)
    pos = g.app(g.mul(u, g.sp("StreakFreq", 7.0)), g.add(g.mul(v, 0.35), g.mul(t, g.sp("Speed", 0.015))))
    streak = g.pw(g.sat(g.noise(pos, scale=1.0, levels=3)), 1.7)
    streak = g.lerp(0.35, 1.5, streak)
    g.finish(g.mul(g.mul(g.vp("Color", (0.55, 0.75, 1.0, 1)), g.mul(g.mul(across, along), streak)), g.sp("Intensity", 0.1)))


def build_halo():
    """current-node halo: soft glow + thin ring + slowly rotating soft rays + gentle shader-side breathing
    (breathing / rotation both live on the GPU Time node, so they stay perfectly smooth - no CPU param stepping)"""
    g = Graph("M_Route_Halo", ADD)
    t = g.time()
    p = g.centered_uv()
    px = g.mask(p, r=True)
    py = g.mask(p, g=True)
    r = g.length2(p)
    glow = g.pw(g.sat(g.one(r)), g.sp("GlowPower", 2.2))
    ring_r = g.sp("RingRadius", 0.55)
    x = g.div(g.sub(r, ring_r), g.sp("RingWidth", 0.012))
    ring = g.wire(unreal.MaterialExpressionPower, {"Base": g.c(2.71828), "Exp": g.sub(0.0, g.mul(x, x))})
    ang = g.atan2(py, px)
    wave = g.sat(g.add(g.mul(g.cos(g.add(g.mul(ang, g.sp("RayCount", 10.0)), g.mul(t, g.sp("Spin", 0.12)))), 0.5), 0.5))
    rays = g.mul(g.pw(wave, g.sp("RayPower", 3.0)), g.pw(g.sat(g.one(r)), 1.6))
    hole = g.smooth(r, g.sp("HoleInner", 0.30), g.sp("HoleOuter", 0.45))
    total = g.add(g.add(glow, g.mul(ring, g.sp("RingIntensity", 0.55))), g.mul(g.mul(rays, hole), g.sp("RayIntensity", 0.65)))
    breath = g.add(1.0, g.mul(g.sin(g.mul(t, g.sp("BreathSpeed", 0.9))), g.sp("BreathAmount", 0.10)))
    g.finish(g.mul(g.mul(g.mul(g.vp("Color", (1, 0.8, 0.35, 1)), total), g.sp("Intensity", 1.0)), breath))

if __name__ == "__main__" or True:
    build_nebula()
    build_rainbow()
    build_rays()
    build_halo()


