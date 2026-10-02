# 踏みつけ加速メカゲーム — Phase 5 踏み台アセット生成スクリプト
#
# 生成するアセット:
#   /Game/Blueprints/BP_StompTarget         （踏み台となる戦車）
#   /Game/Blueprints/BP_StompTargetSpawner  （踏み台の供給拠点）
# あわせて /Game/Main にスポナーを配置する。
#
# 何度実行しても同じ結果になる（冪等）。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

BP_PATH = "/Game/Blueprints"
MAP_PATH = "/Game/Main"
CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"

# 踏み台は当たり判定 120x70x40 uu。Cube(100 uu 立方) を判定に合わせて潰す。
STOMP_TARGET_MESH_SCALE = unreal.Vector(2.4, 1.4, 0.8)

# PlayerStart(0,0,200) の前方に置き、機体へ向かって行進させる。
# 踏み台の当たり判定は Z 半径 40 なので、中心を Z=40 に置くと床（上面 Z=0）に接地する。
SPAWNER_LOCATION = unreal.Vector(4000.0, 0.0, 40.0)
SPAWNER_ROTATION = unreal.Rotator(0.0, 180.0, 0.0)
SPAWNER_LABEL = "ProtoStompSpawner"


def log(message):
    unreal.log("[Phase5Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[Phase5Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def create_blueprint(name, parent_class):
    full_path = "{0}/{1}".format(BP_PATH, name)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        return EAL.load_asset(full_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    blueprint = ASSET_TOOLS.create_asset(name, BP_PATH, unreal.Blueprint, factory)
    if blueprint is None:
        fail("could not create blueprint: {0}".format(full_path))

    log("created: {0}".format(full_path))
    return blueprint


def create_stomp_target():
    blueprint = create_blueprint("BP_StompTarget", unreal.StompTargetActor)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    mesh = cdo.get_editor_property("body_mesh")
    mesh.set_editor_property("static_mesh", load_required(CUBE_MESH_PATH))
    mesh.set_relative_scale3d(STOMP_TARGET_MESH_SCALE)
    log("BP_StompTarget: placeholder mesh assigned and scaled to collision size")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not EAL.save_loaded_asset(blueprint):
        fail("could not save BP_StompTarget")
    return blueprint


def create_spawner(stomp_target_bp):
    blueprint = create_blueprint("BP_StompTargetSpawner", unreal.StompTargetSpawner)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("stomp_target_class", stomp_target_bp.generated_class())
    log("BP_StompTargetSpawner: stomp_target_class assigned")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not EAL.save_loaded_asset(blueprint):
        fail("could not save BP_StompTargetSpawner")
    return blueprint


def place_spawner_in_level(spawner_bp):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not level_subsystem.load_level(MAP_PATH):
        fail("could not load level: {0}".format(MAP_PATH))

    existing = actor_subsystem.get_all_level_actors()
    spawner = next((a for a in existing if a.get_actor_label() == SPAWNER_LABEL), None)

    if spawner is None:
        spawner = actor_subsystem.spawn_actor_from_class(
            spawner_bp.generated_class(), SPAWNER_LOCATION, SPAWNER_ROTATION)
        spawner.set_actor_label(SPAWNER_LABEL)
        log("spawned {0}".format(SPAWNER_LABEL))
    else:
        spawner.set_actor_location(SPAWNER_LOCATION, False, False)
        spawner.set_actor_rotation(SPAWNER_ROTATION, False)
        log("relocated existing {0}".format(SPAWNER_LABEL))

    if not level_subsystem.save_current_level():
        fail("could not save level: {0}".format(MAP_PATH))
    log("saved level: {0}".format(MAP_PATH))


def main():
    log("=== Phase 5 stomp target setup start ===")
    stomp_target_bp = create_stomp_target()
    spawner_bp = create_spawner(stomp_target_bp)
    place_spawner_in_level(spawner_bp)
    log("=== Phase 5 stomp target setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
