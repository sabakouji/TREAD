# 踏みつけ加速メカゲーム — FX-01（破壊演出の作り込み）のアセット設定スクリプト
#
# 生成・更新するアセット:
#   /Game/Tuning/DA_FeedbackTuning           （破壊の手応え・破片の調整値。既にあれば数値は上書きしない）
#   /Game/Blueprints/BP_ImpactPlayerController （Feedback.FeedbackTuning = DA_FeedbackTuning）
#   /Game/Blueprints/BP_DestructibleSmall      （Destructible.FeedbackTuning = DA_FeedbackTuning）
#   /Game/Blueprints/BP_DestructibleLarge      （同上。DestructionGC が既にあれば、その割り当ては変えない）
#
# Geometry Collection（DestructionGC）の資産は Python から作れないため、ユーザーがエディタの Fracture モードで作る
# （手順: Plan/FX-01_GeometryCollection作成手順.md）。作成後にこのスクリプトを再実行すると、
# GC_DestructibleLarge があれば BP_DestructibleLarge の DestructionGC に割り当てる。
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
TUNING_NAME = "DA_FeedbackTuning"

# ユーザーが Fracture モードで作る Geometry Collection の置き場所と名前（手順書と一致させる）。
GC_PATH = "/Game/Destruction/GC_DestructibleLarge"


def log(message):
    unreal.log("[FX01Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[FX01Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_blueprint(name):
    path = "{0}/{1}".format(BP_PATH, name)
    blueprint = EAL.load_asset(path)
    if blueprint is None:
        fail("could not load required asset: {0}".format(path))
    return blueprint


def compiled_cdo(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return unreal.get_default_object(blueprint.generated_class())


def save(asset, name):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        fail("could not save {0}".format(name))
    log("saved: {0}".format(name))


def ensure_tuning():
    full_path = "{0}/{1}".format(TUNING_PATH, TUNING_NAME)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0} (values are kept)".format(full_path))
        return EAL.load_asset(full_path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.FeedbackTuningDataAsset)
    asset = ASSET_TOOLS.create_asset(TUNING_NAME, TUNING_PATH, unreal.FeedbackTuningDataAsset, factory)
    if asset is None:
        fail("could not create {0}".format(full_path))
    save(asset, TUNING_NAME)
    return asset


def assign_to_player_controller(tuning):
    blueprint = load_blueprint("BP_ImpactPlayerController")
    cdo = compiled_cdo(blueprint)
    feedback = cdo.get_editor_property("feedback")
    if feedback is None:
        fail("BP_ImpactPlayerController has no Feedback component")
    feedback.set_editor_property("feedback_tuning", tuning)
    save(blueprint, "BP_ImpactPlayerController")


def assign_to_destructible(name, tuning, geometry_collection):
    blueprint = load_blueprint(name)
    cdo = compiled_cdo(blueprint)
    destructible = cdo.get_editor_property("destructible")
    if destructible is None:
        fail("{0} has no Destructible component".format(name))

    destructible.set_editor_property("feedback_tuning", tuning)
    if geometry_collection is not None and destructible.get_editor_property("destruction_gc") is None:
        destructible.set_editor_property("destruction_gc", geometry_collection)
        log("{0}: destruction_gc = {1}".format(name, GC_PATH))
    save(blueprint, name)


def main():
    tuning = ensure_tuning()
    assign_to_player_controller(tuning)

    geometry_collection = EAL.load_asset(GC_PATH) if EAL.does_asset_exist(GC_PATH) else None
    if geometry_collection is None:
        log("{0} is not created yet; BP_DestructibleLarge breaks without Chaos debris".format(GC_PATH))

    assign_to_destructible("BP_DestructibleSmall", tuning, None)
    assign_to_destructible("BP_DestructibleLarge", tuning, geometry_collection)

    # 保存後の値を検証する。
    pc_cdo = compiled_cdo(load_blueprint("BP_ImpactPlayerController"))
    if pc_cdo.get_editor_property("feedback").get_editor_property("feedback_tuning") != tuning:
        fail("BP_ImpactPlayerController feedback_tuning is unset after save")
    for name in ("BP_DestructibleSmall", "BP_DestructibleLarge"):
        cdo = compiled_cdo(load_blueprint(name))
        if cdo.get_editor_property("destructible").get_editor_property("feedback_tuning") != tuning:
            fail("{0} feedback_tuning is unset after save".format(name))
    log("verified: DA_FeedbackTuning is assigned")
    log("done")


main()
