# 踏みつけ加速メカゲーム — Phase 2 基盤アセット生成スクリプト
#
# 生成するアセット:
#   /Game/Input/IA_Move, IA_Brake, IA_Look, IMC_Vehicle
#   /Game/Tuning/DA_VehicleTuning
#   /Game/Blueprints/BP_ImpactVehicle, BP_ImpactPlayerController, BP_ImpactGameMode
# あわせて /Game/Main に床・PlayerStart・ライトを配置する。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

INPUT_PATH = "/Game/Input"
TUNING_PATH = "/Game/Tuning"
BP_PATH = "/Game/Blueprints"
MAP_PATH = "/Game/Main"

CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"

# IMC に登録するキーの一覧。保存後の件数検証に用いる。
EXPECTED_MAPPING_KEYS = ("W", "A", "D", "S", "Mouse2D")

# 床は Cube（100 uu 立方）を Z スケール 1 で使うため、中心を -50 に置くと上面が Z=0 になる。
FLOOR_LOCATION = unreal.Vector(0.0, 0.0, -50.0)

# 機体の当たり判定は半径 50 uu。床の上面（Z=0）から余裕を持たせた高さに置く。
PLAYER_START_LOCATION = unreal.Vector(0.0, 0.0, 200.0)


def log(message):
    unreal.log("[Phase2Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[Phase2Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def ensure_directory(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)
        log("created directory: {0}".format(path))


def enum_value(enum_type, *candidates):
    """候補名を順に試す。全て外れた場合は実在する定数名をログに出して停止する。"""
    for name in candidates:
        value = getattr(enum_type, name, None)
        if value is not None:
            log("enum {0}.{1} resolved".format(enum_type.__name__, name))
            return value

    available = [a for a in dir(enum_type) if a.isupper()]
    fail("no candidate {0} on {1}; available: {2}".format(candidates, enum_type.__name__, available))


def resolve_factory(asset_class, candidate_names):
    """専用ファクトリがあればそれを、なければ DataAssetFactory を使う。"""
    for name in candidate_names:
        factory_class = getattr(unreal, name, None)
        if factory_class is None:
            continue
        try:
            log("using factory: {0}".format(name))
            return factory_class()
        except Exception as error:
            log("factory {0} unavailable: {1}".format(name, error))

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    log("using factory: DataAssetFactory (data_asset_class={0})".format(asset_class.__name__))
    return factory


def create_asset(name, package_path, asset_class, factory):
    full_path = "{0}/{1}".format(package_path, name)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        return EAL.load_asset(full_path)

    asset = ASSET_TOOLS.create_asset(name, package_path, asset_class, factory)
    if asset is None:
        fail("could not create asset: {0}".format(full_path))

    log("created: {0}".format(full_path))
    return asset


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def make_mapping(action, key_name, modifiers):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))
    if modifiers:
        mapping.set_editor_property("modifiers", modifiers)
    return mapping


def make_modifier(modifier_class, owner):
    """
    Input Modifier を生成する。

    unreal.InputModifierXxx() で直接生成すると Transient パッケージに属してしまい、
    IMC アセットの所有物にならないため保存時に参照が破棄される。
    必ず IMC アセット自身を Outer に指定して生成すること。
    """
    return unreal.new_object(modifier_class, outer=owner)


def make_swizzle_yxz(owner):
    """キー入力の既定値は X 軸に乗るため、前進入力を Y 軸へ入れ替える。"""
    modifier = make_modifier(unreal.InputModifierSwizzleAxis, owner)
    modifier.set_editor_property("order", enum_value(unreal.InputAxisSwizzle, "YXZ", "Y_X_Z"))
    return modifier


