# 踏みつけ加速メカゲーム — Phase 7 敵機アセット生成スクリプト
#
# 生成・更新するアセット:
#   /Game/Tuning/DA_AITuning             （NPC の判断に関わる調整値）
#   /Game/Blueprints/BP_EnemyVehicle     （敵機。機体性能は DA_VehicleTuning を自機と共用）
#   /Game/Blueprints/BP_ImpactGameMode   （敵機クラスと出現位置を設定）
#
# 敵機はマップに配置せず、GameMode が開始時に出現させ、撃破後にリスポーンさせる。
#
# 何度実行しても同じ結果になる（冪等）。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

BP_PATH = "/Game/Blueprints"
TUNING_PATH = "/Game/Tuning"
CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"
VEHICLE_TUNING_PATH = "/Game/Tuning/DA_VehicleTuning"
GAMEMODE_BP_PATH = "/Game/Blueprints/BP_ImpactGameMode"

# 敵機の出現位置。PlayerStart(0,0,200) の左後方に置き、中央レーンの踏み台で加速してから
# プレイヤーへ向かってくるようにする。
ENEMY_SPAWN_LOCATION = unreal.Vector(-3000.0, -1500.0, 200.0)
ENEMY_SPAWN_ROTATION = unreal.Rotator(0.0, 0.0, 0.0)


def log(message):
    unreal.log("[Phase7Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[Phase7Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def set_property(obj, candidates, value):
    """
    候補名を順に試して設定する。

    C++ の識別子から Python 名への変換は略語（AITuning など）で推測が外れやすい。
    誤った名前は例外になるため、全候補が外れた場合は明示的に停止する。
    """
    for name in candidates:
        try:
            obj.set_editor_property(name, value)
            log("  set {0}".format(name))
            return name
        except Exception:
            continue
    fail("none of {0} is a property of {1}".format(candidates, obj.get_name()))


def create_ai_tuning():
    full_path = "{0}/DA_AITuning".format(TUNING_PATH)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        return EAL.load_asset(full_path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.AITuningDataAsset)

    asset = ASSET_TOOLS.create_asset("DA_AITuning", TUNING_PATH, unreal.AITuningDataAsset, factory)
    if asset is None:
        fail("could not create {0}".format(full_path))
    if not EAL.save_loaded_asset(asset):
        fail("could not save {0}".format(full_path))

    log("created: {0}".format(full_path))
    return asset


def create_enemy_vehicle(ai_tuning):
    full_path = "{0}/BP_EnemyVehicle".format(BP_PATH)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        blueprint = EAL.load_asset(full_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.EnemyVehiclePawn)
        blueprint = ASSET_TOOLS.create_asset("BP_EnemyVehicle", BP_PATH, unreal.Blueprint, factory)
        if blueprint is None:
            fail("could not create {0}".format(full_path))
        log("created: {0}".format(full_path))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())

    cdo.get_editor_property("body_mesh").set_editor_property("static_mesh", load_required(CUBE_MESH_PATH))
    set_property(cdo, ("tuning",), load_required(VEHICLE_TUNING_PATH))
    set_property(cdo, ("ai_tuning", "a_i_tuning"), ai_tuning)
    log("BP_EnemyVehicle: placeholder mesh, vehicle tuning and AI tuning assigned")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not EAL.save_loaded_asset(blueprint):
        fail("could not save {0}".format(full_path))
    return blueprint


def configure_gamemode(enemy_bp):
    gamemode_bp = load_required(GAMEMODE_BP_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    cdo = unreal.get_default_object(gamemode_bp.generated_class())

    transform = unreal.Transform(
        location=ENEMY_SPAWN_LOCATION,
        rotation=ENEMY_SPAWN_ROTATION,
        scale=unreal.Vector(1.0, 1.0, 1.0))

    set_property(cdo, ("enemy_vehicle_class",), enemy_bp.generated_class())
    set_property(cdo, ("enemy_spawn_transform",), transform)
    log("BP_ImpactGameMode: enemy class and spawn transform assigned")

    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    if not EAL.save_loaded_asset(gamemode_bp):
        fail("could not save {0}".format(GAMEMODE_BP_PATH))
    return gamemode_bp


def verify(enemy_bp, gamemode_bp):
    """保存後に読み直し、実行時に必要な参照がすべて入っていることを確認する。"""
    enemy_cdo = unreal.get_default_object(load_required("{0}/BP_EnemyVehicle".format(BP_PATH)).generated_class())
    for name in ("tuning",):
        if enemy_cdo.get_editor_property(name) is None:
            fail("BP_EnemyVehicle.{0} is unset after save".format(name))

    ai_tuning_set = False
    for name in ("ai_tuning", "a_i_tuning"):
        try:
            ai_tuning_set = enemy_cdo.get_editor_property(name) is not None
            break
        except Exception:
            continue
    if not ai_tuning_set:
        fail("BP_EnemyVehicle AI tuning is unset after save")

    if enemy_cdo.get_editor_property("body_mesh").get_editor_property("static_mesh") is None:
        fail("BP_EnemyVehicle body mesh is unset after save")

    gamemode_cdo = unreal.get_default_object(load_required(GAMEMODE_BP_PATH).generated_class())
    if gamemode_cdo.get_editor_property("enemy_vehicle_class") is None:
        fail("BP_ImpactGameMode.enemy_vehicle_class is unset after save")

    spawn = gamemode_cdo.get_editor_property("enemy_spawn_transform")
    log("verify: enemy spawn location = {0}".format(spawn.translation))
    log("verified: enemy vehicle and game mode are fully configured")


def main():
    log("=== Phase 7 enemy setup start ===")
    ai_tuning = create_ai_tuning()
    enemy_bp = create_enemy_vehicle(ai_tuning)
    gamemode_bp = configure_gamemode(enemy_bp)
    verify(enemy_bp, gamemode_bp)
    log("=== Phase 7 enemy setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
