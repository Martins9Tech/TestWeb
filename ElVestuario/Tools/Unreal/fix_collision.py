"""
Arregla la colision de los modelos de El Vestuario.

Si al importar los FBX Unreal ignora las cajas UCX_ (pasa con el importador "Interchange"
de las versiones nuevas), los modelos se quedan SIN colision: el jugador y el enemigo caen
al vacio y la pantalla se ve negra.

Este script pone "Use Complex Collision As Simple" en los modelos solidos, de modo que la
propia malla hace de colision. Es seguro ejecutarlo varias veces.

Uso: Herramientas > Ejecutar script de Python... -> Tools/Unreal/fix_collision.py
(build_level.py tambien lo ejecuta automaticamente)
"""
import unreal

MESH_PATH = "/Game/Vestuario/Meshes"

# Modelos contra los que hay que chocar (el resto son decorado fino o no necesitan colision)
SOLID_MESHES = [
    "SM_Room", "SM_PoolStairs", "SM_Locker", "SM_LockerRow", "SM_Bench",
    "SM_ShowerPartition", "SM_ExitDoor", "SM_FusePanel", "SM_Bucket",
]


def fix_mesh(name):
    mesh = unreal.load_asset(f"{MESH_PATH}/{name}.{name}")
    if mesh is None:
        unreal.log_warning(f"[Vestuario] No existe {name}")
        return False

    try:
        body = mesh.get_editor_property("body_setup")
    except Exception:  # noqa: BLE001
        body = None

    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.modify()
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        unreal.log(f"[Vestuario] Colision compleja activada en {name}")
        return True

    # Plan B (si la version de Unreal no expone body_setup): caja simple para los props
    if name != "SM_Room":
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if sub.get_simple_collision_count(mesh) == 0:
            sub.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            unreal.log(f"[Vestuario] Caja de colision anadida a {name}")
            return True
    unreal.log_error(f"[Vestuario] No se pudo arreglar la colision de {name}: hazlo a mano "
                     "(abre el mesh > Collision Complexity > Use Complex Collision As Simple)")
    return False


def main():
    ok = sum(fix_mesh(n) for n in SOLID_MESHES)
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    except Exception:  # noqa: BLE001
        pass
    unreal.log(f"[Vestuario] Colisiones revisadas: {ok}/{len(SOLID_MESHES)}. Pulsa Play para probar.")


main()
