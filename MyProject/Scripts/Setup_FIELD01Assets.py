# 踏みつけ加速メカゲーム — FIELD-01（フィールド用コンポーネント）のアセット設定スクリプト
#
# FIELD-01 で、ゴール・破壊可能オブジェクト・踏み台の供給拠点の設定値を Actor から部品（ActorComponent）へ移した。
# 移した設定値のうち、Blueprint 側で既定値から変えていたものを、移動先の部品に設定し直す。
#
# 更新するアセット:
#   /Game/Blueprints/BP_DestructibleSmall   （Destructible.Rank = Small）
#   /Game/Blueprints/BP_DestructibleLarge   （Destructible.Rank = Large）
#   /Game/Blueprints/BP_StompTargetSpawner  （TankSpawner.StompTargetClass = BP_StompTarget）
#
# 併せて、以下を検証する（保存は行わない）:
#   - BP_Goal / BP_Destructible* の部品が IImpactReceiver を実装していること
#   - 陣営の既定値（プレイヤー 0 / NPC・ゴール 1）
#
# 何度実行しても同じ結果になる（冪等）。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import unreal

EAL = unreal.EditorAssetLibrary

BP_PATH = "/Game/Blueprints"

# 陣営の番号（C++ の ImpactTeam と一致させる）。
TEAM_PLAYER = 0
TEAM_OPPONENT = 1


def log(message):
    unreal.log("[FIELD01Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[FIELD01Setup] FAILED: {0}".format(message))
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


def save(blueprint, name):
    if not EAL.save_loaded_asset(blueprint, only_if_is_dirty=False):
        fail("could not save {0}".format(name))
    log("saved: {0}".format(name))


def setup_destructible(name, rank):
    blueprint = load_blueprint(name)
    cdo = compiled_cdo(blueprint)

    destructible = cdo.get_editor_property("destructible")
    if destructible is None:
        fail("{0} has no Destructible component".format(name))

    destructible.set_editor_property("rank", rank)
    save(blueprint, name)

    if cdo.get_editor_property("destructible").get_editor_property("rank") != rank:
        fail("{0} rank is not {1} after save".format(name, rank))
    log("{0}: rank = {1}".format(name, rank))


def setup_spawner():
    target_class = load_blueprint("BP_StompTarget").generated_class()
    blueprint = load_blueprint("BP_StompTargetSpawner")
    cdo = compiled_cdo(blueprint)

    spawner = cdo.get_editor_property("tank_spawner")
    if spawner is None:
        fail("BP_StompTargetSpawner has no TankSpawner component")

    spawner.set_editor_property("stomp_target_class", target_class)
    save(blueprint, "BP_StompTargetSpawner")

    if cdo.get_editor_property("tank_spawner").get_editor_property("stomp_target_class") != target_class:
        fail("BP_StompTargetSpawner stomp_target_class is unset after save")
    log("BP_StompTargetSpawner: stomp_target_class = BP_StompTarget")


def verify_receiver(name, component_property):
    cdo = compiled_cdo(load_blueprint(name))
    component = cdo.get_editor_property(component_property)
    if component is None:
        fail("{0} has no {1} component".format(name, component_property))
    # UE の Python ではインターフェースは基底クラスにならないため、isinstance では判定できない。
    if not unreal.SystemLibrary.does_implement_interface(component, unreal.ImpactReceiver):
        fail("{0}.{1} does not implement IImpactReceiver".format(name, component_property))
    log("verified: {0}.{1} implements IImpactReceiver".format(name, component_property))


def verify_team(name, expected):
    cdo = compiled_cdo(load_blueprint(name))
    team = cdo.get_editor_property("team")
    if team is None:
        fail("{0} has no Team component".format(name))
    actual = team.get_team_id()
    if actual != expected:
        fail("{0} team is {1}, expected {2}".format(name, actual, expected))
    log("verified: {0} team = {1}".format(name, actual))


def main():
    setup_destructible("BP_DestructibleSmall", unreal.DestructionRank.SMALL)
    setup_destructible("BP_DestructibleLarge", unreal.DestructionRank.LARGE)
    setup_spawner()

    verify_receiver("BP_Goal", "goal")
    verify_receiver("BP_DestructibleSmall", "destructible")
    verify_receiver("BP_DestructibleLarge", "destructible")

    verify_team("BP_ImpactVehicle", TEAM_PLAYER)
    verify_team("BP_EnemyVehicle", TEAM_OPPONENT)
    verify_team("BP_Goal", TEAM_OPPONENT)

    log("done")


main()
