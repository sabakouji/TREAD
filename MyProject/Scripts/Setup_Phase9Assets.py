# 踏みつけ加速メカゲーム — Phase 9 ゲームフロー用アセット生成スクリプト
#
# 生成・更新するアセット:
#   /Game/Maps/Title                                  （タイトル画面用のマップ。画面が全体を覆うため中身は空）
#   /Game/Tuning/DA_MatchTuning                       （カウントダウン・制限時間・遷移先のマップ）
#   /Game/UI/WBP_HUD / WBP_Result / WBP_Title         （画面。配置は C++ で組み立て、ここでは色・寸法の調整用に派生を作る）
#   /Game/Input/IA_DebugToggleHUD                     （F1: デバッグ HUD の表示切り替え。IMC_Debug に追加）
#   /Game/Blueprints/BP_ImpactTitlePlayerController   （WBP_Title を割り当て）
#   /Game/Blueprints/BP_ImpactTitleGameMode           （上記コントローラと DA_MatchTuning を割り当て）
#   /Game/Blueprints/BP_ImpactPlayerController        （WBP_HUD / WBP_Result / IA_DebugToggleHUD を割り当て）
#   /Game/Blueprints/BP_ImpactGameMode                （DA_MatchTuning を割り当て）
#
# タイトル用ゲームモードは World Settings ではなく DefaultEngine.ini の GameModeMapPrefixes で割り当てる。
#
# CLAUDE.md 5-1・5-2 の教訓に従い、入力マッピングは DefaultKeyMappings に書き込み、保存後に読み直して検証する。
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
UI_PATH = "/Game/UI"
INPUT_PATH = "/Game/Input"
MAP_DIR = "/Game/Maps"
TITLE_MAP_PATH = "/Game/Maps/Title"
SOLO_MAP_PATH = "/Game/Maps/SoloArena"

GAMEMODE_BP_PATH = "/Game/Blueprints/BP_ImpactGameMode"
CONTROLLER_BP_PATH = "/Game/Blueprints/BP_ImpactPlayerController"
DEBUG_CONTEXT_PATH = "/Game/Input/IMC_Debug"

TOGGLE_HUD_ACTION = "IA_DebugToggleHUD"
TOGGLE_HUD_KEY = "F1"

# (アセット名, 親クラス)
WIDGETS = [
    ("WBP_HUD", unreal.ImpactHUDWidget),
    ("WBP_Result", unreal.ImpactResultWidget),
    ("WBP_Title", unreal.ImpactTitleWidget),
]


def log(message):
    unreal.log("[Phase9Setup] {0}".format(message))


def warn(message):
    unreal.log_warning("[Phase9Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[Phase9Setup] FAILED: {0}".format(message))
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


def set_property(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as error:
        fail("could not set {0} on {1}: {2}".format(name, obj.get_name(), error))


def ensure_directory(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)
        log("created directory: {0}".format(path))


def object_path(value):
    """ソフト参照・オブジェクト参照のどちらでも、参照先のパスを文字列で返す。"""
    if value is None:
        return ""
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)


def create_title_map():
    """タイトル画面用の空のマップを作る。画面は不透明な背景で全体を覆うため、何も配置しない。"""
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ensure_directory(MAP_DIR)

    if EAL.does_asset_exist(TITLE_MAP_PATH):
        log("already exists: {0}".format(TITLE_MAP_PATH))
        return

    if not level_subsystem.new_level(TITLE_MAP_PATH):
        fail("could not create level: {0}".format(TITLE_MAP_PATH))
    if not level_subsystem.save_current_level():
        fail("could not save level: {0}".format(TITLE_MAP_PATH))
    log("created level: {0}".format(TITLE_MAP_PATH))


