# 踏みつけ加速メカゲーム — PROTO-02（追突・ロックオン仕様）のアセット設定スクリプト
#
# 生成・更新するアセット:
#   /Game/Tuning/DA_ImpactTuning        （追突・面判定・ロックオンの規則。全機体で共用する）
#   /Game/Materials/M_Bubble            （バブルの半透明マテリアル）
#   /Game/Blueprints/BP_ImpactVehicle   （規則・バブルのメッシュ・壁と床の当たり判定の大きさ）
#   /Game/Blueprints/BP_EnemyVehicle    （同上）
#   /Game/Tuning/DA_VehicleTuning       （機体同士の接触半径・カメラ距離）
#   /Game/Blueprints/BP_ImpactGameMode  （敵機の出現位置の高さ）
#   /Game/Maps/SoloArena                （PlayerStart の高さ）
#
# 壁・床との当たり判定は本体のモデル（BodyMesh）の大きさを基準にする。バブルの半径は機体同士の
# 接触判定にだけ使い、壁や床を押し出さない。スポーン時の干渉退避は Blueprint 既定値の形状で行われるため、
# モデルから求めた当たり判定の大きさを Blueprint にも設定する（CLAUDE.md §5-8）。
#
# 何度実行しても同じ結果になる（冪等）。既にある規則アセットの数値は上書きしない。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

BP_PATH = "/Game/Blueprints"
TUNING_PATH = "/Game/Tuning"
MATERIAL_PATH = "/Game/Materials"
MAP_PATH = "/Game/Maps/SoloArena"

IMPACT_TUNING_NAME = "DA_ImpactTuning"
IMPACT_TUNING_PATH = "{0}/{1}".format(TUNING_PATH, IMPACT_TUNING_NAME)
VEHICLE_TUNING_PATH = "{0}/DA_VehicleTuning".format(TUNING_PATH)
BUBBLE_MATERIAL_NAME = "M_Bubble"
BUBBLE_MATERIAL_PATH = "{0}/{1}".format(MATERIAL_PATH, BUBBLE_MATERIAL_NAME)
GAMEMODE_BP_PATH = "{0}/BP_ImpactGameMode".format(BP_PATH)
SPHERE_MESH_PATH = "/Engine/BasicShapes/Sphere.Sphere"

# 規則アセットを割り当てる機体の Blueprint。自機と敵機は同じ規則で判定する。
VEHICLE_BP_PATHS = (
    "{0}/BP_ImpactVehicle".format(BP_PATH),
    "{0}/BP_EnemyVehicle".format(BP_PATH),
)

# バブルのマテリアルが持つパラメータ。C++（ImpactVehiclePawn）が同じ名前で参照する。
BUBBLE_COLOR_PARAMETER = "BubbleColor"
BUBBLE_OPACITY_PARAMETER = "Opacity"
BUBBLE_EMISSIVE_PARAMETER = "EmissiveStrength"
BUBBLE_DEFAULT_COLOR = unreal.LinearColor(0.15, 0.55, 1.0, 1.0)
BUBBLE_DEFAULT_OPACITY = 0.25
BUBBLE_DEFAULT_EMISSIVE = 0.5

# 機体同士の接触半径（uu）。現在は本体のプレースホルダ（Cube 100 uu 立方）に合わせる。
# 本番モデルの導入時に合わせ直す。
BUBBLE_RADIUS = 50.0

# カメラ距離（uu）。当たり判定を本体のモデル基準に戻したため、PROTO-01 の値に戻す。
CAMERA_ARM_AT_REST = 700.0
CAMERA_ARM_AT_TOP_SPEED = 1100.0

# 出現位置の高さ（uu）。本体のモデル基準の当たり判定に合わせ、PROTO-01 の値に戻す。
SPAWN_HEIGHT = 200.0

# 高さ・大きさを同じとみなす許容誤差（uu）。
TOLERANCE = 0.01

# 確認のためにログへ出す規則の値。仕様 4章の暫定値と一致しているかを起動時に読める形にする。
VERIFIED_RULE_PROPERTIES = (
    "overdrive_speed",
    "clash_threshold",
    "facing_power_side",
    "facing_power_back",
    "restitution",
    "lock_on_cone_angle",
    "assist_turn_rate",
)


