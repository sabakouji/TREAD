# 踏みつけ加速メカゲーム — 入力アセット割り当てスクリプト（Phase 3 / Phase 4）
#
# BP_ImpactVehicle の MoveAction / LookAction / BrakeAction に
# IA_Move / IA_Look / IA_Brake を割り当てる。
# 何度実行しても同じ結果になる（冪等）。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

EAL = unreal.EditorAssetLibrary

VEHICLE_BP_PATH = "/Game/Blueprints/BP_ImpactVehicle"

# Pawn のプロパティ名 → 割り当てる InputAction アセット
INPUT_BINDINGS = {
    "move_action": "/Game/Input/IA_Move",
    "look_action": "/Game/Input/IA_Look",
    "brake_action": "/Game/Input/IA_Brake",
}


def log(message):
    unreal.log("[InputSetup] {0}".format(message))


def fail(message):
    unreal.log_error("[InputSetup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def main():
    log("=== input binding start ===")

    vehicle_bp = load_required(VEHICLE_BP_PATH)

    unreal.BlueprintEditorLibrary.compile_blueprint(vehicle_bp)
    cdo = unreal.get_default_object(vehicle_bp.generated_class())

    for property_name, action_path in INPUT_BINDINGS.items():
        cdo.set_editor_property(property_name, load_required(action_path))
        log("assigned {0} -> {1}".format(action_path, property_name))

    unreal.BlueprintEditorLibrary.compile_blueprint(vehicle_bp)
    if not EAL.save_loaded_asset(vehicle_bp):
        fail("could not save {0}".format(VEHICLE_BP_PATH))
    log("saved: {0}".format(VEHICLE_BP_PATH))

    # 割り当てが実際に保存されたかを読み直して確認する。
    reloaded = unreal.get_default_object(load_required(VEHICLE_BP_PATH).generated_class())
    for property_name in INPUT_BINDINGS:
        if reloaded.get_editor_property(property_name) is None:
            fail("{0} is still unset after save".format(property_name))
    log("verified: all {0} input bindings are set".format(len(INPUT_BINDINGS)))

    log("=== input binding complete ===")


main()
unreal.SystemLibrary.quit_editor()
