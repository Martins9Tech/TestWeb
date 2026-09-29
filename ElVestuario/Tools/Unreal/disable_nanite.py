"""
Desactiva Nanite en todos los modelos de El Vestuario.

Unreal 5.8 importa los FBX con Nanite activado. Con estos modelos low-poly (cajas grandes)
la sala se ve desde fuera pero desaparece al entrar. Sin Nanite se renderizan como mallas
normales y el problema desaparece.

Uso: Herramientas > Ejecutar script de Python -> Tools/Unreal/disable_nanite.py
(build_level.py tambien lo ejecuta automaticamente)
"""
import unreal

MESH_PATH = "/Game/Vestuario/Meshes"


def main():
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    paths = unreal.EditorAssetLibrary.list_assets(MESH_PATH, recursive=False)
    changed = 0
    for path in paths:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        name = mesh.get_name()
        try:
            settings = mesh.get_editor_property("nanite_settings")
            was = settings.get_editor_property("enabled")
            if was:
                settings.set_editor_property("enabled", False)
                try:
                    sub.set_nanite_settings(mesh, settings, True)
                except Exception:  # noqa: BLE001
                    mesh.set_editor_property("nanite_settings", settings)
                unreal.EditorAssetLibrary.save_loaded_asset(mesh)
                changed += 1
            unreal.log(f"[Vestuario] {name}: Nanite {'ACTIVADO -> desactivado' if was else 'ya estaba desactivado'}")
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning(f"[Vestuario] {name}: no se pudo leer Nanite ({exc})")
    unreal.log(f"[Vestuario] Nanite desactivado en {changed} modelos. Pulsa Play.")


main()
