# 踏みつけ加速メカゲーム — 制限時間の変更スクリプト
#
# DA_MatchTuning の制限時間（MatchDurationSeconds）を引数の秒数に書き換え、保存後に読み直して検証する。
# パッケージ版の終了条件（タイムアップ）を短時間で確認する際に用いる。確認後は既定の 180 に戻すこと。
#
# エディタ上では DA_MatchTuning を開き、詳細パネルの Match > Match Duration Seconds から同じ値を変更できる。
# 本スクリプトはエディタを開かずに変更したい場合（パッケージ作成の直前など）のためのもの。
#
# 実行方法:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル> <秒数>" -unattended -nosplash -nop4

import sys

import unreal

MATCH_TUNING_PATH = "/Game/Tuning/DA_MatchTuning"
PROPERTY_NAME = "match_duration_seconds"

# C++ 側の ClampMin と同じ下限。これ未満はエディタでも設定できない。
MIN_DURATION = 1.0


def log(message):
    unreal.log("[MatchDuration] {0}".format(message))


def fail(message):
    unreal.log_error("[MatchDuration] FAILED: {0}".format(message))
    raise RuntimeError(message)


def parse_duration():
    if len(sys.argv) < 2:
        fail("usage: Set_MatchDuration.py <seconds>; got arguments {0}".format(sys.argv[1:]))
    try:
        duration = float(sys.argv[1])
    except ValueError:
        fail("'{0}' is not a number".format(sys.argv[1]))
    if duration < MIN_DURATION:
        fail("{0} is below the minimum of {1} seconds".format(duration, MIN_DURATION))
    return duration


def main():
    duration = parse_duration()

    asset = unreal.EditorAssetLibrary.load_asset(MATCH_TUNING_PATH)
    if asset is None:
        fail("could not load {0}".format(MATCH_TUNING_PATH))

    previous = asset.get_editor_property(PROPERTY_NAME)
    asset.set_editor_property(PROPERTY_NAME, duration)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        fail("could not save {0}".format(MATCH_TUNING_PATH))

    saved = asset.get_editor_property(PROPERTY_NAME)
    if abs(saved - duration) > 0.001:
        fail("expected {0}, got {1} after save".format(duration, saved))
    log("DA_MatchTuning.MatchDurationSeconds: {0} -> {1}".format(previous, saved))


main()
unreal.SystemLibrary.quit_editor()
