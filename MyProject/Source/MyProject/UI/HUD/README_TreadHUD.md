# 汎用 HUD 部品（TREAD HUD）の使い方

HUD の要素ごとにクラスを作らず、**値の行先（GameplayTag）と画像を詳細パネルで選び、配置は WBP のデザイナーで行う**仕組み。
仕様は `Plan/GUI_SPEC.md`、実装計画は `Plan/GUI-01_汎用HUD部品システムの実装計画.md`。

## 構成

```
[機体 Pawn] ── UTreadHUDDataComponent（HUDData）
     ↑ SetValue(タグ, 値, 最小, 最大)        ← 移動・ロックオンの各コンポーネントが毎フレーム書き込む
     │
WBP_BattleHUD（配置だけ。BP_ImpactPlayerController が表示する）
 ├─ WBP_HUDMeter       （UTreadHUDMeterWidget       … 弧のメーター）
 ├─ WBP_HUDText        （UTreadHUDTextWidget        … 数値の文字）
 └─ WBP_HUDImageSwitch （UTreadHUDImageSwitchWidget … 値による画像の切り替え）
```

| 部品 | 役割 |
|------|------|
| `UTreadHUDDataComponent` | 値の置き場。`SetValue` は値が変わったときだけ `OnValueChanged` を発火する |
| `UTreadHUDElementWidget` | 3種の部品の共通基底。`BindTag` / `UpdateMode`（Tick / Event）/ `InterpSpeed` / `PreviewFill` |
| `UTreadHUDMeterWidget` | 画像 4 層（未到達・到達・演出・外枠）と弧マスク `M_HUD_ArcFill` でメーターを描く |
| `UTreadHUDTextWidget` | `Format`（例 `{0} km/h`）・`Multiplier`・`FractionalDigits` で数値を出す |
| `UTreadHUDImageSwitchWidget` | `States`（`Threshold` / `Texture` / `Tint`）から、元の値が閾値以上で最大のものを出す |

### 公開している値

| タグ | 値（Raw） | 範囲 | 書き込み元 |
|------|----------|------|-----------|
| `HUD.Speed` | 速さ（uu/s） | 0〜`DA_VehicleTuning.MaxSpeed` | `UImpactVehicleMovementComponent::PublishHUDValues` |
| `HUD.Stun` | 行動不能の残り時間（秒） | 0〜自滅・拮抗の行動不能時間の長いほう | 同上 |
| `HUD.LockOn` | 0 = 未捕捉 / 1 = 捕捉中 / 2 = アシスト作動中（`TreadHUDLockOn`） | 0〜2 | `UImpactLockOnComponent::PublishHUDValue` |
| `HUD.Boost` | （書き込み元なし。ゲームにブーストの値が無いため） | — | — |

---

## WBP_BattleHUD に速度メーターを置く（初回のみ）

1. コンテンツブラウザで `/Game/UI/WBP_BattleHUD` を開く（デザイナー）
2. パレットから **Canvas Panel** をルートに置く
3. パレットの「ユーザー作成」から **WBP_HUDMeter** をキャンバスへドラッグする
   - アンカー: 右下 / 「Size To Content」をオン / 位置を画面の右下の余白に合わせる
   - 詳細パネル `TREAD|HUD`:
     - `Bind Tag` = `HUD.Speed`
   - 詳細パネル `TREAD|HUD|Fill`:
     - `Use Curve` をオン
     - `Response Curve` の外部カーブ（External Curve）に `/Game/UI/Curves/CF_HUD_SpeedResponse` を選ぶ
       （破壊閾値 Small / Large と超加速を、赤・黄・緑・青の区切りに合わせるカーブ）
4. （任意）**WBP_HUDText** をメーターの円の中に置く
   - `Bind Tag` = `HUD.Speed` / `Format` = `{0} km/h` / `Multiplier` = `0.036` / `Interp Speed` = `12`
5. （任意）**WBP_HUDImageSwitch** を置き、`Bind Tag` = `HUD.LockOn` または `HUD.Stun` とし、`States` に画像を登録する
6. コンパイルして保存する

PIE を開始すると、出力ログに次の行が出る。部品の数と紐付け先が置いたとおりか確かめる。

```
LogImpactUI: battle HUD shown: WBP_BattleHUD_C with 2 HUD elements [WBP_HUDMeter_C(HUD.Speed), WBP_HUDText_C(HUD.Speed)]
```

`WBP_HUD` の横棒の速度ゲージは `bShowSpeedGauge = false` で隠している（残り時間・得点・ロックオン表示は従来どおり）。
横棒に戻す場合は `WBP_HUD` のクラスの既定値で `Show Speed Gauge` をオンにする。

### 超加速に入ったときの演出（OnThresholdCrossed）

