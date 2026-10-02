# 踏みつけ加速メカゲーム — Phase 8 ゴール・得点・ソロモード用マップ生成スクリプト
#
# 生成・更新するアセット:
#   /Game/Tuning/DA_ScoreTuning          （配点）
#   /Game/Blueprints/BP_Goal             （ゴール）
#   /Game/Blueprints/BP_ImpactGameMode   （配点を割り当て）
#   /Game/Maps/SoloArena                 （ソロモード用マップ）
#
# 事前確認として、拮抗幅（ClashThreshold）の変更が DA_ImpactTuning に実際に効いているかを読み直す。
# C++ の既定値を変えただけでは、アセットに値の上書きが残っていると反映されないため。
# （PROTO-02 で相打ち幅は DA_VehicleTuning の MutualDestructionThreshold から移設された）
#
# SoloArena の配置（PlayerStart は X=0 で +X を向く。ゴールは最奥 X=18000 で -X を向く）:
#
#   X=-2000                                                              X=20000
#   ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ Y=+4000
#   ┃                 [Small]                  [Large]                    ┃
#   ┃                                                   ◀踏み台(Y=+1500)   ┃
#   ┃ ▶自機             ・              [Small]   [Large]  ◀NPC守備  [ゴール]┃
#   ┃                                                   ◀踏み台(Y=-1500)   ┃
#   ┃                 [Small]                  [Large]                    ┃
#   ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ Y=-4000
#
# 踏み台はゴール側から自機側へ行進し、自機はそれを正面から踏みながらゴールへ向かう。
# NPC はゴール前の守備位置に出現し、その周囲の踏み台で加速して迎撃する。
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
MAP_DIR = "/Game/Maps"
MAP_PATH = "/Game/Maps/SoloArena"
CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"
VEHICLE_TUNING_PATH = "/Game/Tuning/DA_VehicleTuning"
IMPACT_TUNING_PATH = "/Game/Tuning/DA_ImpactTuning"
GAMEMODE_BP_PATH = "/Game/Blueprints/BP_ImpactGameMode"

EXPECTED_CLASH_THRESHOLD = 800.0

# ゴールの当たり判定は半径 (300, 600, 400)。Cube(100 uu 立方) を合わせる。
GOAL_MESH_SCALE = unreal.Vector(6.0, 12.0, 8.0)

# 破壊可能オブジェクトの当たり判定は Z 半径 150。中心を Z=150 に置くと床（上面 Z=0）に接地する。
OBSTACLE_Z = 150.0
WALL_Z = 150.0

NO_ROTATION = unreal.Rotator(0.0, 0.0, 0.0)
FACING_PLAYER = unreal.Rotator(0.0, 180.0, 0.0)

# (ラベル, 位置, 回転, スケール) — 床と外周の壁は素の StaticMeshActor（破壊不能）
STATIC_GEOMETRY = [
    ("Arena_Floor", unreal.Vector(9000.0, 0.0, -50.0), NO_ROTATION, unreal.Vector(220.0, 80.0, 1.0)),
    ("Arena_Wall_North", unreal.Vector(9000.0, 4050.0, WALL_Z), NO_ROTATION, unreal.Vector(220.0, 1.0, 3.0)),
    ("Arena_Wall_South", unreal.Vector(9000.0, -4050.0, WALL_Z), NO_ROTATION, unreal.Vector(220.0, 1.0, 3.0)),
    ("Arena_Wall_West", unreal.Vector(-2050.0, 0.0, WALL_Z), NO_ROTATION, unreal.Vector(1.0, 80.0, 3.0)),
    ("Arena_Wall_East", unreal.Vector(20050.0, 0.0, WALL_Z), NO_ROTATION, unreal.Vector(1.0, 80.0, 3.0)),
]

SMALL_OBSTACLES = [
    ("Arena_Small_1", unreal.Vector(6000.0, -2500.0, OBSTACLE_Z)),
    ("Arena_Small_2", unreal.Vector(6000.0, 2500.0, OBSTACLE_Z)),
    ("Arena_Small_3", unreal.Vector(10000.0, 0.0, OBSTACLE_Z)),
]

LARGE_OBSTACLES = [
    ("Arena_Large_1", unreal.Vector(12500.0, 0.0, OBSTACLE_Z)),
    ("Arena_Large_2", unreal.Vector(13500.0, -2800.0, OBSTACLE_Z)),
    ("Arena_Large_3", unreal.Vector(13500.0, 2800.0, OBSTACLE_Z)),
]