def create_match_tuning():
    full_path = "{0}/DA_MatchTuning".format(TUNING_PATH)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        asset = EAL.load_asset(full_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.MatchTuningDataAsset)
        asset = ASSET_TOOLS.create_asset("DA_MatchTuning", TUNING_PATH, unreal.MatchTuningDataAsset, factory)
        if asset is None:
            fail("could not create {0}".format(full_path))
        log("created: {0}".format(full_path))

    set_property(asset, "title_map", load_required(TITLE_MAP_PATH))
    set_property(asset, "solo_map", load_required(SOLO_MAP_PATH))
    if not EAL.save_loaded_asset(asset):
        fail("could not save {0}".format(full_path))

    for name, expected in (("title_map", TITLE_MAP_PATH), ("solo_map", SOLO_MAP_PATH)):
        actual = object_path(asset.get_editor_property(name))
        if not actual.startswith(expected):
            fail("DA_MatchTuning.{0} is '{1}', expected {2}".format(name, actual, expected))
        log("DA_MatchTuning.{0} = {1}".format(name, actual))

    log("DA_MatchTuning: countdown {0} s, duration {1} s".format(
        asset.get_editor_property("countdown_seconds"),
        asset.get_editor_property("match_duration_seconds")))
    return asset


def create_widget_blueprint(name, parent_class):
    full_path = "{0}/{1}".format(UI_PATH, name)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        blueprint = EAL.load_asset(full_path)
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = ASSET_TOOLS.create_asset(name, UI_PATH, unreal.WidgetBlueprint, factory)
        if blueprint is None:
            fail("could not create {0}".format(full_path))
        log("created: {0}".format(full_path))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not EAL.save_loaded_asset(blueprint):
        fail("could not save {0}".format(full_path))

    widget_class = blueprint.generated_class()
    if not isinstance(unreal.get_default_object(widget_class), parent_class):
        fail("{0} does not derive from {1}".format(name, parent_class.__name__))
    return widget_class


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


def configure_blueprint(blueprint, properties):
    """CDO にプロパティを設定し、コンパイルして保存する。"""
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    for name, value in properties:
        set_property(cdo, name, value)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not EAL.save_loaded_asset(blueprint):
        fail("could not save {0}".format(blueprint.get_name()))
    log("{0}: set {1}".format(blueprint.get_name(), ", ".join(name for name, _ in properties)))


def create_toggle_hud_input():
    full_path = "{0}/{1}".format(INPUT_PATH, TOGGLE_HUD_ACTION)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        action = EAL.load_asset(full_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.InputAction)
        action = ASSET_TOOLS.create_asset(TOGGLE_HUD_ACTION, INPUT_PATH, unreal.InputAction, factory)
        if action is None:
            fail("could not create {0}".format(full_path))
        log("created: {0}".format(full_path))

    action.set_editor_property("value_type", enum_value(unreal.InputActionValueType, "BOOLEAN", "BOOL"))
    if not EAL.save_loaded_asset(action):
        fail("could not save {0}".format(TOGGLE_HUD_ACTION))

    # 既存のデバッグ操作（Q / R）を残したまま、この操作のマッピングだけを差し替える。
    context = load_required(DEBUG_CONTEXT_PATH)
    mapping_data = context.get_editor_property("default_key_mappings")
    existing = list(mapping_data.get_editor_property("mappings"))
    kept = [m for m in existing if object_path(m.get_editor_property("action")) != action.get_path_name()]

    key = unreal.Key()
    key.set_editor_property("key_name", TOGGLE_HUD_KEY)
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", key)

    # 実行時が読むのは DefaultKeyMappings のみ（CLAUDE.md 5-1）。
    mapping_data.set_editor_property("mappings", kept + [mapping])
    context.set_editor_property("default_key_mappings", mapping_data)
    if not EAL.save_loaded_asset(context):
        fail("could not save IMC_Debug")

    saved = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    keys = [str(m.get_editor_property("key").get_editor_property("key_name")) for m in saved]
    if len(saved) != len(kept) + 1 or TOGGLE_HUD_KEY not in keys:
        fail("IMC_Debug.DefaultKeyMappings is {0}; expected the previous {1} keys plus {2}".format(
            keys, len(kept), TOGGLE_HUD_KEY))
    log("IMC_Debug: {0} mappings in DefaultKeyMappings {1}".format(len(saved), keys))
    return action