def create_input_assets():
    ensure_directory(INPUT_PATH)

    action_factory = resolve_factory(unreal.InputAction, ["InputActionFactory"])

    axis2d = enum_value(unreal.InputActionValueType, "AXIS2D", "AXIS2_D", "AXIS_2D")
    boolean = enum_value(unreal.InputActionValueType, "BOOLEAN", "BOOL")

    move = create_asset("IA_Move", INPUT_PATH, unreal.InputAction, action_factory)
    move.set_editor_property("value_type", axis2d)

    look = create_asset("IA_Look", INPUT_PATH, unreal.InputAction, action_factory)
    look.set_editor_property("value_type", axis2d)

    brake = create_asset("IA_Brake", INPUT_PATH, unreal.InputAction, action_factory)
    brake.set_editor_property("value_type", boolean)

    context_factory = resolve_factory(unreal.InputMappingContext, ["InputMappingContextFactory"])
    context = create_asset("IMC_Vehicle", INPUT_PATH, unreal.InputMappingContext, context_factory)

    # X = 旋回（左右）、Y = 前進。S は後退ではなくブレーキに割り当てる。
    mappings = [
        make_mapping(move, "W", [make_swizzle_yxz(context)]),
        make_mapping(move, "A", [make_modifier(unreal.InputModifierNegate, context)]),
        make_mapping(move, "D", None),
        make_mapping(brake, "S", None),
        make_mapping(look, "Mouse2D", None),
    ]
    # 書き込み先は DefaultKeyMappings。旧来の Mappings は UE 5.7 で非推奨となり、
    # 実行時の RebuildControlMappings は DefaultKeyMappings.Mappings しか読まない。
    # 非推奨側に書いても保存・検査は成功して見えるため、取り違えると原因が掴みにくい。
    mapping_data = unreal.InputMappingContextMappingData()
    mapping_data.set_editor_property("mappings", mappings)
    context.set_editor_property("default_key_mappings", mapping_data)

    for asset in (move, look, brake, context):
        EAL.save_loaded_asset(asset)

    verify_mapping_modifiers(context)

    return {"move": move, "look": look, "brake": brake, "context": context}


def verify_mapping_modifiers(context):
    """
    保存後の IMC を読み直し、マッピングが実行時に読まれる場所へ永続化されたかを確認する。

    確認する内容は2点:
      1. DefaultKeyMappings.Mappings に入っていること（実行時が読むのはここだけ）
      2. Modifier が保存されていること（Outer を誤ると黙って破棄される）

    いずれも失敗しても実行時にエラーが出ず「操作が効かない」としか現れないため、
    ここで明示的に検知する。
    """
    context_path = context.get_path_name().split(".")[0]
    EAL.load_asset(context_path)

    mapping_data = context.get_editor_property("default_key_mappings")
    saved_mappings = mapping_data.get_editor_property("mappings")
    modifier_count = sum(len(m.get_editor_property("modifiers")) for m in saved_mappings)

    log("verify: DefaultKeyMappings has {0} mappings, {1} modifiers".format(
        len(saved_mappings), modifier_count))

    if len(saved_mappings) != len(EXPECTED_MAPPING_KEYS):
        fail("expected {0} mappings in DefaultKeyMappings, got {1}".format(
            len(EXPECTED_MAPPING_KEYS), len(saved_mappings)))

    # W の Swizzle と A の Negate の2件が保存されていなければならない。
    if modifier_count < 2:
        fail("input modifiers were not persisted (expected 2, got {0})".format(modifier_count))

    # 非推奨側へ書き込んでいないことを確認する。ここに入っていると実行時に読まれない。
    deprecated_mappings = context.get_editor_property("mappings")
    if len(deprecated_mappings) > 0:
        log("note: deprecated Mappings array still holds {0} entries; clearing it".format(
            len(deprecated_mappings)))
        context.set_editor_property("mappings", [])
        EAL.save_loaded_asset(context)


def create_tuning_asset():
    ensure_directory(TUNING_PATH)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.VehicleTuningDataAsset)

    tuning = create_asset("DA_VehicleTuning", TUNING_PATH, unreal.VehicleTuningDataAsset, factory)
    EAL.save_loaded_asset(tuning)
    return tuning


