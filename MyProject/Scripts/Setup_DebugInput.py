# 踏みつけ加速メカゲーム — デバッグ用入力アセット生成スクリプト（DEBUG-01）
#
# 生成・更新するアセット:
#   /Game/Input/IA_DebugBoost       （Q: 踏みつけ1回分の加速）
#   /Game/Input/IA_DebugToggleHold  （R: 加速の維持を切り替え）
#   /Game/Input/IMC_Debug           （デバッグ専用の Mapping Context）
#   /Game/Blueprints/BP_ImpactVehicle            （上記 Input Action を割り当て）
#   /Game/Blueprints/BP_ImpactPlayerController   （IMC_Debug を割り当て）
#
# デバッグ用キーは本番の IMC_Vehicle に混ぜず、専用の Context に分ける。
#
# CLAUDE.md 5-1・5-2 の教訓に従う:
#   - マッピングは非推奨の Mappings ではなく DefaultKeyMappings に書き込む
#   - 保存後に読み直して、実行時に読まれる場所へ入っていることを検証する
#
# 何度実行しても同じ結果になる（冪等）。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

INPUT_PATH = "/Game/Input"
VEHICLE_BP_PATH = "/Game/Blueprints/BP_ImpactVehicle"
CONTROLLER_BP_PATH = "/Game/Blueprints/BP_ImpactPlayerController"

# (Input Action 名, キー, Pawn のプロパティ名の候補)
DEBUG_ACTIONS = [
    ("IA_DebugBoost", "Q", ("debug_boost_action",)),
    ("IA_DebugToggleHold", "R", ("debug_toggle_hold_action",)),
]


def log(message):
    unreal.log("[DebugInputSetup] {0}".format(message))


def fail(message):
    unreal.log_error("[DebugInputSetup] FAILED: {0}".format(message))
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


def set_property(obj, candidates, value):
    """候補名を順に試して設定する。全て外れた場合は明示的に停止する。"""
    for name in candidates:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            continue
    fail("none of {0} is a property of {1}".format(candidates, obj.get_name()))


def get_property(obj, candidates):
    for name in candidates:
        try:
            return obj.get_editor_property(name)
        except Exception:
            continue
    fail("none of {0} is a property of {1}".format(candidates, obj.get_name()))


def create_data_asset(name, asset_class):
    full_path = "{0}/{1}".format(INPUT_PATH, name)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        return EAL.load_asset(full_path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = ASSET_TOOLS.create_asset(name, INPUT_PATH, asset_class, factory)
    if asset is None:
        fail("could not create {0}".format(full_path))

    log("created: {0}".format(full_path))
    return asset


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def make_mapping(action, key_name):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))
    return mapping


def create_debug_input():
    boolean = enum_value(unreal.InputActionValueType, "BOOLEAN", "BOOL")

    actions = {}
    for action_name, _, _ in DEBUG_ACTIONS:
        action = create_data_asset(action_name, unreal.InputAction)
        action.set_editor_property("value_type", boolean)
        if not EAL.save_loaded_asset(action):
            fail("could not save {0}".format(action_name))
        actions[action_name] = action

    context = create_data_asset("IMC_Debug", unreal.InputMappingContext)

    # 実行時が読むのは DefaultKeyMappings のみ（CLAUDE.md 5-1）。
    mappings = [make_mapping(actions[name], key) for name, key, _ in DEBUG_ACTIONS]
    mapping_data = unreal.InputMappingContextMappingData()
    mapping_data.set_editor_property("mappings", mappings)
    context.set_editor_property("default_key_mappings", mapping_data)
    context.set_editor_property("mappings", [])

    if not EAL.save_loaded_asset(context):
        fail("could not save IMC_Debug")

    saved = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    if len(saved) != len(DEBUG_ACTIONS):
        fail("expected {0} mappings in IMC_Debug.DefaultKeyMappings, got {1}".format(len(DEBUG_ACTIONS), len(saved)))
    log("IMC_Debug: {0} mappings in DefaultKeyMappings".format(len(saved)))

    return actions, context


def assign_to_blueprints(actions, context):
    vehicle_bp = load_required(VEHICLE_BP_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(vehicle_bp)
    vehicle_cdo = unreal.get_default_object(vehicle_bp.generated_class())
    for action_name, _, property_candidates in DEBUG_ACTIONS:
        name = set_property(vehicle_cdo, property_candidates, actions[action_name])
        log("BP_ImpactVehicle.{0} = {1}".format(name, action_name))
    unreal.BlueprintEditorLibrary.compile_blueprint(vehicle_bp)
    if not EAL.save_loaded_asset(vehicle_bp):
        fail("could not save BP_ImpactVehicle")

    controller_bp = load_required(CONTROLLER_BP_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(controller_bp)
    controller_cdo = unreal.get_default_object(controller_bp.generated_class())
    name = set_property(controller_cdo, ("debug_mapping_context",), context)
    log("BP_ImpactPlayerController.{0} = IMC_Debug".format(name))
    unreal.BlueprintEditorLibrary.compile_blueprint(controller_bp)
    if not EAL.save_loaded_asset(controller_bp):
        fail("could not save BP_ImpactPlayerController")


def verify():
    """保存後に読み直し、実行時に必要な参照がすべて入っていることを確認する。"""
    vehicle_cdo = unreal.get_default_object(load_required(VEHICLE_BP_PATH).generated_class())
    for action_name, _, property_candidates in DEBUG_ACTIONS:
        if get_property(vehicle_cdo, property_candidates) is None:
            fail("BP_ImpactVehicle has no {0} after save".format(action_name))

    controller_cdo = unreal.get_default_object(load_required(CONTROLLER_BP_PATH).generated_class())
    if get_property(controller_cdo, ("debug_mapping_context",)) is None:
        fail("BP_ImpactPlayerController has no debug mapping context after save")

    # 本番の操作系を壊していないことも確認する。
    if get_property(controller_cdo, ("vehicle_mapping_context",)) is None:
        fail("BP_ImpactPlayerController lost its vehicle mapping context")

    log("verified: debug input actions and mapping context are assigned")


def main():
    log("=== debug input setup start ===")
    actions, context = create_debug_input()
    assign_to_blueprints(actions, context)
    verify()
    log("=== debug input setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