def verify(match_tuning, widget_classes, toggle_action):
    """保存後に読み直し、実行時に必要な参照がすべて入っていることを確認する。"""
    def cdo_of(path):
        return unreal.get_default_object(load_required(path).generated_class())

    expectations = [
        (GAMEMODE_BP_PATH, "match_tuning", match_tuning),
        (GAMEMODE_BP_PATH, "score_tuning", None),
        (CONTROLLER_BP_PATH, "hud_widget_class", widget_classes["WBP_HUD"]),
        (CONTROLLER_BP_PATH, "result_widget_class", widget_classes["WBP_Result"]),
        (CONTROLLER_BP_PATH, "toggle_debug_hud_action", toggle_action),
        (CONTROLLER_BP_PATH, "vehicle_mapping_context", None),
        (CONTROLLER_BP_PATH, "debug_mapping_context", None),
        ("{0}/BP_ImpactTitlePlayerController".format(BP_PATH), "title_widget_class", widget_classes["WBP_Title"]),
        ("{0}/BP_ImpactTitleGameMode".format(BP_PATH), "match_tuning", match_tuning),
    ]
    for path, name, expected in expectations:
        actual = cdo_of(path).get_editor_property(name)
        if actual is None:
            fail("{0}.{1} is unset after save".format(path, name))
        # 期待値が None の項目は、既存の割り当てが壊れていないことだけを確かめる。
        if expected is not None and object_path(actual) != object_path(expected):
            fail("{0}.{1} is {2}, expected {3}".format(path, name, object_path(actual), object_path(expected)))

    title_gamemode = cdo_of("{0}/BP_ImpactTitleGameMode".format(BP_PATH))
    title_controller_class = load_required("{0}/BP_ImpactTitlePlayerController".format(BP_PATH)).generated_class()
    if object_path(title_gamemode.get_editor_property("player_controller_class")) != object_path(title_controller_class):
        fail("BP_ImpactTitleGameMode.player_controller_class is not BP_ImpactTitlePlayerController")

    if not EAL.does_asset_exist(TITLE_MAP_PATH):
        fail("{0} does not exist after save".format(TITLE_MAP_PATH))

    log("verified: game flow assets are created and assigned ({0} checks)".format(len(expectations) + 2))


def main():
    log("=== Phase 9 game flow setup start ===")
    create_title_map()
    match_tuning = create_match_tuning()

    ensure_directory(UI_PATH)
    widget_classes = {name: create_widget_blueprint(name, parent) for name, parent in WIDGETS}

    toggle_action = create_toggle_hud_input()

    title_controller = create_blueprint("BP_ImpactTitlePlayerController", unreal.ImpactTitlePlayerController)
    configure_blueprint(title_controller, [("title_widget_class", widget_classes["WBP_Title"])])

    title_gamemode = create_blueprint("BP_ImpactTitleGameMode", unreal.ImpactTitleGameMode)
    configure_blueprint(title_gamemode, [
        ("player_controller_class", title_controller.generated_class()),
        ("match_tuning", match_tuning),
    ])

    configure_blueprint(load_required(CONTROLLER_BP_PATH), [
        ("hud_widget_class", widget_classes["WBP_HUD"]),
        ("result_widget_class", widget_classes["WBP_Result"]),
        ("toggle_debug_hud_action", toggle_action),
    ])
    configure_blueprint(load_required(GAMEMODE_BP_PATH), [("match_tuning", match_tuning)])

    verify(match_tuning, widget_classes, toggle_action)
    log("=== Phase 9 game flow setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