STOMP_SPAWNERS = [
    ("Arena_StompSpawner_South", unreal.Vector(15000.0, -1500.0, 40.0)),
    ("Arena_StompSpawner_North", unreal.Vector(15000.0, 1500.0, 40.0)),
]

GOAL_LABEL = "Arena_Goal"
GOAL_LOCATION = unreal.Vector(18000.0, 0.0, 400.0)

PLAYER_START_LABEL = "Arena_PlayerStart"
PLAYER_START_LOCATION = unreal.Vector(0.0, 0.0, 200.0)


def log(message):
    unreal.log("[Phase8Setup] {0}".format(message))


def warn(message):
    unreal.log_warning("[Phase8Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[Phase8Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def set_property(obj, candidates, value):
    """候補名を順に試して設定する。全て外れた場合は明示的に停止する。"""
    for name in candidates:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            continue
    fail("none of {0} is a property of {1}".format(candidates, obj.get_name()))


def preflight_vehicle_tuning():
    rules = load_required(IMPACT_TUNING_PATH)
    value = rules.get_editor_property("clash_threshold")
    log("DA_ImpactTuning.ClashThreshold = {0}".format(value))
    if abs(value - EXPECTED_CLASH_THRESHOLD) > 0.01:
        fail("expected {0}, got {1}; DA_ImpactTuning may hold an override of the C++ default".format(
            EXPECTED_CLASH_THRESHOLD, value))


def create_blueprint(name, parent_class):
    full_path = "{0}/{1}".format(BP_PATH, name)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        return EAL.load_asset(full_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = ASSET_TOOLS.create_asset(name, BP_PATH, unreal.Blueprint, factory)
    if blueprint is None:
        fail("could not create {0}".format(full_path))

    log("created: {0}".format(full_path))
    return blueprint


def create_score_tuning():
    full_path = "{0}/DA_ScoreTuning".format(TUNING_PATH)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        return EAL.load_asset(full_path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ScoreTuningDataAsset)
    asset = ASSET_TOOLS.create_asset("DA_ScoreTuning", TUNING_PATH, unreal.ScoreTuningDataAsset, factory)
    if asset is None:
        fail("could not create {0}".format(full_path))
    if not EAL.save_loaded_asset(asset):
        fail("could not save {0}".format(full_path))

    log("created: {0}".format(full_path))
    return asset


def create_goal_blueprint():
    blueprint = create_blueprint("BP_Goal", unreal.GoalActor)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    mesh = cdo.get_editor_property("body_mesh")
    mesh.set_editor_property("static_mesh", load_required(CUBE_MESH_PATH))
    mesh.set_relative_scale3d(GOAL_MESH_SCALE)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not EAL.save_loaded_asset(blueprint):
        fail("could not save BP_Goal")

    log("BP_Goal: placeholder mesh assigned and scaled to collision size")
    return blueprint


def configure_gamemode(score_tuning):
    gamemode_bp = load_required(GAMEMODE_BP_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    cdo = unreal.get_default_object(gamemode_bp.generated_class())
    set_property(cdo, ("score_tuning",), score_tuning)

    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    if not EAL.save_loaded_asset(gamemode_bp):
        fail("could not save {0}".format(GAMEMODE_BP_PATH))

    log("BP_ImpactGameMode: score tuning assigned")


def open_or_create_map():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    if not EAL.does_directory_exist(MAP_DIR):
        EAL.make_directory(MAP_DIR)
        log("created directory: {0}".format(MAP_DIR))

    if EAL.does_asset_exist(MAP_PATH):
        if not level_subsystem.load_level(MAP_PATH):
            fail("could not load level: {0}".format(MAP_PATH))
        log("loaded existing level: {0}".format(MAP_PATH))
    else:
        if not level_subsystem.new_level(MAP_PATH):
            fail("could not create level: {0}".format(MAP_PATH))
        log("created level: {0}".format(MAP_PATH))

    return level_subsystem


def place_or_update(actor_subsystem, existing, label, actor_class, location, rotation, scale=None):
    actor = existing.get(label)
    if actor is None:
        actor = actor_subsystem.spawn_actor_from_class(actor_class, location, rotation)
        if actor is None:
            fail("could not spawn {0}".format(label))
        actor.set_actor_label(label)
        existing[label] = actor
    else:
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)

    if scale is not None:
        actor.set_actor_scale3d(scale)
    return actor


def make_movable(actor, label):
    """静的ライティングを焼かずに済むよう、ライトは可動にする（Lumen で動的に照らす）。"""
    try:
        actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception as error:
        warn("could not make {0} movable: {1}".format(label, error))


def place_lighting(actor_subsystem, existing):
    sun = place_or_update(actor_subsystem, existing, "Arena_Sun", unreal.DirectionalLight,
                          unreal.Vector(0.0, 0.0, 1000.0), unreal.Rotator(-40.0, -30.0, 0.0))
    make_movable(sun, "Arena_Sun")

    sky_light = place_or_update(actor_subsystem, existing, "Arena_SkyLight", unreal.SkyLight,
                                unreal.Vector(0.0, 0.0, 1000.0), NO_ROTATION)
    make_movable(sky_light, "Arena_SkyLight")
    try:
        # 空の見た目を実行時に取り込む。取り込めなくても機能には影響しないため警告に留める。
        sky_light.light_component.set_editor_property("real_time_capture", True)
    except Exception as error:
        warn("could not enable real-time capture on the sky light: {0}".format(error))

    place_or_update(actor_subsystem, existing, "Arena_Sky", unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0), NO_ROTATION)


def build_arena(goal_bp):
    level_subsystem = open_or_create_map()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = {actor.get_actor_label(): actor for actor in actor_subsystem.get_all_level_actors()}

    cube = load_required(CUBE_MESH_PATH)
    for label, location, rotation, scale in STATIC_GEOMETRY:
        actor = place_or_update(actor_subsystem, existing, label, unreal.StaticMeshActor, location, rotation, scale)
        actor.static_mesh_component.set_static_mesh(cube)

    place_lighting(actor_subsystem, existing)

    place_or_update(actor_subsystem, existing, PLAYER_START_LABEL, unreal.PlayerStart,
                    PLAYER_START_LOCATION, NO_ROTATION)

    place_or_update(actor_subsystem, existing, GOAL_LABEL, goal_bp.generated_class(), GOAL_LOCATION, FACING_PLAYER)

    spawner_class = load_required("{0}/BP_StompTargetSpawner".format(BP_PATH)).generated_class()
    for label, location in STOMP_SPAWNERS:
        place_or_update(actor_subsystem, existing, label, spawner_class, location, FACING_PLAYER)

    small_class = load_required("{0}/BP_DestructibleSmall".format(BP_PATH)).generated_class()
    for label, location in SMALL_OBSTACLES:
        place_or_update(actor_subsystem, existing, label, small_class, location, NO_ROTATION)

    large_class = load_required("{0}/BP_DestructibleLarge".format(BP_PATH)).generated_class()
    for label, location in LARGE_OBSTACLES:
        place_or_update(actor_subsystem, existing, label, large_class, location, NO_ROTATION)

    if not level_subsystem.save_current_level():
        fail("could not save level: {0}".format(MAP_PATH))
    log("saved level: {0}".format(MAP_PATH))

    # 保存後に配置を読み直し、コースの全要素がそろっていることを確認する。
    expected = ([label for label, _, _, _ in STATIC_GEOMETRY]
                + ["Arena_Sun", "Arena_SkyLight", "Arena_Sky", PLAYER_START_LABEL, GOAL_LABEL]
                + [label for label, _ in STOMP_SPAWNERS]
                + [label for label, _ in SMALL_OBSTACLES]
                + [label for label, _ in LARGE_OBSTACLES])
    placed = {actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()}
    missing = [label for label in expected if label not in placed]
    if missing:
        fail("arena actors missing after save: {0}".format(missing))
    log("verified: all {0} arena actors are placed".format(len(expected)))


def verify_assets():
    gamemode_cdo = unreal.get_default_object(load_required(GAMEMODE_BP_PATH).generated_class())
    if gamemode_cdo.get_editor_property("score_tuning") is None:
        fail("BP_ImpactGameMode.score_tuning is unset after save")

    goal_cdo = unreal.get_default_object(load_required("{0}/BP_Goal".format(BP_PATH)).generated_class())
    if goal_cdo.get_editor_property("body_mesh").get_editor_property("static_mesh") is None:
        fail("BP_Goal body mesh is unset after save")

    log("verified: score tuning and goal blueprint are configured")


def main():
    log("=== Phase 8 goal and score setup start ===")
    preflight_vehicle_tuning()
    score_tuning = create_score_tuning()
    goal_bp = create_goal_blueprint()
    configure_gamemode(score_tuning)
    verify_assets()
    build_arena(goal_bp)
    log("=== Phase 8 goal and score setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