def create_blueprint(name, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return create_asset(name, BP_PATH, unreal.Blueprint, factory)


def create_blueprints(input_assets, tuning):
    ensure_directory(BP_PATH)

    cube = EAL.load_asset(CUBE_MESH_PATH)
    if cube is None:
        fail("could not load placeholder mesh: {0}".format(CUBE_MESH_PATH))

    vehicle_bp = create_blueprint("BP_ImpactVehicle", unreal.ImpactVehiclePawn)
    unreal.BlueprintEditorLibrary.compile_blueprint(vehicle_bp)
    vehicle_cdo = unreal.get_default_object(vehicle_bp.generated_class())
    vehicle_cdo.set_editor_property("tuning", tuning)
    vehicle_cdo.get_editor_property("body_mesh").set_editor_property("static_mesh", cube)
    log("BP_ImpactVehicle: placeholder mesh and tuning asset assigned")

    controller_bp = create_blueprint("BP_ImpactPlayerController", unreal.ImpactPlayerController)
    unreal.BlueprintEditorLibrary.compile_blueprint(controller_bp)
    controller_cdo = unreal.get_default_object(controller_bp.generated_class())
    controller_cdo.set_editor_property("vehicle_mapping_context", input_assets["context"])
    log("BP_ImpactPlayerController: IMC_Vehicle assigned")

    gamemode_bp = create_blueprint("BP_ImpactGameMode", unreal.ImpactGameMode)
    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    gamemode_cdo = unreal.get_default_object(gamemode_bp.generated_class())
    gamemode_cdo.set_editor_property("default_pawn_class", vehicle_bp.generated_class())
    gamemode_cdo.set_editor_property("player_controller_class", controller_bp.generated_class())
    log("BP_ImpactGameMode: pawn and controller classes assigned")

    for blueprint in (vehicle_bp, controller_bp, gamemode_bp):
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        EAL.save_loaded_asset(blueprint)

    return {"vehicle": vehicle_bp, "controller": controller_bp, "gamemode": gamemode_bp}


def setup_main_level():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not level_subsystem.load_level(MAP_PATH):
        fail("could not load level: {0}".format(MAP_PATH))
    log("loaded level: {0}".format(MAP_PATH))

    existing = actor_subsystem.get_all_level_actors()
    existing_classes = [actor.get_class().get_name() for actor in existing]
    log("existing actors in level: {0}".format(len(existing)))

    # 床は上面を Z=0 に揃える。Cube は 100 uu 立方なので、Z スケール 1 なら中心を -50 に置く。
    floor = next((a for a in existing if a.get_actor_label() == "ProtoFloor"), None)
    if floor is None:
        floor = actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor, FLOOR_LOCATION, unreal.Rotator(0.0, 0.0, 0.0))
        floor.set_actor_label("ProtoFloor")
        floor.static_mesh_component.set_static_mesh(EAL.load_asset(CUBE_MESH_PATH))
        log("spawned ProtoFloor")
    else:
        floor.set_actor_location(FLOOR_LOCATION, False, False)
        log("relocated existing ProtoFloor")

    floor.set_actor_scale3d(unreal.Vector(200.0, 200.0, 1.0))
    log("ProtoFloor: 200x200 m, top surface at Z=0")

    # PlayerStart は床の上に置く。床や機体と干渉するとスポーンが失敗する。
    player_start = next((a for a in existing if a.get_class().get_name() == "PlayerStart"), None)
    if player_start is None:
        actor_subsystem.spawn_actor_from_class(
            unreal.PlayerStart, PLAYER_START_LOCATION, unreal.Rotator(0.0, 0.0, 0.0))
        log("spawned PlayerStart at {0}".format(PLAYER_START_LOCATION))
    else:
        player_start.set_actor_location(PLAYER_START_LOCATION, False, False)
        log("relocated existing PlayerStart to {0}".format(PLAYER_START_LOCATION))

    if "DirectionalLight" not in existing_classes:
        actor_subsystem.spawn_actor_from_class(
            unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0), unreal.Rotator(-45.0, -45.0, 0.0))
        log("spawned DirectionalLight")

    if "SkyLight" not in existing_classes:
        actor_subsystem.spawn_actor_from_class(
            unreal.SkyLight, unreal.Vector(0.0, 0.0, 1000.0), unreal.Rotator(0.0, 0.0, 0.0))
        log("spawned SkyLight")

    if not level_subsystem.save_current_level():
        fail("could not save level: {0}".format(MAP_PATH))
    log("saved level: {0}".format(MAP_PATH))


def main():
    log("=== Phase 2 asset setup start ===")
    input_assets = create_input_assets()
    tuning = create_tuning_asset()
    create_blueprints(input_assets, tuning)
    setup_main_level()
    log("=== Phase 2 asset setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
