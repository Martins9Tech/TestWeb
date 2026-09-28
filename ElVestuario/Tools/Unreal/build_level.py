"""
Construye "El Vestuario" dentro del editor de Unreal Engine.

Como usarlo (con el proyecto ya compilado y abierto en el editor):
    Menu Tools > Execute Python Script...  ->  elegir este archivo
    (o en la consola Python del Output Log:  py "Tools/Unreal/build_level.py")

Que hace:
    1. Importa texturas, sonidos y modelos de SourceArt/  ->  /Game/Vestuario/...
    2. Crea un material maestro y un Material Instance por cada material de Blender
    3. Asigna los materiales a los meshes (por nombre de slot)
    4. Crea el nivel /Game/Vestuario/Maps/L_Vestuario y coloca todo segun Tools/layout.json
       (sala, props, taquillas, fusibles, cuadro, nota, puerta, enemigo, luces, niebla, post-proceso, navmesh)

Se puede ejecutar varias veces: reimporta y reconstruye el nivel desde cero.
"""
import json
import os

import unreal

PROJECT_DIR = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE_ART = os.path.join(PROJECT_DIR, "SourceArt")
with open(os.path.join(PROJECT_DIR, "Tools", "layout.json"), encoding="utf-8") as fh:
    LAYOUT = json.load(fh)

BASE = "/Game/Vestuario"
TEX_PATH = BASE + "/Textures"
MAT_PATH = BASE + "/Materials"
MESH_PATH = BASE + "/Meshes"
AUDIO_PATH = BASE + "/Audio"
LEVEL_PATH = BASE + "/Maps/L_Vestuario"

LOOPING_SOUNDS = {"S_Ambience", "S_EnemyBreath", "S_Heartbeat"}

# Material de Blender -> parametros del Material Instance
MATERIALS = {
    "M_WallTile":    {"tex": "T_WallTile",    "rough": 0.35},
    "M_FloorTile":   {"tex": "T_FloorTile",   "rough": 0.3},
    "M_PoolTile":    {"tex": "T_PoolTile",    "rough": 0.25},
    "M_Plaster":     {"tex": "T_Plaster",     "rough": 0.9},
    "M_Concrete":    {"tex": "T_Concrete",    "rough": 0.85},
    "M_LockerPaint": {"tex": "T_LockerPaint", "rough": 0.55, "metal": 0.3},
    "M_Metal":       {"tex": "T_Metal",       "rough": 0.45, "metal": 0.8},
    "M_Wood":        {"tex": "T_Wood",        "rough": 0.7},
    "M_Skin":        {"tex": "T_Skin",        "rough": 0.3},
    "M_Paper":       {"tex": "T_Paper",       "rough": 0.9},
    "M_Socket":      {"tint": (0.05, 0.05, 0.05), "rough": 0.8},
    "M_Ceramic":     {"tint": (0.75, 0.73, 0.68), "rough": 0.3},
    "M_Warning":     {"tint": (0.85, 0.65, 0.05), "rough": 0.6},
    "M_ExitSign":    {"tint": (0.05, 0.45, 0.12), "rough": 0.4, "emissive": (0.2, 3.0, 0.5)},
    "M_LampTube":    {"tint": (0.9, 0.92, 0.95), "rough": 0.2},
    "M_Fuse_Red":    {"tint": (0.60, 0.05, 0.04), "rough": 0.4},
    "M_Fuse_Blue":   {"tint": (0.05, 0.15, 0.60), "rough": 0.4},
    "M_Fuse_Green":  {"tint": (0.05, 0.45, 0.10), "rough": 0.4},
    "M_Cloth":       {"tint": (0.33, 0.30, 0.24), "rough": 0.95},
    "M_Mouth":       {"tint": (0.01, 0.0, 0.0), "rough": 0.2},
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Vestuario] " + msg)


def files_in(folder, ext):
    path = os.path.join(SOURCE_ART, folder)
    return sorted(os.path.join(path, f) for f in os.listdir(path) if f.lower().endswith(ext))


