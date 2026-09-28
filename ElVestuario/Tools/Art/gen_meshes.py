"""
Genera todos los modelos 3D de "El Vestuario" y los exporta a FBX para Unreal.

Uso (cualquiera de las dos):
    blender --background --python gen_meshes.py            (Blender 4.x / 5.x instalado)
    python3 gen_meshes.py                                   (con el modulo 'bpy' de pip)
Opciones:
    --preview   ademas renderiza imagenes de previsualizacion en ../../Docs/

Requisitos: haber ejecutado antes gen_textures.py (las texturas se referencian por ruta relativa).

Convenciones (para que Unreal lo importe bien):
  * 1 unidad de Blender = 1 metro  ->  100 cm en Unreal.
  * Cada asset mira hacia +X y es simetrico en Y (Unreal invierte el eje Y al importar).
  * La colision va en objetos UCX_<NombreAsset>_NN (cajas convexas).
"""
import json
import math
import os
import sys

import bpy  # debe importarse antes que bmesh
import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TEX_DIR = os.path.join(ROOT, "SourceArt", "Textures")
MESH_DIR = os.path.join(ROOT, "SourceArt", "Meshes")
DOCS_DIR = os.path.join(ROOT, "Docs")
with open(os.path.join(HERE, "..", "layout.json"), encoding="utf-8") as fh:
    LAYOUT = json.load(fh)

# Tamano en metros que cubre cada textura (para el mapeado UV en caja)
UV_SIZE = {
    "M_WallTile": 1.2, "M_FloorTile": 1.2, "M_PoolTile": 1.2, "M_Plaster": 2.0,
    "M_Concrete": 2.0, "M_LockerPaint": 1.0, "M_Metal": 1.0, "M_Wood": 1.0,
    "M_Skin": 0.5, "M_Paper": 0.4,
}

# Punto de union de la cabeza del enemigo respecto a sus pies (metros). Debe coincidir con
# EnemyCharacter.cpp (HeadAttachOffset = 30, 0, 195 cm).
ENEMY_HEAD_ATTACH = Vector((0.30, 0.0, 1.95))


# ---------------------------------------------------------------------------
# Materiales
# ---------------------------------------------------------------------------

MATERIALS = {}


def _principled(mat):
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None:
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        out = next((n for n in nodes if n.type == "OUTPUT_MATERIAL"), None) or nodes.new("ShaderNodeOutputMaterial")
        mat.node_tree.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return bsdf


