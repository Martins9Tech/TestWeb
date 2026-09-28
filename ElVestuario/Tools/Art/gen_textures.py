"""
Genera las texturas procedurales de "El Vestuario" (albedo + normal).

Uso:  python3 gen_textures.py
Salida: ../../SourceArt/Textures/*.png  (1024x1024, tileables)

Solo necesita numpy. Los normal maps salen en convencion DirectX (verde invertido),
que es la que usa Unreal Engine.
"""
import os
import struct
import zlib

import numpy as np

SIZE = 1024
OUT_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "SourceArt", "Textures"))


# ---------------------------------------------------------------------------
# Utilidades
# ---------------------------------------------------------------------------

def write_png(path, img):
    """Escribe un PNG RGB de 8 bits sin dependencias externas. img: float [0,1] HxWx3."""
    data = (np.clip(img, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    h, w, _ = data.shape
    raw = b"".join(b"\x00" + data[y].tobytes() for y in range(h))

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def value_noise(size, cells, rng):
    """Ruido de valor tileable con interpolacion suave."""
    grid = rng.random((cells, cells))
    coords = np.arange(size) * cells / size
    i0 = np.floor(coords).astype(int)
    f = coords - i0
    f = f * f * (3 - 2 * f)
    i1 = (i0 + 1) % cells
    i0 = i0 % cells
    a = grid[np.ix_(i0, i0)]
    b = grid[np.ix_(i0, i1)]
    c = grid[np.ix_(i1, i0)]
    d = grid[np.ix_(i1, i1)]
    fx = f[None, :]
    fy = f[:, None]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(size, base_cells, octaves, rng, gain=0.5):
    total = np.zeros((size, size))
    amp, norm = 1.0, 0.0
    cells = base_cells
    for _ in range(octaves):
        total += value_noise(size, cells, rng) * amp
        norm += amp
        amp *= gain
        cells *= 2
    return total / norm


def normal_from_height(height, strength):
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    n = np.stack([-dx, dy, np.ones_like(height)], axis=-1)  # dy positivo -> DirectX
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def lerp(a, b, t):
    t = t[..., None] if np.ndim(t) == 2 else t
    return a * (1 - t) + b * t


def color(rgb):
    return np.array(rgb, dtype=float)[None, None, :]


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def tile_pattern(size, tiles, grout, rng):
    """Devuelve (mascara_junta, id_por_azulejo, bisel) para una cuadricula de azulejos."""
    coords = np.arange(size) * tiles / size
    u = coords[None, :].repeat(size, 0)
    v = coords[:, None].repeat(size, 1)
    fu, fv = u % 1.0, v % 1.0
    edge = np.minimum(np.minimum(fu, 1 - fu), np.minimum(fv, 1 - fv))
    grout_mask = 1.0 - smoothstep(grout * 0.5, grout, edge)
    bevel = smoothstep(grout, grout * 3.0, edge)
    tile_id = (np.floor(v).astype(int) % tiles) * tiles + (np.floor(u).astype(int) % tiles)
    rand = rng.random(tiles * tiles)
    return grout_mask, rand[tile_id], bevel, tile_id


# ---------------------------------------------------------------------------
# Materiales
# ---------------------------------------------------------------------------

def tiles_material(name, tiles, base, grout_col, variation, grime_col, seed,
                   missing_ratio=0.02, algae=None):
    rng = np.random.default_rng(seed)
    grout_mask, tile_rand, bevel, tile_id = tile_pattern(SIZE, tiles, 0.035, rng)
    grime = fbm(SIZE, 4, 6, rng)
    spots = fbm(SIZE, 8, 5, rng)

    tile_col = color(base) * (1.0 - variation + variation * 2 * tile_rand[..., None])
    stained = tile_rand > 0.93
    tile_col = np.where(stained[..., None], tile_col * 0.72, tile_col)

    alb = lerp(tile_col, color(grout_col), grout_mask)
    grime_mask = smoothstep(0.45, 0.75, grime) * 0.75 + smoothstep(0.6, 0.8, spots) * 0.35
    alb = lerp(alb, color(grime_col), np.clip(grime_mask, 0, 1))
    if algae is not None:
        algae_mask = smoothstep(0.55, 0.8, fbm(SIZE, 3, 5, rng))
        alb = lerp(alb, color(algae), algae_mask * 0.8)

    # Azulejos que faltan: muestran cemento rugoso
    missing = np.isin(tile_id, np.nonzero(rng.random(tiles * tiles) < missing_ratio)[0])
    cement = color((0.33, 0.32, 0.30)) * (0.7 + 0.5 * spots[..., None])
    alb = np.where(missing[..., None], cement, alb)

    height = bevel * 0.8 + fbm(SIZE, 32, 3, rng) * 0.05
    height = np.where(missing, 0.1 + spots * 0.2, height)
    nrm = normal_from_height(height, 6.0)
    return name, alb, nrm


def plaster_material(seed):
    rng = np.random.default_rng(seed)
    base = fbm(SIZE, 4, 7, rng)
    stains = fbm(SIZE, 2, 6, rng)
    peel = fbm(SIZE, 6, 6, rng)
    alb = color((0.74, 0.72, 0.66)) * (0.85 + 0.25 * base[..., None])
    water = smoothstep(0.5, 0.75, stains)
    alb = lerp(alb, color((0.42, 0.36, 0.26)), water * 0.7)
    peel_mask = smoothstep(0.62, 0.66, peel)
    alb = lerp(alb, color((0.45, 0.44, 0.42)), peel_mask)
    mold = smoothstep(0.7, 0.9, fbm(SIZE, 16, 4, rng)) * water
    alb = lerp(alb, color((0.12, 0.14, 0.10)), mold * 0.8)
    height = base * 0.3 - peel_mask * 0.4
    return "T_Plaster", alb, normal_from_height(height, 4.0)


def concrete_material(seed):
    rng = np.random.default_rng(seed)
    base = fbm(SIZE, 4, 8, rng)
    pores = smoothstep(0.75, 0.8, fbm(SIZE, 64, 2, rng))
    alb = color((0.36, 0.35, 0.33)) * (0.75 + 0.45 * base[..., None])
    alb = lerp(alb, color((0.1, 0.1, 0.1)), pores * 0.6)
    wet = smoothstep(0.55, 0.8, fbm(SIZE, 2, 5, rng))
    alb = alb * (1 - 0.35 * wet[..., None])
    return "T_Concrete", alb, normal_from_height(base * 0.4 - pores * 0.3, 5.0)


def locker_paint_material(seed):
    rng = np.random.default_rng(seed)
    base = fbm(SIZE, 4, 6, rng)
    rust_n = fbm(SIZE, 6, 7, rng)
    scratches = np.zeros((SIZE, SIZE))
    for _ in range(140):
        x0, y0 = rng.integers(0, SIZE, 2)
        length = rng.integers(20, 160)
        ang = rng.uniform(-0.4, 0.4) + np.pi / 2 * rng.integers(0, 2)
        t = np.linspace(0, 1, length * 2)
        xs = ((x0 + np.cos(ang) * t * length) % SIZE).astype(int)
        ys = ((y0 + np.sin(ang) * t * length) % SIZE).astype(int)
        scratches[ys, xs] = 1.0
    paint = color((0.20, 0.33, 0.28)) * (0.8 + 0.3 * base[..., None])
    rust = color((0.36, 0.17, 0.07)) * (0.6 + 0.6 * fbm(SIZE, 32, 3, rng)[..., None])
    rust_mask = smoothstep(0.6, 0.68, rust_n)
    alb = lerp(paint, rust, rust_mask)
    alb = lerp(alb, color((0.55, 0.55, 0.52)), scratches * 0.7)
    # Veteado vertical de oxido que chorrea
    streak = fbm(SIZE, 4, 4, rng)
    streak = np.repeat(streak[:1, :], SIZE, axis=0) * np.linspace(0.2, 1.0, SIZE)[:, None]
    alb = lerp(alb, color((0.30, 0.16, 0.08)), smoothstep(0.55, 0.9, streak) * 0.5)
    height = -rust_mask * 0.3 + base * 0.1 - scratches * 0.2
    return "T_LockerPaint", alb, normal_from_height(height, 4.0)


def metal_material(seed):
    rng = np.random.default_rng(seed)
    brushed = fbm(SIZE, 4, 5, rng)
    lines = value_noise(SIZE, 256, rng)
    lines = np.repeat(lines[:, :1], SIZE, axis=1)
    alb = color((0.42, 0.43, 0.44)) * (0.8 + 0.2 * brushed[..., None] + 0.08 * lines[..., None])
    rust_mask = smoothstep(0.62, 0.72, fbm(SIZE, 5, 7, rng))
    alb = lerp(alb, color((0.33, 0.16, 0.07)), rust_mask)
    return "T_Metal", alb, normal_from_height(lines * 0.1 - rust_mask * 0.3, 3.0)


def wood_material(seed):
    rng = np.random.default_rng(seed)
    warp = fbm(SIZE, 4, 5, rng)
    ys = np.arange(SIZE)[:, None] / SIZE
    rings = np.sin((ys * 60 + warp * 6) * np.pi * 2) * 0.5 + 0.5
    rings = np.repeat(rings, SIZE, axis=1) if rings.shape[1] == 1 else rings
    grain = fbm(SIZE, 64, 3, rng)
    alb = lerp(color((0.33, 0.21, 0.12)), color((0.20, 0.12, 0.07)), rings * 0.7 + grain * 0.3)
    rot = smoothstep(0.62, 0.8, fbm(SIZE, 3, 6, rng))
    alb = lerp(alb, color((0.12, 0.11, 0.08)), rot * 0.8)
    return "T_Wood", alb, normal_from_height(rings * 0.2 + grain * 0.1 - rot * 0.2, 3.0)


def skin_material(seed):
    rng = np.random.default_rng(seed)
    mottled = fbm(SIZE, 6, 7, rng)
    veins_n = fbm(SIZE, 8, 6, rng)
    veins = 1.0 - smoothstep(0.0, 0.025, np.abs(veins_n - 0.5))
    alb = color((0.62, 0.60, 0.56)) * (0.75 + 0.35 * mottled[..., None])
    alb = lerp(alb, color((0.30, 0.34, 0.38)), veins * 0.6)
    bruises = smoothstep(0.6, 0.8, fbm(SIZE, 3, 5, rng))
    alb = lerp(alb, color((0.35, 0.30, 0.32)), bruises * 0.5)
    return "T_Skin", alb, normal_from_height(mottled * 0.3 + veins * 0.15, 3.0)


def paper_material(seed):
    rng = np.random.default_rng(seed)
    base = fbm(SIZE, 4, 7, rng)
    stains = smoothstep(0.55, 0.8, fbm(SIZE, 2, 5, rng))
    alb = color((0.80, 0.76, 0.64)) * (0.9 + 0.12 * base[..., None])
    alb = lerp(alb, color((0.55, 0.45, 0.28)), stains * 0.6)
    return "T_Paper", alb, normal_from_height(base * 0.1, 2.0)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    materials = [
        tiles_material("T_WallTile", 8, (0.78, 0.80, 0.74), (0.30, 0.30, 0.28), 0.06,
                       (0.30, 0.28, 0.20), seed=11, missing_ratio=0.03),
        tiles_material("T_FloorTile", 12, (0.30, 0.34, 0.32), (0.12, 0.12, 0.11), 0.10,
                       (0.10, 0.09, 0.07), seed=12, missing_ratio=0.01),
        tiles_material("T_PoolTile", 12, (0.28, 0.55, 0.60), (0.20, 0.25, 0.25), 0.08,
                       (0.15, 0.18, 0.12), seed=13, missing_ratio=0.02, algae=(0.14, 0.22, 0.10)),
        plaster_material(21),
        concrete_material(22),
        locker_paint_material(23),
        metal_material(24),
        wood_material(25),
        skin_material(26),
        paper_material(27),
    ]
    for name, alb, nrm in materials:
        write_png(os.path.join(OUT_DIR, f"{name}_D.png"), alb)
        write_png(os.path.join(OUT_DIR, f"{name}_N.png"), nrm)
        print("ok", name)


if __name__ == "__main__":
    main()