def import_files(files, dest, options=None):
    tasks = []
    for f in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", f)
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        if options is not None:
            task.set_editor_property("options", options)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)


def asset_name(path):
    return os.path.splitext(os.path.basename(path))[0]


def load(path_no_ext):
    name = path_no_ext.split("/")[-1]
    return unreal.load_asset(f"{path_no_ext}.{name}")


# ---------------------------------------------------------------------------
# 1. Importacion
# ---------------------------------------------------------------------------

def import_textures():
    files = files_in("Textures", ".png")
    import_files(files, TEX_PATH)
    for f in files:
        name = asset_name(f)
        tex = load(f"{TEX_PATH}/{name}")
        if tex is None:
            unreal.log_warning(f"No se pudo importar {name}")
            continue
        if name.endswith("_N"):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        eal.save_loaded_asset(tex)
    log(f"Texturas importadas: {len(files)}")


def import_audio():
    files = files_in("Audio", ".wav")
    import_files(files, AUDIO_PATH)
    for f in files:
        name = asset_name(f)
        sound = load(f"{AUDIO_PATH}/{name}")
        if sound is None:
            unreal.log_warning(f"No se pudo importar {name}")
            continue
        sound.set_editor_property("looping", name in LOOPING_SOUNDS)
        eal.save_loaded_asset(sound)
    log(f"Sonidos importados: {len(files)}")


def import_meshes():
    # El importador FBX clasico respeta las opciones de abajo y las colisiones UCX_.
    # En versiones recientes Unreal usa "Interchange"; intentamos desactivarlo para el FBX.
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX false")
    except Exception as exc:  # noqa: BLE001
        unreal.log_warning(f"No se pudo desactivar Interchange ({exc}); se usara el importador por defecto")

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    smd = options.get_editor_property("static_mesh_import_data")
    smd.set_editor_property("combine_meshes", True)
    smd.set_editor_property("auto_generate_collision", False)
    smd.set_editor_property("generate_lightmap_u_vs", False)
    files = files_in("Meshes", ".fbx")
    import_files(files, MESH_PATH, options)
    log(f"Modelos importados: {len(files)}")


# ---------------------------------------------------------------------------
# 2. Materiales
# ---------------------------------------------------------------------------

def create_master_material():
    path = f"{MAT_PATH}/M_VestuarioMaster"
    if eal.does_asset_exist(path):
        return load(path)

    mat = asset_tools.create_asset("M_VestuarioMaster", MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", True)
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    flat = unreal.load_asset("/Engine/EngineMaterials/FlatNormal.FlatNormal")

    coords = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    tiling = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1100, 150)
    tiling.set_editor_property("parameter_name", "Tiling")
    tiling.set_editor_property("default_value", 1.0)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -900, 50)
    mel.connect_material_expressions(coords, "", uv, "A")
    mel.connect_material_expressions(tiling, "", uv, "B")

    base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, -250)
    base.set_editor_property("parameter_name", "BaseColorTex")
    base.set_editor_property("texture", white)
    mel.connect_material_expressions(uv, "", base, "UVs")

    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -30)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -150)
    mel.connect_material_expressions(base, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 200)
    normal.set_editor_property("parameter_name", "NormalTex")
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal.set_editor_property("texture", flat)
    mel.connect_material_expressions(uv, "", normal, "UVs")
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    for name, default, prop, y in (("Roughness", 0.6, unreal.MaterialProperty.MP_ROUGHNESS, 450),
                                   ("Metallic", 0.0, unreal.MaterialProperty.MP_METALLIC, 550)):
        p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, y)
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", default)
        mel.connect_material_property(p, "", prop)

    emissive = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 650)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 1))
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def create_material_instances(master):
    instances = {}
    for mat_name, cfg in MATERIALS.items():
        mi_name = "MI_" + mat_name[2:]
        path = f"{MAT_PATH}/{mi_name}"
        mi = load(path) if eal.does_asset_exist(path) else asset_tools.create_asset(
            mi_name, MAT_PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        if "tex" in cfg:
            d = load(f"{TEX_PATH}/{cfg['tex']}_D")
            n = load(f"{TEX_PATH}/{cfg['tex']}_N")
            if d:
                mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", d)
            if n:
                mel.set_material_instance_texture_parameter_value(mi, "NormalTex", n)
        tint = cfg.get("tint", (1.0, 1.0, 1.0))
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        em = cfg.get("emissive", (0.0, 0.0, 0.0))
        mel.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(em[0], em[1], em[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", cfg.get("rough", 0.6))
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", cfg.get("metal", 0.0))
        eal.save_loaded_asset(mi)
        instances[mat_name] = mi
    log(f"Materiales creados: {len(instances)}")
    return instances


def assign_materials(instances):
    for f in files_in("Meshes", ".fbx"):
        name = asset_name(f)
        mesh = load(f"{MESH_PATH}/{name}")
        if mesh is None:
            unreal.log_warning(f"Mesh no encontrado: {name}")
            continue
        for i, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            key = next((k for k in instances if slot_name == k or slot_name.startswith(k)), None)
            if key:
                mesh.set_material(i, instances[key])
            else:
                unreal.log_warning(f"{name}: slot '{slot_name}' sin material conocido")
        eal.save_loaded_asset(mesh)


# ---------------------------------------------------------------------------
# 3. Nivel
# ---------------------------------------------------------------------------

def vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]))