def log(message):
    unreal.log("[PROTO02Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[PROTO02Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def set_property(obj, candidates, value):
    """
    候補名を順に試して設定する。

    C++ の識別子から Python 名への変換は略語で推測が外れやすい。
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


def save_asset(asset, path):
    if not EAL.save_loaded_asset(asset):
        fail("could not save {0}".format(path))


def create_impact_tuning():
    if EAL.does_asset_exist(IMPACT_TUNING_PATH):
        log("already exists: {0}".format(IMPACT_TUNING_PATH))
        return load_required(IMPACT_TUNING_PATH)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ImpactTuningDataAsset)

    asset = ASSET_TOOLS.create_asset(IMPACT_TUNING_NAME, TUNING_PATH, unreal.ImpactTuningDataAsset, factory)
    if asset is None:
        fail("could not create {0}".format(IMPACT_TUNING_PATH))
    save_asset(asset, IMPACT_TUNING_PATH)

    log("created: {0}".format(IMPACT_TUNING_PATH))
    return asset


def create_bubble_material():
    """バブルの半透明マテリアルを作る。色・不透明度・発光はパラメータにし、C++ から実行時に変える。"""
    if EAL.does_asset_exist(BUBBLE_MATERIAL_PATH):
        log("already exists: {0}".format(BUBBLE_MATERIAL_PATH))
        return load_required(BUBBLE_MATERIAL_PATH)

    if not EAL.does_directory_exist(MATERIAL_PATH):
        EAL.make_directory(MATERIAL_PATH)
        log("created directory: {0}".format(MATERIAL_PATH))

    material = ASSET_TOOLS.create_asset(
        BUBBLE_MATERIAL_NAME, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        fail("could not create {0}".format(BUBBLE_MATERIAL_PATH))

    # 中の機体が透けて見えるようにする。球の裏面も描いて輪郭が途切れないようにする。
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)

    color = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", BUBBLE_COLOR_PARAMETER)
    color.set_editor_property("default_value", BUBBLE_DEFAULT_COLOR)

    opacity = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 250)
    opacity.set_editor_property("parameter_name", BUBBLE_OPACITY_PARAMETER)
    opacity.set_editor_property("default_value", BUBBLE_DEFAULT_OPACITY)

    emissive = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 400)
    emissive.set_editor_property("parameter_name", BUBBLE_EMISSIVE_PARAMETER)
    emissive.set_editor_property("default_value", BUBBLE_DEFAULT_EMISSIVE)

    # 超加速の表示（Phase 6）は、この発光の強さを上げて示す。
    glow = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 300)
    MEL.connect_material_expressions(color, "", glow, "A")
    MEL.connect_material_expressions(emissive, "", glow, "B")

    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    MEL.recompile_material(material)
    save_asset(material, BUBBLE_MATERIAL_PATH)

    log("created: {0}".format(BUBBLE_MATERIAL_PATH))
    return material


def model_collision_extent(cdo):
    """本体のモデル（BodyMesh）の境界から、壁・床との当たり判定の大きさを求める。C++ の ApplyCollisionFromModel と同じ計算。"""
    body_mesh = cdo.get_editor_property("body_mesh")
    model = body_mesh.get_editor_property("static_mesh")
    if model is None:
        fail("{0} has no body mesh; collision cannot follow the model".format(cdo.get_name()))

    extent = model.get_bounds().box_extent
    scale = body_mesh.get_editor_property("relative_scale3d")
    return unreal.Vector(extent.x * abs(scale.x), extent.y * abs(scale.y), extent.z * abs(scale.z))


def configure_vehicle_blueprint(blueprint_path, impact_tuning, material, sphere_mesh):
    """機体の Blueprint に規則アセット・バブルの見た目・壁と床の当たり判定の大きさを設定する。"""
    blueprint = load_required(blueprint_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    set_property(cdo, ("impact_tuning",), impact_tuning)

    bubble_mesh = cdo.get_editor_property("bubble_mesh")
    bubble_mesh.set_editor_property("static_mesh", sphere_mesh)
    bubble_mesh.set_editor_property("override_materials", [material])

    # スポーン時の干渉退避は Blueprint 既定値の形状で行われるため、モデルから求めた大きさを入れる。
    extent = model_collision_extent(cdo)
    cdo.get_editor_property("collision_box").set_editor_property("box_extent", extent)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save_asset(blueprint, blueprint_path)

    log("{0}: rules, bubble mesh and model collision extent {1} assigned".format(blueprint_path, extent))
    return blueprint


def update_vehicle_tuning(vehicle_tuning):
    """機体同士の接触半径とカメラ距離を設定する。"""
    before_radius = vehicle_tuning.get_editor_property("bubble_radius")
    vehicle_tuning.set_editor_property("bubble_radius", BUBBLE_RADIUS)
    log("DA_VehicleTuning: bubble radius {0} -> {1}".format(before_radius, BUBBLE_RADIUS))

    before_rest = vehicle_tuning.get_editor_property("camera_arm_length_at_rest")
    before_top = vehicle_tuning.get_editor_property("camera_arm_length_at_top_speed")

    vehicle_tuning.set_editor_property("camera_arm_length_at_rest", CAMERA_ARM_AT_REST)
    vehicle_tuning.set_editor_property("camera_arm_length_at_top_speed", CAMERA_ARM_AT_TOP_SPEED)
    save_asset(vehicle_tuning, VEHICLE_TUNING_PATH)

    log("DA_VehicleTuning: camera arm ({0}, {1}) -> ({2}, {3})".format(
        before_rest, before_top, CAMERA_ARM_AT_REST, CAMERA_ARM_AT_TOP_SPEED))


def update_enemy_spawn_height():
    """敵機の出現位置の高さを、本体のモデル基準の当たり判定に合わせる。"""
    gamemode_bp = load_required(GAMEMODE_BP_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)

    cdo = unreal.get_default_object(gamemode_bp.generated_class())
    transform = cdo.get_editor_property("enemy_spawn_transform")
    # translation は参照で返るため、書き換える前に値を控える（ログが同じ値になるのを防ぐ）。
    location = unreal.Vector(transform.translation.x, transform.translation.y, transform.translation.z)
    if abs(location.z - SPAWN_HEIGHT) <= TOLERANCE:
        log("enemy spawn height is already {0}".format(location.z))
        return

    transform.translation = unreal.Vector(location.x, location.y, SPAWN_HEIGHT)
    cdo.set_editor_property("enemy_spawn_transform", transform)

    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    save_asset(gamemode_bp, GAMEMODE_BP_PATH)
    log("BP_ImpactGameMode: enemy spawn height {0} -> {1}".format(location.z, SPAWN_HEIGHT))


def update_player_start_height():
    """自機の出現位置の高さを、本体のモデル基準の当たり判定に合わせる。"""
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.load_level(MAP_PATH):
        fail("could not load level: {0}".format(MAP_PATH))

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    starts = [actor for actor in actor_subsystem.get_all_level_actors()
              if isinstance(actor, unreal.PlayerStart)]
    if not starts:
        fail("no PlayerStart in {0}".format(MAP_PATH))

    changed = False
    for start in starts:
        location = start.get_actor_location()
        if abs(location.z - SPAWN_HEIGHT) <= TOLERANCE:
            log("{0}: height is already {1}".format(start.get_actor_label(), location.z))
            continue
        start.set_actor_location(unreal.Vector(location.x, location.y, SPAWN_HEIGHT), False, True)
        changed = True
        log("{0}: height {1} -> {2}".format(start.get_actor_label(), location.z, SPAWN_HEIGHT))

    if changed and not level_subsystem.save_current_level():
        fail("could not save level: {0}".format(MAP_PATH))


def verify():
    """保存後に読み直し、実行時に必要な参照と値がすべて入っていることを確認する。"""
    rules = load_required(IMPACT_TUNING_PATH)
    for name in VERIFIED_RULE_PROPERTIES:
        log("verify: {0}.{1} = {2}".format(IMPACT_TUNING_NAME, name, rules.get_editor_property(name)))

    radius = load_required(VEHICLE_TUNING_PATH).get_editor_property("bubble_radius")
    if abs(radius - BUBBLE_RADIUS) > TOLERANCE:
        fail("DA_VehicleTuning.bubble_radius is {0} after save".format(radius))

    for blueprint_path in VEHICLE_BP_PATHS:
        cdo = unreal.get_default_object(load_required(blueprint_path).generated_class())
        if cdo.get_editor_property("impact_tuning") is None:
            fail("{0}.impact_tuning is unset after save".format(blueprint_path))
        if cdo.get_editor_property("tuning") is None:
            fail("{0}.tuning is unset after save".format(blueprint_path))

        bubble_mesh = cdo.get_editor_property("bubble_mesh")
        if bubble_mesh.get_editor_property("static_mesh") is None:
            fail("{0} bubble mesh is unset after save".format(blueprint_path))
        if not bubble_mesh.get_editor_property("override_materials"):
            fail("{0} bubble material is unset after save".format(blueprint_path))

        expected = model_collision_extent(cdo)
        actual = cdo.get_editor_property("collision_box").get_editor_property("box_extent")
        if (abs(actual.x - expected.x) > TOLERANCE or abs(actual.y - expected.y) > TOLERANCE
                or abs(actual.z - expected.z) > TOLERANCE):
            fail("{0} collision extent {1} does not match the body model {2}".format(blueprint_path, actual, expected))

        log("verified: {0} (model collision extent {1})".format(blueprint_path, actual))


def main():
    log("=== PROTO-02 asset setup start ===")
    vehicle_tuning = load_required(VEHICLE_TUNING_PATH)

    impact_tuning = create_impact_tuning()
    material = create_bubble_material()
    sphere_mesh = load_required(SPHERE_MESH_PATH)

    for blueprint_path in VEHICLE_BP_PATHS:
        configure_vehicle_blueprint(blueprint_path, impact_tuning, material, sphere_mesh)

    update_vehicle_tuning(vehicle_tuning)
    update_enemy_spawn_height()
    update_player_start_height()
    verify()
    log("=== PROTO-02 asset setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
