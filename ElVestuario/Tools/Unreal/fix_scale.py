"""
Corrige la escala de los modelos importados.

Los FBX declaran que su unidad es el metro. El importador clasico de Unreal lo convierte
a centimetros (x100), pero Interchange (Unreal 5.8) puede ignorarlo y los modelos quedan
100 veces mas pequenos: la sala mide 16 cm, el jugador aparece "fuera" y todo se ve negro.

Este script mide SM_Room (debe medir ~2160 cm de largo), calcula el factor que falta y lo
aplica como "Build Scale" a todos los modelos. Tambien rehace las cajas de colision de los props.
Se puede ejecutar varias veces: si la escala ya es correcta, no cambia nada.

Uso: Herramientas > Ejecutar script de Python -> Tools/Unreal/fix_scale.py
"""
import unreal

MESH_PATH = "/Game/Vestuario/Meshes"
EXPECTED_ROOM_LENGTH = 2160.0  # cm, de x=-540 a x=1620
PROPS_WITH_BOX = ["SM_Locker", "SM_LockerRow", "SM_Bench", "SM_ShowerPartition",
                  "SM_ExitDoor", "SM_FusePanel", "SM_Bucket"]


def mesh_length(mesh):
    bounds = mesh.get_bounds()
    return bounds.box_extent.x * 2.0


def main():
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    room = unreal.load_asset(f"{MESH_PATH}/SM_Room.SM_Room")
    if room is None:
        unreal.log_error("[Vestuario] No existe SM_Room. Ejecuta antes build_level.py")
        return

    length = mesh_length(room)
    factor = EXPECTED_ROOM_LENGTH / max(length, 0.001)
    unreal.log(f"[Vestuario] SM_Room mide {length:.1f} cm (esperado {EXPECTED_ROOM_LENGTH:.0f}). Factor = {factor:.2f}")
    if abs(factor - 1.0) < 0.05:
        unreal.log("[Vestuario] La escala ya es correcta. Nada que hacer.")
        return
    # Redondear a potencia de 10 (100, 10...) para evitar errores por decimales
    for nice in (100.0, 10.0, 0.1, 0.01):
        if abs(factor / nice - 1.0) < 0.1:
            factor = nice
            break

    paths = unreal.EditorAssetLibrary.list_assets(MESH_PATH, recursive=False)
    for path in paths:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        name = mesh.get_name()
        settings = sub.get_lod_build_settings(mesh, 0)
        current = settings.get_editor_property("build_scale3d")
        new_scale = unreal.Vector(current.x * factor, current.y * factor, current.z * factor)
        settings.set_editor_property("build_scale3d", new_scale)
        sub.set_lod_build_settings(mesh, 0, settings)

        # Las colisiones simples se ajustaron al tamano viejo: se rehacen
        if name in PROPS_WITH_BOX:
            sub.remove_collisions(mesh)
            sub.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        unreal.log(f"[Vestuario] {name}: escala {new_scale.x:.1f} -> mide {mesh_length(mesh):.0f} cm")

    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    except Exception:  # noqa: BLE001
        pass
    unreal.log(f"[Vestuario] Escala corregida (x{factor:g}). Pulsa Play.")


main()
