// 踏みつけ加速メカゲーム — デバッグ HUD

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ImpactDebugHUD.generated.h"

class AImpactVehiclePawn;
class UVehicleTuningDataAsset;

/**
 * 機体の内部状態を画面へ常時描画するデバッグ HUD。
 *
 * 手触りの評価には「今どの速度で、どれだけ曲がれるのか」を数値で追える必要があるため、
 * プロトタイプ期間中は既定で表示する。表示は F1 キーまたはコンソールコマンド `ImpactShowDebug` で切り替える。
 *
 * 加点・減点のポップアップはプレイ中のフィードバックであるため、デバッグ表示を消しても出し続ける。
 * 試合終了後はリザルト画面に任せ、何も描かない。
 */
UCLASS()
class MYPROJECT_API AImpactDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	AImpactDebugHUD();

	virtual void DrawHUD() override;

	/** デバッグ情報の表示切り替え。 */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void SetShowVehicleDebug(bool bInShow) { bShowVehicleDebug = bInShow; }

	UFUNCTION(BlueprintPure, Category = "Debug")
	bool IsShowingVehicleDebug() const { return bShowVehicleDebug; }

protected:
	/** 機体のデバッグ情報を表示するか。親クラス AHUD の bShowDebugInfo とは別物。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debug")
	bool bShowVehicleDebug = true;

	/** 描画開始位置（画面左上からのオフセット、px）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	FVector2D DebugDrawOrigin = FVector2D(24.0f, 24.0f);

	/** 行間（px）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	float DebugLineHeight = 20.0f;

	/** 見出しの色。項目のまとまりごとに見出しの色を分け、視線が迷わないようにする。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Style")
	FLinearColor HeaderColor = FLinearColor(0.55f, 0.85f, 1.0f, 1.0f);

	/** 値の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Style")
	FLinearColor ValueColor = FLinearColor::White;

	/** 注意を促す値（行動不能・破壊できない速度など）の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Style")
	FLinearColor WarnColor = FLinearColor(1.0f, 0.65f, 0.2f, 1.0f);

	/** 得点のまとまりの見出しの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Style")
	FLinearColor ScoreHeaderColor = FLinearColor(0.6f, 1.0f, 0.5f, 1.0f);

	/** 戦闘（敵機・機体同士の衝突）のまとまりの見出しの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Style")
	FLinearColor CombatHeaderColor = FLinearColor(1.0f, 0.55f, 0.35f, 1.0f);

	/** 加点のポップアップの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Style")
	FLinearColor PopupPositiveColor = FLinearColor(0.5f, 1.0f, 0.4f, 1.0f);

	/** 減点のポップアップの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Style")
	FLinearColor PopupNegativeColor = FLinearColor(1.0f, 0.35f, 0.3f, 1.0f);

private:
	/** 1行描画し、次の行の Y 座標を返す。 */
	float DrawDebugLine(const FString& Text, float PosY, const FLinearColor& Color);

	/** 操作対象の機体を取得する。取得できない場合は nullptr。 */
	AImpactVehiclePawn* GetViewedVehicle() const;

	/**
	 * 機体同士の戦闘に関する情報（敵機の状態・直近の衝突の相殺内訳）を描画し、次の行の Y 座標を返す。
	 * 敵機・AI への依存を本体から切り離すため、実装は ImpactDebugHUDCombat.cpp に置く。
	 */
	float DrawCombatDebug(const AImpactVehiclePawn& Vehicle, float PosY);

	/** 超加速の状態と接触半径を描画し、次の行の Y 座標を返す。実装は ImpactDebugHUDCombat.cpp に置く。 */
	float DrawOverdriveDebug(const AImpactVehiclePawn& Vehicle, float PosY);

	/** ロックオンの対象とアシストの作動状態を描画し、次の行の Y 座標を返す。実装は ImpactDebugHUDCombat.cpp に置く。 */
	float DrawLockOnDebug(const AImpactVehiclePawn& Vehicle, float PosY);

	/**
	 * ゴールの耐久値と得点の内訳を描画し、次の行の Y 座標を返す。
	 * 実装は ImpactDebugHUDScore.cpp に置く。
	 */
	float DrawScoreDebug(float PosY);

	/** 得点の集計を取得し、加点・減点のポップアップを描画する。実装は ImpactDebugHUDScore.cpp に置く。 */
	void DrawScoreFeedback();

	/** 直近の加点・減点を画面中央付近にポップアップ表示する。 */
	void DrawScorePopups(const class UImpactScoreSubsystem& Score, double Now);

	/** 加点・減点のポップアップを表示し続ける時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Score", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ScorePopupDuration = 1.5f;

	/** ポップアップの表示開始位置（画面の高さに対する割合）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Score", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScorePopupTopRatio = 0.3f;
};