def rot(yaw):
    return unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(yaw))


def open_clean_level():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if eal.does_asset_exist(LEVEL_PATH):
        les.load_level(LEVEL_PATH)
        # Se borra todo menos WorldSettings y el Brush por defecto del nivel
        # (ojo: los Volumes tambien heredan de Brush, por eso se compara la clase exacta)
        actors = [a for a in eas.get_all_level_actors()
                  if not isinstance(a, unreal.WorldSettings) and a.get_class() != unreal.Brush.static_class()]
        eas.destroy_actors(actors)
    else:
        les.new_level(LEVEL_PATH)
    return les, eas


def spawn_mesh(eas, mesh_name, loc, yaw, folder, label=None):
    mesh = load(f"{MESH_PATH}/{mesh_name}")
    if mesh is None:
        unreal.log_warning(f"No existe {mesh_name}")
        return None
    actor = eas.spawn_actor_from_object(mesh, vec(loc), rot(yaw))
    actor.set_actor_label(label or mesh_name)
    actor.set_folder_path(folder)
    return actor


def spawn_class(eas, class_name, loc, yaw, folder, label):
    cls = unreal.load_class(None, f"/Script/ElVestuario.{class_name}")
    if cls is None:
        unreal.log_error(f"No se encuentra la clase C++ {class_name}. Has compilado el proyecto?")
        return None
    actor = eas.spawn_actor_from_class(cls, vec(loc), rot(yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def set_tags(actor, tags):
    actor.set_editor_property("tags", [unreal.Name(t) for t in tags])


def build_level():
    les, eas = open_clean_level()

    # Decorado estatico
    for item in LAYOUT["static_meshes"]:
        spawn_mesh(eas, item["mesh"], item["loc"], item["yaw"], "Decorado")

    # Actores de juego (clases C++)
    fuse_enum = {"RED": unreal.FuseColor.RED, "BLUE": unreal.FuseColor.BLUE, "GREEN": unreal.FuseColor.GREEN}
    for item in LAYOUT["actors"]:
        actor = spawn_class(eas, item["class"], item["loc"], item["yaw"], "Juego", item["label"])
        if actor and "fuse" in item:
            actor.set_editor_property("fuse_color", fuse_enum[item["fuse"]])

    # Jugador
    ps = LAYOUT["player_start"]
    start = eas.spawn_actor_from_class(unreal.PlayerStart, vec(ps["loc"]), rot(ps["yaw"]))
    start.set_folder_path("Juego")

    # Enemigo y patrulla
    en = LAYOUT["enemy"]
    points = []
    for i, p in enumerate(en["patrol"]):
        tp = eas.spawn_actor_from_class(unreal.TargetPoint, vec(p), rot(0))
        tp.set_actor_label(f"Patrulla_{i}")
        tp.set_folder_path("Juego/Patrulla")
        set_tags(tp, ["Patrol"])
        points.append(tp)
    enemy = spawn_class(eas, "EnemyCharacter", en["loc"], en["yaw"], "Juego", "ElBanista")
    if enemy:
        enemy.set_editor_property("patrol_points", points)

    # Luces
    for L in LAYOUT["lights"]:
        light = eas.spawn_actor_from_class(unreal.PointLight, vec(L["loc"]), rot(0))
        light.set_actor_label(L["label"])
        light.set_folder_path("Luces")
        light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp = light.get_component_by_class(unreal.PointLightComponent)
        comp.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        comp.set_editor_property("intensity", float(L["lumens"]))
        comp.set_editor_property("attenuation_radius", float(L["radius"]))
        comp.set_light_color(unreal.LinearColor(L["color"][0], L["color"][1], L["color"][2], 1.0))
        set_tags(light, L["tags"])
        if "PowerLight" in L["tags"]:
            comp.set_visibility(False)

    # Niebla volumetrica
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(800, 0, -200), rot(0))
    fog.set_folder_path("Ambiente")
    fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for prop, value in (("fog_density", 0.06), ("fog_height_falloff", 0.2), ("volumetric_fog", True),
                        ("volumetric_fog_scattering_distribution", 0.6),
                        ("volumetric_fog_extinction_scale", 1.5)):
        try:
            fog_comp.set_editor_property(prop, value)
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning(f"Niebla: no se pudo ajustar {prop}: {exc}")

    # Post-proceso: exposicion contenida (que la oscuridad sea oscura), grano, vineta, aberracion
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(800, 0, 100), rot(0))
    ppv.set_actor_label("PostProceso")
    ppv.set_folder_path("Ambiente")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    tweaks = {
        "auto_exposure_min_brightness": -1.0,
        "auto_exposure_max_brightness": 1.5,
        "auto_exposure_speed_up": 1.0,
        "auto_exposure_speed_down": 0.6,
        "vignette_intensity": 0.65,
        "film_grain_intensity": 0.3,
        "scene_fringe_intensity": 0.8,
        "bloom_intensity": 0.8,
        "color_saturation": unreal.Vector4(0.85, 0.85, 0.85, 1.0),
    }
    for prop, value in tweaks.items():
        try:
            s.set_editor_property("override_" + prop, True)
            s.set_editor_property(prop, value)
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning(f"Post-proceso: no se pudo ajustar {prop}: {exc}")
    ppv.set_editor_property("settings", s)

    # Navmesh (el enemigo necesita saber por donde puede andar)
    nb = LAYOUT["nav_bounds"]
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, vec(nb["center"]), rot(0))
    nav.set_folder_path("Ambiente")
    nav.set_actor_scale3d(unreal.Vector(nb["extent"][0] / 100.0, nb["extent"][1] / 100.0, nb["extent"][2] / 100.0))
    _, extent = nav.get_actor_bounds(False)
    if extent.x < 10:
        unreal.log_error("El NavMeshBoundsVolume se ha creado sin forma. Borralo y anade uno a mano "
                         "(Place Actors > Volumes > Nav Mesh Bounds Volume) que cubra toda la sala.")
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    except Exception:  # noqa: BLE001
        pass

    les.save_current_level()
    log("Nivel guardado en " + LEVEL_PATH)


def main():
    with unreal.ScopedSlowTask(6, "Construyendo El Vestuario...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Importando texturas")
        import_textures()
        task.enter_progress_frame(1, "Importando sonidos")
        import_audio()
        task.enter_progress_frame(1, "Importando modelos 3D")
        import_meshes()
        task.enter_progress_frame(1, "Creando materiales")
        instances = create_material_instances(create_master_material())
        assign_materials(instances)
        task.enter_progress_frame(1, "Montando el nivel")
        build_level()
        task.enter_progress_frame(1, "Listo")
    log("TODO LISTO. Pulsa Play (Alt+P) para jugar.")


main()