メーターは、表示値がセグメントの区切りを跨ぐたびに `OnThresholdCrossed(SegmentIndex, bRising)` を呼ぶ。
速度メーターでは `SegmentIndex = 3` が青（超加速域）。

- **全メーター共通の演出**にする場合: `WBP_HUDMeter` のイベントグラフで `Event On Threshold Crossed` を実装する
- **配置した 1 個だけの演出**にする場合: `WBP_BattleHUD` でメーターの「変数にする（Is Variable）」をオンにし、
  イベントグラフでそのメーターの `On Threshold Crossed Event` を購読する（例: `Print String` で確認）

最初に値を受け取った時点では呼ばれない（試合開始時に一斉に発火しない）。

---

## 新しい HUD 要素を足す手順（C++ のクラスは増やさない）

例: 行動不能の間だけアイコンを出す。

1. **タグを用意する**: 既存のタグ（`UI/HUD/TreadHUDTags.h`）に無ければ追加する
   - C++ で使うタグ: `TreadHUDTags.h` に `UE_DECLARE_GAMEPLAY_TAG_EXTERN`、`.cpp` に `UE_DEFINE_GAMEPLAY_TAG_COMMENT` を 1 行ずつ
   - Blueprint からだけ書き込むタグ: プロジェクト設定 > GameplayTags に追加するだけでよい
2. **値を書き込む**: 値の出どころで 1 行書く
   ```cpp
   HUDData->SetValue(TreadHUDTags::Stun, StunRemaining, 0.0f, LongestStun);
   ```
   Blueprint からは機体の `HUDData` コンポーネントの `Set Value` を呼ぶ。毎フレーム呼んでよい（変わったときだけ通知される）
3. **配置する**: `WBP_BattleHUD` に `WBP_HUDImageSwitch` を置く
4. **タグを選ぶ**: `Bind Tag` = `HUD.Stun`、`States` に `Threshold` = `0.01`・`Texture` = アイコンを 1 件登録する
   - めったに変わらない値は `Update Mode` = `Event` にすると毎フレーム読みに行かない

---

## 画像を差し替える

- **同じ画像を描き直した場合**: `C:\ImpactVehicleGame\Image` の同じファイル名で置き換え、`Scripts/Setup_GUI01Assets.py` を再実行する
  （テクスチャを取り込み直す。コードの変更は不要）
- **別の画像にする場合**: 新しいテクスチャを取り込み、`WBP_HUDMeter`（全メーターの既定）か、配置したメーター（その 1 個だけ）の
  `Frame Texture` / `Background Texture` / `Fill Texture` / `Overlay Texture` を選び直す
- **画像は同じキャンバスサイズ・同じ基準位置で書き出すのが望ましい**。そろっていない場合は各層の `Layer Offset`（px）で合わせる
  - 現在の速度メーターの画像は枠 269×339・ゲージ 206×276 で、ゲージ 2 枚の `Layer Offset` を (3, 7) にしている

## 弧の値の求め方

`M_HUD_ArcFill` は `Fill Texture` を角度で切り抜く。角度は**右 = 0°、反時計回りが正**（画面の上が 90°）。

| 項目 | 求め方 | 速度メーターの値 |
|------|--------|-----------------|
| `Pivot UV` | 塗り画像上の**円の中心**の位置 ÷ 画像の幅・高さ（画像の中心ではない。0〜1 の外でもよい） | (160 / 206, 162.1 / 276) = (0.777, 0.587) |
| `Start Angle` | 0% の端の角度。端より少し外側にする | -138.5（左下の赤の端） |
| `Sweep Angle` | 100% の端までの回転量。**時計回りは負** | -151.5（青の端まで） |
| `Segment End Ratios` | 各セグメントの終端の角度 ÷ 全体の回転量（昇順、最後は 1） | 0.2 / 0.455 / 0.726 / 1.0 |

`Aspect`（幅 / 高さ）はテクスチャの寸法から自動で設定する。

デザイナーで `Preview Fill` を 0 → 1 に動かし、左下から時計回りに伸び、1 でちょうど全体が出ることを確かめる。
`Fill Mode` を `Stepped` にするとセグメント単位で点灯する。

## 速度の閾値を変えたとき

`DA_VehicleTuning`（`SmallDestructionSpeed` / `LargeDestructionSpeed` / `MaxSpeed`）や
`DA_ImpactTuning`（`OverdriveSpeed`）を変えたら、`Scripts/Setup_GUI01Assets.py` を再実行して
`CF_HUD_SpeedResponse` を作り直す。スクリプトは「超加速の速度が青の始端に写る」ことを検証する。

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\ImpactVehicleGame\Project\MyProject\MyProject.uproject" -ExecutePythonScript="C:\ImpactVehicleGame\Project\MyProject\Scripts\Setup_GUI01Assets.py" -unattended -nosplash -nop4
```

スクリプトは既存の `WBP_BattleHUD` の中身（デザイナーの配置）には触れない。
