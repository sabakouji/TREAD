# 踏みつけ加速メカゲーム — Phase 6 衝突テストコース生成スクリプト
#
# 生成するアセット:
#   /Game/Blueprints/BP_DestructibleSmall  （小物: 1500 uu/s で破壊）
#   /Game/Blueprints/BP_DestructibleLarge  （壁・建物: 3200 uu/s で破壊）
# あわせて /Game/Main に衝突テスト用のコースを配置する。
#
# コース配置（PlayerStart は (0,0,200) で +X を向く。スポナーは X=4000）:
#
#          Y=-1500        Y=0          Y=+1500       Y=+3000
#   X=6000 [Small]      [Large]     [破壊不能ブロック]   ┃
#                                                      ┃ 破壊不能の側壁
#                                                      ┃ （X=1000〜9000）
#                  ↑ 中央レーンで踏み台を踏んで加速     ┃
#
# 中央レーンで加速してからレーンを選び、各判定を個別に試せるようにしている。
# 同一レーンに直列に並べると、破壊成功時の減速ペナルティで後続の判定が試せなくなるため。
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

# 破壊可能オブジェクトの当たり判定は半径 (100, 100, 150)。Cube(100 uu 立方) を合わせる。
OBSTACLE_MESH_SCALE = unreal.Vector(2.0, 2.0, 3.0)

# 当たり判定の Z 半径は 150 なので、中心を Z=150 に置くと床（上面 Z=0）に接地する。
OBSTACLE_Z = 150.0

# (ラベル, 種別, 位置, 破壊不能壁のスケール)
# 種別: "small" / "large" は破壊可能、"wall" は破壊不能（ADestructibleObstacleActor を継承しない）
COURSE = [
    ("ProtoSmall_1", "small", unreal.Vector(6000.0, -1500.0, OBSTACLE_Z), None),
    ("ProtoLarge_1", "large", unreal.Vector(6000.0, 0.0, OBSTACLE_Z), None),
    ("ProtoWall_Front", "wall", unreal.Vector(6000.0, 1500.0, OBSTACLE_Z), unreal.Vector(2.0, 4.0, 3.0)),
    ("ProtoWall_Side", "wall", unreal.Vector(5000.0, 3000.0, OBSTACLE_Z), unreal.Vector(80.0, 1.0, 3.0)),
]


def log(message):
    unreal.log("[Phase6Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[Phase6Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def enum_value(enum_type, *candidates):
    """候補名を順に試す。全て外れた場合は実在する定数名をログに出して停止する。"""
    for name in candidates:
        value = getattr(enum_type, name, None)
        if value is not None:
            return value

    available = [a for a in dir(enum_type) if a.isupper()]
    fail("no candidate {0} on {1}; available: {2}".format(candidates, enum_type.__name__, available))


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


def create_destructible(name, rank):
    blueprint = create_blueprint(name, unreal.DestructibleObstacleActor)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("rank", rank)

    mesh = cdo.get_editor_property("body_mesh")
    mesh.set_editor_property("static_mesh", load_required(CUBE_MESH_PATH))
    mesh.set_relative_scale3d(OBSTACLE_MESH_SCALE)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not EAL.save_loaded_asset(blueprint):
        fail("could not save {0}".format(name))

    log("{0}: rank and placeholder mesh assigned".format(name))
    return blueprint


def place_or_relocate(actor_subsystem, existing, label, actor_class, location):
    actor = next((a for a in existing if a.get_actor_label() == label), None)
    if actor is None:
        actor = actor_subsystem.spawn_actor_from_class(actor_class, location, unreal.Rotator(0.0, 0.0, 0.0))
        actor.set_actor_label(label)
        log("spawned {0}".format(label))
    else:
        actor.set_actor_location(location, False, False)
        log("relocated existing {0}".format(label))
    return actor


def place_course(small_bp, large_bp):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not level_subsystem.load_level(MAP_PATH):
        fail("could not load level: {0}".format(MAP_PATH))

    existing = actor_subsystem.get_all_level_actors()
    cube = load_required(CUBE_MESH_PATH)

    for label, kind, location, wall_scale in COURSE:
        if kind == "small":
            place_or_relocate(actor_subsystem, existing, label, small_bp.generated_class(), location)
        elif kind == "large":
            place_or_relocate(actor_subsystem, existing, label, large_bp.generated_class(), location)
        else:
            # 破壊不能の壁は素の StaticMeshActor とする。
            # ADestructibleObstacleActor を継承しないため、衝突解決では破壊不能として扱われる。
            wall = place_or_relocate(actor_subsystem, existing, label, unreal.StaticMeshActor, location)
            wall.static_mesh_component.set_static_mesh(cube)
            wall.set_actor_scale3d(wall_scale)

    if not level_subsystem.save_current_level():
        fail("could not save level: {0}".format(MAP_PATH))
    log("saved level: {0}".format(MAP_PATH))

    # 配置結果を読み直して、コースの全要素が存在することを確認する。
    placed_labels = {a.get_actor_label() for a in actor_subsystem.get_all_level_actors()}
    missing = [label for label, _, _, _ in COURSE if label not in placed_labels]
    if missing:
        fail("course actors missing after save: {0}".format(missing))
    log("verified: all {0} course actors are placed".format(len(COURSE)))


def main():
    log("=== Phase 6 impact course setup start ===")

    small_rank = enum_value(unreal.DestructionRank, "SMALL")
    large_rank = enum_value(unreal.DestructionRank, "LARGE")

    small_bp = create_destructible("BP_DestructibleSmall", small_rank)
    large_bp = create_destructible("BP_DestructibleLarge", large_rank)
    place_course(small_bp, large_bp)

    log("=== Phase 6 impact course setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
