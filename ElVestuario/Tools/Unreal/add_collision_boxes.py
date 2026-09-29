"""
Anade colision fiable al nivel L_Vestuario con cubos invisibles del motor.

Por que: en Unreal 5.8 el importador de FBX (Interchange) puede dejar los modelos sin
colision, y activar "Use Complex As Simple" por script no siempre reconstruye la fisica.
Resultado: el jugador cae al vacio y todo se ve negro.

Este script coloca cubos (/Engine/BasicShapes/Cube, que SIEMPRE tienen colision) en la
carpeta "Colision" del nivel, invisibles, en la posicion exacta de suelo, paredes, techo,
piscina y escaleras. Ademas pone una caja de colision simple a los props (taquillas, bancos...).

Uso: con L_Vestuario abierto, Herramientas > Ejecutar script de Python -> este archivo.
Se puede ejecutar varias veces (borra y recrea la carpeta "Colision").
"""
import unreal

FOLDER = "Colision"
MESH_PATH = "/Game/Vestuario/Meshes"

# Cajas en cm de Unreal: (min xyz, max xyz). Coinciden con build_room() de gen_meshes.py
ROOM_BOXES = [
    # Suelo alrededor de la piscina
    ((0, -500, -20), (550, 500, 0)),
    ((1050, -500, -20), (1600, 500, 0)),
    ((550, 225, -20), (1050, 500, 0)),
    ((550, -500, -20), (1050, -225, 0)),
    # Piscina
    ((550, -225, -140), (1050, 225, -120)),
    ((530, -245, -140), (550, 245, -20)),
    ((1050, -245, -140), (1070, 245, -20)),
    ((550, 225, -140), (1050, 245, -20)),
    ((550, -245, -140), (1050, -225, -20)),
    # Paredes (altura completa)
    ((0, 500, 0), (1600, 520, 380)),
    ((0, -520, 0), (1600, -500, 380)),
    ((1600, -520, 0), (1620, 520, 380)),
    ((-20, 60, 0), (0, 520, 380)),
    ((-20, -520, 0), (0, -60, 380)),
    ((-20, -60, 220), (0, 60, 380)),
    # Techo
    ((-20, -520, 380), (1620, 520, 400)),
    # Pasillo de salida
    ((-520, -110, -20), (0, 110, 0)),
    ((-520, 110, 0), (-20, 130, 260)),
    ((-520, -130, 0), (-20, -110, 260)),
    ((-520, -130, 260), (-20, 130, 280)),
    ((-540, -130, 0), (-520, 130, 260)),
    # Escaleras de la piscina (3 escalones de 30 cm)
    ((550, 125, -120), (590, 225, -30)),
    ((590, 125, -120), (630, 225, -60)),
    ((630, 125, -120), (670, 225, -90)),
]

# Props que necesitan una caja simple para no atravesarlos
PROPS = ["SM_Locker", "SM_LockerRow", "SM_Bench", "SM_ShowerPartition",
         "SM_ExitDoor", "SM_FusePanel", "SM_Bucket"]


def add_prop_boxes():
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for name in PROPS:
        mesh = unreal.load_asset(f"{MESH_PATH}/{name}.{name}")
        if mesh is None:
            continue
        try:
            body = mesh.get_editor_property("body_setup")
            if body:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
        except Exception:  # noqa: BLE001
            pass
        if sub.get_simple_collision_count(mesh) == 0:
            sub.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        unreal.log(f"[Vestuario] {name}: colisiones simples = {sub.get_simple_collision_count(mesh)}")


def add_room_boxes():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in eas.get_all_level_actors() if str(a.get_folder_path()) == FOLDER]
    if old:
        eas.destroy_actors(old)

    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    for i, (mn, mx) in enumerate(ROOM_BOXES):
        center = unreal.Vector((mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2, (mn[2] + mx[2]) / 2)
        scale = unreal.Vector((mx[0] - mn[0]) / 100.0, (mx[1] - mn[1]) / 100.0, (mx[2] - mn[2]) / 100.0)
        actor = eas.spawn_actor_from_object(cube, center, unreal.Rotator(0, 0, 0))
        actor.set_actor_label(f"Colision_{i:02d}")
        actor.set_folder_path(FOLDER)
        actor.set_actor_scale3d(scale)
        comp = actor.static_mesh_component
        comp.set_collision_profile_name("BlockAll")
        comp.set_visibility(False)          # invisible, pero sigue chocando
        comp.set_cast_shadow(False)
    unreal.log(f"[Vestuario] Cajas de colision de la sala: {len(ROOM_BOXES)}")


def check_nav_bounds():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    vols = [a for a in eas.get_all_level_actors() if isinstance(a, unreal.NavMeshBoundsVolume)]
    for v in vols:
        _, ext = v.get_actor_bounds(False)
        unreal.log(f"[Vestuario] NavMeshBoundsVolume tamano: {ext.x * 2:.0f} x {ext.y * 2:.0f} x {ext.z * 2:.0f} cm")
        if ext.x < 10:
            unreal.log_error("[Vestuario] El NavMeshBoundsVolume esta VACIO: el enemigo no podra moverse. "
                             "Borralo y pon uno nuevo desde Place Actors > Volumes > Nav Mesh Bounds Volume, "
                             "escalado para cubrir toda la sala (pulsa P para ver la zona verde).")
    if not vols:
        unreal.log_error("[Vestuario] No hay NavMeshBoundsVolume en el nivel.")


def main():
    add_prop_boxes()
    add_room_boxes()
    check_nav_bounds()
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    except Exception:  # noqa: BLE001
        pass
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.log("[Vestuario] Colision lista y nivel guardado. Pulsa Play.")


main()
