# 踏みつけ加速メカゲーム — FIX-01 の変更後にアセットが正しく読み込まれるかを検証するスクリプト
#
# 注意（FIELD-01 以降）: ゴール・破壊可能オブジェクトの IImpactReceiver は Actor ではなく部品
# （UGoalComponent / UDestructibleComponent）が実装するようになったため、検証 3 の Actor に対する判定は失敗する。
# FIX-01 時点の検証記録として残している。現在の構成の検証には Setup_FIELD01Assets.py を使うこと。
#
# 読み取り専用。アセットは一切保存しない。何度実行しても結果は同じ。
#
# 検証内容:
#   1. DA_VehicleTuning の新しいカメラ調整値（CameraLagSpeed / CameraBasePitch）が C++ の既定値
#      （8.0 / -15.0）で読み込まれること。既存アセットには保存値がないため既定値になるはずで、
#      修正前のコンストラクタの直書きと同じ値であることを確かめる
#   2. 親クラスが変わった既存の Blueprint が、コンパイルでエラーにならないこと
#   3. ゴール・破壊可能オブジェクトが IImpactReceiver を、踏み台が IStompable を実装していること
#      （Blueprint 派生の CDO で確かめる）
#
# 結果は "FIX01 verify:" の接頭辞でログに出す。失敗は unreal.log_error で残す。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

EAL = unreal.EditorAssetLibrary

VEHICLE_TUNING_PATH = "/Game/Tuning/DA_VehicleTuning"

EXPECTED_CAMERA = {
    "camera_lag_speed": 8.0,
    "camera_base_pitch": -15.0,
}

# 修正前から変わっていないことを記録するためのカメラ関連の値（比較は行わず記録のみ）。
RECORDED_CAMERA = [
    "camera_arm_length_at_rest",
    "camera_arm_length_at_top_speed",
    "camera_fov_at_rest",
    "camera_fov_at_top_speed",
    "look_sensitivity",
    "camera_pitch_min",
    "camera_pitch_max",
]

BLUEPRINTS = [
    "/Game/Blueprints/BP_ImpactVehicle",
    "/Game/Blueprints/BP_EnemyVehicle",
    "/Game/Blueprints/BP_Goal",
    "/Game/Blueprints/BP_DestructibleSmall",
    "/Game/Blueprints/BP_DestructibleLarge",
    "/Game/Blueprints/BP_StompTarget",
    "/Game/Blueprints/BP_StompTargetSpawner",
    "/Game/Blueprints/BP_ImpactGameMode",
    "/Game/Blueprints/BP_ImpactPlayerController",
    "/Game/UI/WBP_HUD",
    "/Game/UI/WBP_Result",
]

# (Blueprint のパス, 実装しているべきインターフェース)
INTERFACES = [
    ("/Game/Blueprints/BP_Goal", unreal.ImpactReceiver),
    ("/Game/Blueprints/BP_DestructibleSmall", unreal.ImpactReceiver),
    ("/Game/Blueprints/BP_DestructibleLarge", unreal.ImpactReceiver),
    ("/Game/Blueprints/BP_StompTarget", unreal.Stompable),
]

FLOAT_TOLERANCE = 0.001


class Verifier:
    def __init__(self):
        self.failures = 0
        self.checks = 0

    def ok(self, message):
        self.checks += 1
        unreal.log("FIX01 verify: OK   {0}".format(message))

    def ng(self, message):
        self.checks += 1
        self.failures += 1
        unreal.log_error("FIX01 verify: FAIL {0}".format(message))

    def info(self, message):
        unreal.log("FIX01 verify: INFO {0}".format(message))


def verify_camera_tuning(verifier):
    tuning = EAL.load_asset(VEHICLE_TUNING_PATH)
    if tuning is None:
        verifier.ng("could not load {0}".format(VEHICLE_TUNING_PATH))
        return

    for name, expected in EXPECTED_CAMERA.items():
        actual = tuning.get_editor_property(name)
        if abs(actual - expected) <= FLOAT_TOLERANCE:
            verifier.ok("DA_VehicleTuning.{0} = {1}".format(name, actual))
        else:
            verifier.ng("DA_VehicleTuning.{0} = {1}, expected {2}".format(name, actual, expected))

    for name in RECORDED_CAMERA:
        verifier.info("DA_VehicleTuning.{0} = {1}".format(name, tuning.get_editor_property(name)))


def blueprint_status(blueprint):
    """コンパイル状態を返す。取得できない環境では None を返し、生成クラスの有無で判定する。"""
    try:
        return blueprint.get_editor_property("status")
    except Exception:
        return None


def verify_blueprints(verifier):
    error_status = getattr(unreal.BlueprintStatus, "BS_ERROR", None) if hasattr(unreal, "BlueprintStatus") else None

    for path in BLUEPRINTS:
        blueprint = EAL.load_asset(path)
        if blueprint is None:
            verifier.ng("could not load {0}".format(path))
            continue

        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

        generated = blueprint.generated_class()
        status = blueprint_status(blueprint)
        if generated is None:
            verifier.ng("{0}: no generated class after compile".format(path))
        elif status is not None and error_status is not None and status == error_status:
            verifier.ng("{0}: compiled with errors (status {1})".format(path, status))
        else:
            verifier.ok("{0}: compiled (status {1}, class {2})".format(path, status, generated.get_name()))


def verify_interfaces(verifier):
    for path, interface in INTERFACES:
        blueprint = EAL.load_asset(path)
        if blueprint is None:
            verifier.ng("could not load {0}".format(path))
            continue

        cdo = unreal.get_default_object(blueprint.generated_class())
        if unreal.SystemLibrary.does_implement_interface(cdo, interface):
            verifier.ok("{0} implements {1}".format(path, interface.__name__))
        else:
            verifier.ng("{0} does not implement {1}".format(path, interface.__name__))


def main():
    verifier = Verifier()
    unreal.log("FIX01 verify: === start (read-only, nothing is saved) ===")
    verify_camera_tuning(verifier)
    verify_blueprints(verifier)
    verify_interfaces(verifier)

    summary = "=== done: {0} checks, {1} failed ===".format(verifier.checks, verifier.failures)
    if verifier.failures:
        unreal.log_error("FIX01 verify: " + summary)
    else:
        unreal.log("FIX01 verify: " + summary)


main()
unreal.SystemLibrary.quit_editor()