def textured_material(name, tex, roughness=0.6, metallic=0.0):
    mat = bpy.data.materials.new(name)
    bsdf = _principled(mat)
    nt = mat.node_tree
    img = bpy.data.images.load(os.path.join(TEX_DIR, tex + "_D.png"), check_existing=True)
    tnode = nt.nodes.new("ShaderNodeTexImage")
    tnode.image = img
    nt.links.new(tnode.outputs["Color"], bsdf.inputs["Base Color"])
    nimg = bpy.data.images.load(os.path.join(TEX_DIR, tex + "_N.png"), check_existing=True)
    nimg.colorspace_settings.name = "Non-Color"
    nnode = nt.nodes.new("ShaderNodeTexImage")
    nnode.image = nimg
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(nnode.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    MATERIALS[name] = mat
    return mat


def flat_material(name, rgb, roughness=0.5, metallic=0.0, emission=None):
    mat = bpy.data.materials.new(name)
    bsdf = _principled(mat)
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission is not None:
        key = "Emission Color" if "Emission Color" in bsdf.inputs else "Emission"
        bsdf.inputs[key].default_value = (*emission, 1.0)
        if "Emission Strength" in bsdf.inputs:
            bsdf.inputs["Emission Strength"].default_value = 3.0
    mat.diffuse_color = (*rgb, 1.0)
    MATERIALS[name] = mat
    return mat


def create_materials():
    textured_material("M_WallTile", "T_WallTile", 0.35)
    textured_material("M_FloorTile", "T_FloorTile", 0.4)
    textured_material("M_PoolTile", "T_PoolTile", 0.3)
    textured_material("M_Plaster", "T_Plaster", 0.9)
    textured_material("M_Concrete", "T_Concrete", 0.85)
    textured_material("M_LockerPaint", "T_LockerPaint", 0.55, 0.2)
    textured_material("M_Metal", "T_Metal", 0.45, 0.8)
    textured_material("M_Wood", "T_Wood", 0.7)
    textured_material("M_Skin", "T_Skin", 0.35)
    textured_material("M_Paper", "T_Paper", 0.9)
    flat_material("M_Socket", (0.05, 0.05, 0.05), 0.8)
    flat_material("M_Ceramic", (0.75, 0.73, 0.68), 0.3)
    flat_material("M_Warning", (0.85, 0.65, 0.05), 0.6)
    flat_material("M_ExitSign", (0.05, 0.45, 0.12), 0.4, emission=(0.05, 0.6, 0.15))
    flat_material("M_LampTube", (0.9, 0.92, 0.95), 0.2)
    flat_material("M_Fuse_Red", (0.60, 0.05, 0.04), 0.4)
    flat_material("M_Fuse_Blue", (0.05, 0.15, 0.60), 0.4)
    flat_material("M_Fuse_Green", (0.05, 0.45, 0.10), 0.4)
    flat_material("M_Cloth", (0.33, 0.30, 0.24), 0.95)
    flat_material("M_Mouth", (0.01, 0.0, 0.0), 0.2)


# ---------------------------------------------------------------------------
# Constructor de assets
# ---------------------------------------------------------------------------

class Asset:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.mats = []
        self.collisions = []

    def _mat(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def _tag(self, faces, mat, smooth):
        idx = self._mat(mat)
        for f in faces:
            f.material_index = idx
            f.smooth = smooth
        return faces

    def box(self, mn, mx, mat, collide=False, matrix=None, smooth=False):
        x0, y0, z0 = mn
        x1, y1, z1 = mx
        bm = self.bm
        v = [bm.verts.new(c) for c in [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                                        (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]]
        idx = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        faces = [bm.faces.new([v[i] for i in f]) for f in idx]
        if matrix is not None:
            bmesh.ops.transform(bm, matrix=matrix, verts=v)
        if collide:
            self.collisions.append((Vector(mn), Vector(mx), matrix))
        return self._tag(faces, mat, smooth)

    def cylinder(self, p0, p1, r0, mat, r1=None, segments=16, caps=True, smooth=True):
        p0, p1 = Vector(p0), Vector(p1)
        direction = p1 - p0
        length = direction.length
        rot = direction.normalized().to_track_quat("Z", "Y").to_matrix().to_4x4()
        m = Matrix.Translation((p0 + p1) * 0.5) @ rot
        res = bmesh.ops.create_cone(self.bm, cap_ends=caps, cap_tris=False, segments=segments,
                                    radius1=r0, radius2=r1 if r1 is not None else r0,
                                    depth=length, matrix=m)
        faces = {f for vert in res["verts"] for f in vert.link_faces}
        return self._tag(faces, mat, smooth)

    def mesh(self, mesh_data, mat, matrix=None, smooth=True):
        before = set(self.bm.faces)
        self.bm.from_mesh(mesh_data)
        new_faces = [f for f in self.bm.faces if f not in before]
        if matrix is not None:
            verts = {v for f in new_faces for v in f.verts}
            bmesh.ops.transform(self.bm, matrix=matrix, verts=list(verts))
        return self._tag(new_faces, mat, smooth)

    def col_box(self, mn, mx, matrix=None):
        self.collisions.append((Vector(mn), Vector(mx), matrix))

    def _box_uvs(self):
        self.bm.normal_update()
        for f in self.bm.faces:
            size = UV_SIZE.get(self.mats[f.material_index].name, 1.0)
            n = f.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            for loop in f.loops:
                co = loop.vert.co
                if ax == 0:
                    uv = (co.y * (1 if n.x > 0 else -1), co.z)
                elif ax == 1:
                    uv = (co.x * (-1 if n.y > 0 else 1), co.z)
                else:
                    uv = (co.x, co.y)
                loop[self.uv].uv = (uv[0] / size, uv[1] / size)

    def build(self):
        self._box_uvs()
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(m)
        obj = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(obj)
        cols = []
        for i, (mn, mx, matrix) in enumerate(self.collisions):
            cbm = bmesh.new()
            x0, y0, z0 = mn
            x1, y1, z1 = mx
            v = [cbm.verts.new(c) for c in [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                                             (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]]
            for f in [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]:
                cbm.faces.new([v[j] for j in f])
            if matrix is not None:
                bmesh.ops.transform(cbm, matrix=matrix, verts=v)
            cme = bpy.data.meshes.new(f"UCX_{self.name}_{i:02d}")
            cbm.to_mesh(cme)
            cbm.free()
            cobj = bpy.data.objects.new(f"UCX_{self.name}_{i:02d}", cme)
            bpy.context.scene.collection.objects.link(cobj)
            cols.append(cobj)
        return obj, cols


def ue(x, y, z):
    """Convierte cm de Unreal a metros de Blender (Y invertida)."""
    return (x / 100.0, -y / 100.0, z / 100.0)


def ue_box(asset, a, b, mat, collide=True):
    pa, pb = ue(*a), ue(*b)
    mn = tuple(min(pa[i], pb[i]) for i in range(3))
    mx = tuple(max(pa[i], pb[i]) for i in range(3))
    asset.box(mn, mx, mat, collide=collide)


M = MATERIALS  # atajo


# ---------------------------------------------------------------------------
# Assets
# ---------------------------------------------------------------------------

def build_room():
    a = Asset("SM_Room")
    floor, wall, plaster, pool, conc = M["M_FloorTile"], M["M_WallTile"], M["M_Plaster"], M["M_PoolTile"], M["M_Concrete"]
    # Suelo alrededor de la piscina (x 550..1050, y +-225)
    ue_box(a, (0, -500, -20), (550, 500, 0), floor)
    ue_box(a, (1050, -500, -20), (1600, 500, 0), floor)
    ue_box(a, (550, 225, -20), (1050, 500, 0), floor)
    ue_box(a, (550, -500, -20), (1050, -225, 0), floor)
    # Piscina vacia (profundidad 120)
    ue_box(a, (550, -225, -140), (1050, 225, -120), pool)
    ue_box(a, (530, -245, -140), (550, 245, -20), pool)
    ue_box(a, (1050, -245, -140), (1070, 245, -20), pool)
    ue_box(a, (550, 225, -140), (1050, 245, -20), pool)
    ue_box(a, (550, -245, -140), (1050, -225, -20), pool)
    # Paredes: azulejo hasta 2 m, yeso por encima
    for (z0, z1, mat) in [(0, 200, wall), (200, 380, plaster)]:
        ue_box(a, (0, 500, z0), (1600, 520, z1), mat)        # norte
        ue_box(a, (0, -520, z0), (1600, -500, z1), mat)      # sur
        ue_box(a, (1600, -520, z0), (1620, 520, z1), mat)    # este (duchas)
        ue_box(a, (-20, 60, z0), (0, 520, z1), mat)          # oeste, lado derecho de la puerta
        ue_box(a, (-20, -520, z0), (0, -60, z1), mat)        # oeste, lado izquierdo
    ue_box(a, (-20, -60, 220), (0, 60, 380), plaster)        # dintel de la puerta
    ue_box(a, (-20, -520, 380), (1620, 520, 400), plaster)   # techo
    # Pasillo de salida (x -520..0)
    ue_box(a, (-520, -110, -20), (0, 110, 0), conc)
    ue_box(a, (-520, 110, 0), (-20, 130, 260), conc)
    ue_box(a, (-520, -130, 0), (-20, -110, 260), conc)
    ue_box(a, (-520, -130, 260), (-20, 130, 280), conc)
    ue_box(a, (-540, -130, 0), (-520, 130, 260), conc)
    # Marco de la puerta
    metal = M["M_Metal"]
    ue_box(a, (-2, 60, 0), (4, 68, 228), metal, collide=False)
    ue_box(a, (-2, -68, 0), (4, -60, 228), metal, collide=False)
    ue_box(a, (-2, -68, 220), (4, 68, 228), metal, collide=False)
    return a.build()


def locker_body(a, cx=0.0, cy=0.0, with_door=False):
    """Taquilla de 0.5 (X) x 0.55 (Y) x 1.9 (Z), frente en +X, abierta por delante."""
    paint = M["M_LockerPaint"]
    t = 0.02
    x0, x1 = cx - 0.25, cx + 0.25
    y0, y1 = cy - 0.275, cy + 0.275
    a.box((x0, y0, 0.0), (x1, y1, 0.08), paint)                  # zocalo
    a.box((x0, y0, 0.08), (x0 + t, y1, 1.9), paint)              # fondo
    a.box((x0, y0, 0.08), (x1, y0 + t, 1.9), paint)              # lateral
    a.box((x0, y1 - t, 0.08), (x1, y1, 1.9), paint)              # lateral
    a.box((x0, y0, 1.88), (x1, y1, 1.9), paint)                  # techo
    a.box((x0, y0, 0.08), (x1, y1, 0.1), paint)                  # suelo
    a.box((x0 + 0.05, y0 + t, 1.55), (x1 - 0.02, y1 - t, 1.57), paint)  # balda
    if with_door:
        locker_door(a, x1, cy)


def locker_door(a, x=0.0, cy=0.0):
    """Puerta centrada en Y, con rejilla de lamas para ver a traves."""
    paint = M["M_LockerPaint"]
    d = 0.02
    w = 0.265
    a.box((x, cy - w, 0.1), (x + d, cy + w, 1.3), paint)                 # parte baja
    a.box((x, cy - w, 1.66), (x + d, cy + w, 1.88), paint)               # parte alta
    a.box((x, cy - w, 1.3), (x + d, cy - w + 0.04, 1.66), paint)         # marco lateral
    a.box((x, cy + w - 0.04, 1.3), (x + d, cy + w, 1.66), paint)
    for i in range(6):                                                   # lamas con huecos
        z = 1.32 + i * 0.056
        a.box((x, cy - w + 0.04, z), (x + d, cy + w - 0.04, z + 0.024), paint)
    a.box((x + d, cy - 0.02, 0.95), (x + d + 0.025, cy + 0.02, 1.1), M["M_Metal"])  # tirador
    a.box((x + d, cy - 0.06, 1.72), (x + d + 0.003, cy + 0.06, 1.8), M["M_Paper"])  # etiqueta


def build_locker():
    a = Asset("SM_Locker")
    locker_body(a)
    a.col_box((-0.25, -0.275, 0.0), (0.27, 0.275, 1.9))
    return a.build()


def build_locker_door():
    a = Asset("SM_LockerDoor")
    locker_door(a, 0.0, 0.0)
    return a.build()


def build_locker_row():
    a = Asset("SM_LockerRow")
    for i in range(4):
        locker_body(a, 0.0, -0.825 + i * 0.55, with_door=True)
    a.col_box((-0.25, -1.1, 0.0), (0.3, 1.1, 1.9))
    return a.build()


def build_bench():
    a = Asset("SM_Bench")
    wood, metal = M["M_Wood"], M["M_Metal"]
    for i in range(3):
        y = -0.14 + i * 0.14
        a.box((-0.75, y - 0.06, 0.40), (0.75, y + 0.06, 0.44), wood)
    for x in (-0.6, 0.6):
        a.box((x - 0.02, -0.18, 0.0), (x + 0.02, -0.15, 0.40), metal)
        a.box((x - 0.02, 0.15, 0.0), (x + 0.02, 0.18, 0.40), metal)
        a.box((x - 0.02, -0.18, 0.36), (x + 0.02, 0.18, 0.40), metal)
    a.box((-0.6, -0.01, 0.12), (0.6, 0.01, 0.15), metal)
    a.col_box((-0.75, -0.2, 0.0), (0.75, 0.2, 0.44))
    return a.build()


def build_shower_partition():
    a = Asset("SM_ShowerPartition")
    a.box((-1.2, -0.025, 0.0), (0.0, 0.025, 2.0), M["M_WallTile"], collide=True)
    a.box((-1.22, -0.03, 0.0), (-1.18, 0.03, 2.02), M["M_Metal"])
    return a.build()


def build_shower_head():
    a = Asset("SM_ShowerHead")
    metal = M["M_Metal"]
    a.cylinder((-0.02, 0, -1.0), (-0.02, 0, 1.7), 0.015, metal, segments=10)       # tuberia vertical
    a.cylinder((-0.02, 0, 0.0), (-0.3, 0, 0.0), 0.015, metal, segments=10)         # brazo
    a.cylinder((-0.3, 0, 0.0), (-0.3, 0, -0.06), 0.06, metal, r1=0.03, segments=16)  # alcachofa
    a.cylinder((-0.02, 0, -0.9), (-0.08, 0, -0.9), 0.04, metal, segments=12)       # llave
    a.box((-0.1, -0.06, -0.91), (-0.08, 0.06, -0.89), metal)
    return a.build()


def build_fuse_panel():
    """Cuadro electrico. Origen en la pared (parte trasera), centro vertical del cuadro."""
    a = Asset("SM_FusePanel")
    metal = M["M_Metal"]
    a.box((0.0, -0.3, -0.4), (0.13, 0.3, 0.4), metal, collide=True)
    a.box((0.13, -0.28, -0.38), (0.135, 0.28, 0.25), M["M_Ceramic"])
    for y in (-0.18, 0.0, 0.18):
        a.box((0.135, y - 0.045, -0.1), (0.15, y + 0.045, 0.14), M["M_Ceramic"])
        a.box((0.15, y - 0.022, -0.06), (0.152, y + 0.022, 0.1), M["M_Socket"])
    a.box((0.13, -0.2, 0.28), (0.135, 0.2, 0.37), M["M_Warning"])
    a.cylinder((0.07, 0, 0.4), (0.07, 0, 2.4), 0.03, metal, segments=12)   # conducto al techo
    a.cylinder((0.07, -0.22, -0.4), (0.07, -0.22, -1.0), 0.02, metal, segments=10)
    a.cylinder((0.07, 0.22, -0.4), (0.07, 0.22, -1.0), 0.02, metal, segments=10)
    return a.build()


def build_fuse(color_name):
    a = Asset(f"SM_Fuse_{color_name}")
    body = M[f"M_Fuse_{color_name}"]
    metal = M["M_Metal"]
    a.cylinder((0, 0, -0.03), (0, 0, 0.03), 0.018, body, segments=16)
    a.cylinder((0, 0, -0.045), (0, 0, -0.03), 0.02, metal, segments=16)
    a.cylinder((0, 0, 0.03), (0, 0, 0.045), 0.02, metal, segments=16)
    a.col_box((-0.02, -0.02, -0.045), (0.02, 0.02, 0.045))
    return a.build()


def build_exit_door():
    """Puerta centrada en Y. El pivote de la bisagra lo pone ExitDoor.cpp."""
    a = Asset("SM_ExitDoor")
    metal = M["M_Metal"]
    a.box((-0.08, -0.6, 0.0), (-0.02, 0.6, 2.2), M["M_Wood"], collide=True)
    a.box((-0.02, -0.25, 0.95), (-0.01, 0.25, 1.1), metal)             # placa antipanico
    a.box((-0.02, -0.55, 0.05), (-0.01, 0.55, 0.3), metal)             # chapa inferior
    a.box((-0.01, -0.02, 0.98), (0.03, 0.02, 1.07), metal)             # cerradura
    return a.build()


def build_exit_sign():
    a = Asset("SM_ExitSign")
    a.box((0.0, -0.18, -0.06), (0.03, 0.18, 0.06), M["M_ExitSign"])
    return a.build()


def build_pool_stairs():
    a = Asset("SM_PoolStairs")
    tile, metal = M["M_PoolTile"], M["M_Metal"]
    for i in range(3):
        top = -0.3 * (i + 1)
        a.box((0.4 * i, -0.5, -1.2), (0.4 * (i + 1), 0.5, top), tile, collide=True)
    for y in (-0.45, 0.45):
        a.cylinder((0.05, y, 0.0), (0.05, y, 0.9), 0.02, metal, segments=10)
        a.cylinder((0.05, y, 0.9), (0.9, y, -0.3), 0.02, metal, segments=10)
        a.cylinder((0.9, y, -0.3), (0.9, y, -0.9), 0.02, metal, segments=10)
    return a.build()


def build_lamp_fixture():
    a = Asset("SM_LampFixture")
    metal = M["M_Metal"]
    a.box((-0.6, -0.1, -0.08), (0.6, 0.1, 0.0), metal)
    a.cylinder((-0.55, -0.04, -0.1), (0.55, -0.04, -0.1), 0.016, M["M_LampTube"], segments=10)
    a.cylinder((-0.55, 0.04, -0.1), (0.3, 0.04, -0.1), 0.016, M["M_LampTube"], segments=10)  # tubo roto
    return a.build()


def build_pipe():
    a = Asset("SM_Pipe")
    metal = M["M_Metal"]
    a.cylinder((-1.0, 0, 0), (1.0, 0, 0), 0.05, metal, segments=14)
    a.cylinder((0.94, 0, 0), (1.0, 0, 0), 0.07, metal, segments=14)
    a.box((-0.02, -0.12, -0.02), (0.02, 0.12, 0.02), metal)            # abrazadera
    return a.build()


def build_debris():
    import random
    rnd = random.Random(7)
    a = Asset("SM_Debris")
    for _ in range(14):
        s = rnd.uniform(0.04, 0.15)
        m = (Matrix.Translation((rnd.uniform(-0.6, 0.6), rnd.uniform(-0.6, 0.6), 0.006))
             @ Matrix.Rotation(rnd.uniform(0, math.pi), 4, "Z")
             @ Matrix.Rotation(rnd.uniform(-0.2, 0.2), 4, "X"))
        mat = M["M_WallTile"] if rnd.random() < 0.6 else M["M_Plaster"]
        a.box((-s / 2, -s / 2, -0.006), (s / 2, s * 0.35, 0.006), mat, matrix=m)
    return a.build()


def build_bucket():
    a = Asset("SM_Bucket")
    metal = M["M_Metal"]
    a.cylinder((0, 0, 0.0), (0, 0, 0.32), 0.12, metal, r1=0.15, segments=20, caps=False)
    a.cylinder((0, 0, 0.0), (0, 0, 0.01), 0.12, metal, segments=20)
    a.col_box((-0.15, -0.15, 0.0), (0.15, 0.15, 0.32))
    return a.build()


def build_note():
    a = Asset("SM_Note")
    a.box((0.0, -0.16, -0.21), (0.003, 0.16, 0.21), M["M_Paper"])
    return a.build()


# --- Enemigo: "El Banista" -------------------------------------------------

def metaball_mesh(name, elements, resolution=0.02, decimate=None):
    mb = bpy.data.metaballs.new(name + "MB")
    mb.resolution = resolution
    mb.render_resolution = resolution
    mb.threshold = 0.6
    obj = bpy.data.objects.new(name + "MB", mb)
    bpy.context.scene.collection.objects.link(obj)
    for e in elements:
        el = mb.elements.new(type=e.get("type", "BALL"))
        el.co = e["co"]
        el.radius = e["r"]
        el.stiffness = e.get("stiff", 2.0)
        el.use_negative = e.get("neg", False)
        if "size" in e:
            el.size_x, el.size_y, el.size_z = e["size"]
        if "rot" in e:
            el.rotation = e["rot"]
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(obj.evaluated_get(dg))
    bpy.data.objects.remove(obj)
    if decimate:
        tmp = bpy.data.objects.new(name + "Tmp", me)
        bpy.context.scene.collection.objects.link(tmp)
        mod = tmp.modifiers.new("dec", "DECIMATE")
        mod.ratio = decimate
        dg = bpy.context.evaluated_depsgraph_get()
        me2 = bpy.data.meshes.new_from_object(tmp.evaluated_get(dg))
        bpy.data.objects.remove(tmp)
        me = me2
    return me


def capsule(p0, p1, r, stiff=2.0):
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    q = d.normalized().to_track_quat("X", "Z")
    return {"type": "CAPSULE", "co": (p0 + p1) * 0.5, "r": r, "size": (d.length * 0.5, 1, 1), "rot": q, "stiff": stiff}


def ellipsoid(co, size, r=1.0, neg=False):
    """size = semiejes aproximados en metros (se compensa el umbral del metaball)."""
    k = 1.35 * r / 0.5
    return {"type": "ELLIPSOID", "co": co, "r": 1.0, "size": tuple(s * k for s in size), "neg": neg}


def build_enemy_body():
    a = Asset("SM_Enemy")
    el = [
        ellipsoid((0.0, 0, 1.08), (0.13, 0.17, 0.12), 0.5),                 # pelvis
        capsule((0.0, 0, 1.1), (0.15, 0, 1.55), 0.13),                       # abdomen hundido
        ellipsoid((0.14, 0, 1.52), (0.14, 0.19, 0.24), 0.5),                 # costillar
        capsule((0.16, 0, 1.70), (0.31, 0, 2.0), 0.06),                      # cuello (entra en la cabeza)
        capsule((0.12, -0.24, 1.68), (0.12, 0.24, 1.68), 0.07),              # hombros
    ]
    for s in (-1, 1):
        el += [
            capsule((0.0, 0.11 * s, 1.02), (0.08, 0.14 * s, 0.56), 0.075),   # muslo
            capsule((0.08, 0.14 * s, 0.56), (-0.03, 0.14 * s, 0.08), 0.055),  # espinilla
            ellipsoid((0.05, 0.14 * s, 0.035), (0.13, 0.05, 0.035), 0.5),     # pie
            capsule((0.12, 0.26 * s, 1.66), (0.26, 0.31 * s, 1.18), 0.05),   # brazo
            capsule((0.26, 0.31 * s, 1.18), (0.36, 0.28 * s, 0.74), 0.04),   # antebrazo
            ellipsoid((0.37, 0.28 * s, 0.70), (0.035, 0.05, 0.06), 0.5),      # mano
            ellipsoid((0.15, 0.11 * s, 1.47), (0.03, 0.05, 0.02), 0.4),       # costillas marcadas
            ellipsoid((0.15, 0.11 * s, 1.40), (0.03, 0.05, 0.02), 0.4),
        ]
    body = metaball_mesh("EnemyBody", el, resolution=0.018, decimate=0.35)
    a.mesh(body, M["M_Skin"])
    # Dedos larguisimos
    for s in (-1, 1):
        for i, off in enumerate((-0.035, -0.012, 0.012, 0.035)):
            base = Vector((0.37, 0.28 * s + off, 0.66))
            mid = base + Vector((0.03, off * 0.3, -0.12))
            tip = mid + Vector((0.05, off * 0.4, -0.10 - 0.02 * (i % 2)))
            a.cylinder(base, mid, 0.011, M["M_Skin"], r1=0.009, segments=6)
            a.cylinder(mid, tip, 0.009, M["M_Skin"], r1=0.004, segments=6)
    # Toalla raida alrededor de la cintura
    import random
    rnd = random.Random(3)
    faces = a.cylinder((0, 0, 1.18), (0, 0, 0.78), 0.19, M["M_Cloth"], r1=0.25, segments=24, caps=False)
    verts = {v for f in faces for v in f.verts}
    for v in verts:
        if v.co.z < 0.9:
            v.co.z += rnd.uniform(-0.08, 0.12)
            v.co.x += rnd.uniform(-0.02, 0.02)
    return a.build()


def build_enemy_head():
    a = Asset("SM_EnemyHead")
    el = [
        ellipsoid((0.06, 0, 0.14), (0.12, 0.09, 0.15), 0.5),                 # craneo alargado
        ellipsoid((0.10, 0, 0.0), (0.07, 0.065, 0.12), 0.5),                 # mandibula caida
        ellipsoid((0.17, 0, 0.02), (0.03, 0.025, 0.08), 0.5, neg=True),      # boca (hueco)
        ellipsoid((0.16, -0.04, 0.17), (0.03, 0.025, 0.02), 0.5, neg=True),  # cuencas vacias
        ellipsoid((0.16, 0.04, 0.17), (0.03, 0.025, 0.02), 0.5, neg=True),
    ]
    head = metaball_mesh("EnemyHead", el, resolution=0.01, decimate=0.5)
    a.mesh(head, M["M_Skin"])
    # Interior oscuro de la boca y las cuencas
    mouth = metaball_mesh("EnemyMouth", [ellipsoid((0.145, 0, 0.02), (0.02, 0.02, 0.07), 0.5)], 0.01)
    a.mesh(mouth, M["M_Mouth"])
    for s in (-1, 1):
        eye = metaball_mesh(f"EnemyEye{s}", [ellipsoid((0.135, 0.04 * s, 0.17), (0.02, 0.018, 0.014), 0.5)], 0.008)
        a.mesh(eye, M["M_Mouth"])
    return a.build()


BUILDERS = [
    build_room, build_locker, build_locker_door, build_locker_row, build_bench,
    build_shower_partition, build_shower_head, build_fuse_panel,
    lambda: build_fuse("Red"), lambda: build_fuse("Blue"), lambda: build_fuse("Green"),
    build_exit_door, build_exit_sign, build_pool_stairs, build_lamp_fixture, build_pipe,
    build_debris, build_bucket, build_note, build_enemy_body, build_enemy_head,
]


# ---------------------------------------------------------------------------
# Exportacion
# ---------------------------------------------------------------------------

def export_fbx(obj, cols):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    obj.select_set(True)
    for c in cols:
        c.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(MESH_DIR, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"MESH"},
        apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE",
        use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False,
        path_mode="RELATIVE", embed_textures=False,
    )
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print(f"  {obj.name:<22} {tris:>7} tris  {len(cols)} colisiones")


# ---------------------------------------------------------------------------
# Previsualizacion (render con Cycles)
# ---------------------------------------------------------------------------

ACTOR_MESHES = {
    "Locker": [("SM_Locker", (0, 0, 0)), ("SM_LockerDoor", (0.25, 0, 0))],
    "FusePanel": [("SM_FusePanel", (0, 0, 0))],
    "ClueNote": [("SM_Note", (0, 0, 0))],
    "ExitDoor": [("SM_ExitDoor", (0, 0, 0))],
}


def place(src, loc_ue, yaw, extra=(0, 0, 0), extra_rot=None):
    inst = bpy.data.objects.new(src.name + "_inst", src.data)
    bpy.context.scene.collection.objects.link(inst)
    x, y, z = ue(*loc_ue)
    m = Matrix.Translation((x, y, z)) @ Matrix.Rotation(math.radians(-yaw), 4, "Z") @ Matrix.Translation(extra)
    if extra_rot is not None:
        m = m @ extra_rot
    inst.matrix_world = m
    return inst


def build_preview_scene(assets):
    for obj in list(bpy.context.scene.objects):
        obj.hide_render = True
    for item in LAYOUT["static_meshes"]:
        place(assets[item["mesh"]], item["loc"], item["yaw"])
    for act in LAYOUT["actors"]:
        if act["class"] == "FusePickup":
            src = assets["SM_Fuse_" + act["fuse"].capitalize()]
            place(src, act["loc"], act["yaw"], extra_rot=Matrix.Rotation(math.pi / 2, 4, "Y"))
            continue
        for mesh_name, off in ACTOR_MESHES.get(act["class"], []):
            place(assets[mesh_name], act["loc"], act["yaw"], extra=off)
    # Enemigo junto al borde de la piscina, mirando al jugador
    enemy_loc, enemy_yaw = (1080, 120, 0), 190
    place(assets["SM_Enemy"], enemy_loc, enemy_yaw)
    place(assets["SM_EnemyHead"], enemy_loc, enemy_yaw, extra=tuple(ENEMY_HEAD_ATTACH),
          extra_rot=Matrix.Rotation(math.radians(18), 4, "X") @ Matrix.Rotation(math.radians(12), 4, "Y"))


def add_light(kind, loc, energy, color, name, rot=None, size=0.1, spot=None):
    ld = bpy.data.lights.new(name, kind)
    ld.energy = energy
    ld.color = color
    if hasattr(ld, "shadow_soft_size"):
        ld.shadow_soft_size = size
    if spot:
        ld.spot_size, ld.spot_blend = spot
    lo = bpy.data.objects.new(name, ld)
    lo.location = loc
    if rot:
        lo.rotation_euler = rot
    bpy.context.scene.collection.objects.link(lo)
    return lo


def render(path, cam_loc_ue, target_ue, lens=18, power_on=False, flashlight=True, samples=48):
    scene = bpy.context.scene
    for o in [o for o in scene.objects if o.type in {"LIGHT", "CAMERA"}]:
        bpy.data.objects.remove(o)
    cam_d = bpy.data.cameras.new("Cam")
    cam_d.lens = lens
    cam = bpy.data.objects.new("Cam", cam_d)
    scene.collection.objects.link(cam)
    cam.location = ue(*cam_loc_ue)
    direction = Vector(ue(*target_ue)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    for L in LAYOUT["lights"]:
        is_power = "PowerLight" in L["tags"]
        if is_power and not power_on:
            continue
        add_light("POINT", ue(*L["loc"]), L["lumens"] / 683.0 * 4 * math.pi * (0.4 if is_power else 1.0),
                  L["color"], L["label"])
    if flashlight:
        fl = add_light("SPOT", cam.location + Vector((0, 0, -0.1)), 180.0, (1.0, 0.93, 0.82), "Flashlight",
                       spot=(math.radians(55), 0.4))
        fl.rotation_euler = cam.rotation_euler
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    try:
        scene.cycles.use_denoising = True
    except Exception:
        pass
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = path
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.color = (0.0, 0.0, 0.0)
    try:
        scene.view_settings.view_transform = "AgX"
    except Exception:
        pass
    scene.view_settings.exposure = 1.2
    bpy.ops.render.render(write_still=True)
    print("  render ->", path)


def main():
    preview = "--preview" in sys.argv
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(MESH_DIR, exist_ok=True)
    create_materials()
    assets = {}
    print("Exportando FBX a", MESH_DIR)
    for builder in BUILDERS:
        obj, cols = builder()
        export_fbx(obj, cols)
        assets[obj.name] = obj
        for c in cols:
            c.hide_render = True
    if preview:
        os.makedirs(DOCS_DIR, exist_ok=True)
        build_preview_scene(assets)
        render(os.path.join(DOCS_DIR, "preview_jugador.png"), (1450, 0, 165), (900, 60, 60), lens=16)
        render(os.path.join(DOCS_DIR, "preview_luz.png"), (1560, -440, 330), (500, 150, 0),
               lens=14, power_on=True, flashlight=False)
        render(os.path.join(DOCS_DIR, "preview_enemigo.png"), (1290, 60, 175), (1080, 120, 170),
               lens=35, flashlight=True)


if __name__ == "__main__":
    main()
