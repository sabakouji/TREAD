# 踏みつけ加速メカゲーム — FIELD-02（建物の分割必須化と小物の追加）のアセット設定スクリプト
#
# 生成・更新するアセット:
#   /Game/Blueprints/BP_DestructibleSmall  （Destructible.DestructionGC = GC_DestructibleLarge。未設定のときだけ割り当てる）
#   /Game/Blueprints/BP_BreakableProp      （分割しない小物。仮の見た目は細長い Cylinder）
#
# BP_DestructibleSmall は今の仮の見た目が Large と同じ直方体（200 × 200 × 300 uu）のため、Large 用の Geometry Collection を流用する。
# 本番の見た目のモデルを入れる段階で、建物ごとに Geometry Collection を作り直す。
#
# 併せて、ADestructibleObstacleActor を継承するすべての Blueprint に DestructionGC が割り当てられているかを検証する
# （フィールド上の建物はすべて分割する方針のため）。未設定があれば一覧を出して失敗する。
#
# 何度実行しても同じ結果になる（冪等）。
#
# 実行方法:
#   UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4 -nullrhi

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

BP_PATH = "/Game/Blueprints"
GC_PATH = "/Game/Destruction/GC_DestructibleLarge"
CYLINDER_MESH_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"

# 小物の当たり判定は半径 (20, 20, 200)。Cylinder（直径 100・高さ 100 uu）を合わせる。
PROP_MESH_SCALE = unreal.Vector(0.4, 0.4, 4.0)


def log(message):
    unreal.log("[FIELD02Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[FIELD02Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def compiled_cdo(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return unreal.get_default_object(blueprint.generated_class())


def save(asset, name):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        fail("could not save {0}".format(name))
    log("saved: {0}".format(name))


def assign_gc_to_small(geometry_collection):
    blueprint = load_required("{0}/BP_DestructibleSmall".format(BP_PATH))
    destructible = compiled_cdo(blueprint).get_editor_property("destructible")
    if destructible.get_editor_property("destruction_gc") is None:
        destructible.set_editor_property("destruction_gc", geometry_collection)
        save(blueprint, "BP_DestructibleSmall")
        log("BP_DestructibleSmall: destruction_gc = {0}".format(GC_PATH))
    else:
        log("BP_DestructibleSmall: destruction_gc already assigned (kept)")


def ensure_prop_blueprint():
    name = "BP_BreakableProp"
    full_path = "{0}/{1}".format(BP_PATH, name)
    if EAL.does_asset_exist(full_path):
        blueprint = EAL.load_asset(full_path)
        log("already exists: {0}".format(full_path))
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.BreakablePropActor)
        blueprint = ASSET_TOOLS.create_asset(name, BP_PATH, unreal.Blueprint, factory)
        if blueprint is None:
            fail("could not create {0}".format(full_path))
        log("created: {0}".format(full_path))

    cdo = compiled_cdo(blueprint)
    mesh = cdo.get_editor_property("body_mesh")
    if mesh.get_editor_property("static_mesh") is None:
        mesh.set_editor_property("static_mesh", load_required(CYLINDER_MESH_PATH))
        mesh.set_relative_scale3d(PROP_MESH_SCALE)
        log("{0}: placeholder cylinder assigned".format(name))
    save(blueprint, name)


def verify_all_buildings_fracture():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    parent = unreal.DestructibleObstacleActor.static_class()
    missing = []
    checked = 0
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_class_path.asset_name) != "Blueprint":
            continue
        blueprint = data.get_asset()
        generated = blueprint.generated_class() if blueprint else None
        if generated is None or not unreal.MathLibrary.class_is_child_of(generated, parent):
            continue
        checked += 1
        destructible = unreal.get_default_object(generated).get_editor_property("destructible")
        if destructible is None or destructible.get_editor_property("destruction_gc") is None:
            missing.append(str(data.package_name))

    if missing:
        fail("buildings without DestructionGC: {0}".format(", ".join(missing)))
    log("verified: all {0} building blueprints have DestructionGC".format(checked))


def main():
    geometry_collection = load_required(GC_PATH)
    assign_gc_to_small(geometry_collection)
    ensure_prop_blueprint()
    verify_all_buildings_fracture()
    log("done")


main()
