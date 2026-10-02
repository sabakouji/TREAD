# 踏みつけ加速メカゲーム — 入力マッピングの実体検査スクリプト
#
# IMC_Vehicle をディスクから読み込み、実行時にマッピングが構築されない原因を切り分ける。
# アセットの変更は一切行わない（読み取り専用）。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

EAL = unreal.EditorAssetLibrary

CONTEXT_PATH = "/Game/Input/IMC_Vehicle"


def log(message):
    unreal.log("[InspectInput] {0}".format(message))


def describe(value):
    if value is None:
        return "None"
    try:
        return value.get_name()
    except Exception:
        return str(value)


def inspect_mappings(context):
    """
    実行時が読む DefaultKeyMappings と、非推奨の Mappings の両方を検査する。

    UE 5.7 で Mappings は非推奨となり、RebuildControlMappings は
    DefaultKeyMappings.Mappings しか読まない。非推奨側にだけ入っていると
    保存も検査も成功して見えるのに実行時は無反応になる。
    """
    deprecated = context.get_editor_property("mappings")
    log("deprecated Mappings count = {0} (runtime ignores this)".format(len(deprecated)))

    mapping_data = context.get_editor_property("default_key_mappings")
    mappings = mapping_data.get_editor_property("mappings")
    log("DefaultKeyMappings count = {0} (runtime reads this)".format(len(mappings)))

    for index, mapping in enumerate(mappings):
        action = mapping.get_editor_property("action")
        key = mapping.get_editor_property("key")
        modifiers = mapping.get_editor_property("modifiers")
        triggers = mapping.get_editor_property("triggers")

        key_name = "None"
        if key is not None:
            key_name = str(key.get_editor_property("key_name"))

        log("  [{0}] action={1} key={2} modifiers={3} triggers={4}".format(
            index, describe(action), key_name, len(modifiers), len(triggers)))

        for modifier in modifiers:
            log("        modifier: {0}".format(type(modifier).__name__))


def inspect_input_mode(context):
    # Input Mode フィルタの設定。既定は UseProjectDefaultQuery。
    for property_name in ("input_mode_filter_options", "input_mode_query_override"):
        try:
            value = context.get_editor_property(property_name)
            log("{0} = {1}".format(property_name, value))
        except Exception as error:
            log("{0} is not readable: {1}".format(property_name, error))


def inspect_developer_settings():
    """
    Enhanced Input の開発者設定を確認する。

    UEnhancedInputDeveloperSettings は Python に公開されていないため、
    コンソール変数として登録されている設定値を読む。
    """
    variable_name = "EnhancedInput.EnableInputModeFiltering"
    try:
        value = unreal.SystemLibrary.get_console_variable_int_value(variable_name)
        log("cvar {0} = {1}".format(variable_name, value))
    except Exception as error:
        log("cvar {0} is not readable: {1}".format(variable_name, error))


def main():
    log("=== input mapping inspection start ===")

    context = EAL.load_asset(CONTEXT_PATH)
    if context is None:
        unreal.log_error("[InspectInput] could not load {0}".format(CONTEXT_PATH))
        return

    log("loaded: {0} ({1})".format(CONTEXT_PATH, type(context).__name__))

    inspect_mappings(context)
    inspect_input_mode(context)
    inspect_developer_settings()

    log("=== input mapping inspection complete ===")


main()
unreal.SystemLibrary.quit_editor()
