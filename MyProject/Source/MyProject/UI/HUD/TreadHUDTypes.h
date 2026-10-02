// 踏みつけ加速メカゲーム — 汎用 HUD 部品の型定義

#pragma once

#include "CoreMinimal.h"
#include "TreadHUDTypes.generated.h"

class UTexture2D;

/** HUD に公開する値 1 件。正規化値は SetValue の時点で求めておく。 */
USTRUCT(BlueprintType)
struct FTreadHUDValue
{
	GENERATED_BODY()

	/** 元の値（単位は値ごとに異なる。速度なら uu/s）。 */
	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD")
	float Raw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD")
	float Min = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD")
	float Max = 1.0f;

	/** Min〜Max を 0〜1 に写した値。範囲外は切り詰める。 */
	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD")
	float Normalized = 0.0f;
};

/** 表示部品が値を受け取る方式。 */
UENUM(BlueprintType)
enum class ETreadHUDUpdateMode : uint8
{
	/** 毎フレーム値を読みに行く。毎フレーム変わる値（速度など）向け。 */
	Tick,

	/** 値が変わったときの通知だけで受け取る。めったに変わらない値向け。 */
	Event
};

/** メーターの伸び方。 */
UENUM(BlueprintType)
enum class ETreadHUDFillMode : uint8
{
	/** 値どおり滑らかに伸びる。 */
	Continuous,

	/** セグメント単位で点灯する。セグメントに入った時点でそのセグメント全体が点灯する。 */
	Stepped
};

/** 画像切替部品の状態 1 つ。元の値（Raw）が Threshold 以上の状態のうち、閾値が最大のものを表示する。 */
USTRUCT(BlueprintType)
struct FTreadHUDImageState
{
	GENERATED_BODY()

	/** この状態になる元の値の下限（Raw と比較する。正規化値ではない）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	float Threshold = 0.0f;

	/** 表示する画像。未設定なら、この状態の間は何も表示しない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	TObjectPtr<UTexture2D> Texture;

	/** 画像に掛ける色。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	FLinearColor Tint = FLinearColor::White;
};

/** HUD.LockOn に書き込む値。画像切替部品の Threshold にはこの値を使う。 */
namespace TreadHUDLockOn
{
	/** 捕捉している相手がいない。 */
	constexpr float None = 0.0f;

	/** 相手を捕捉しているが、ステアリングアシストは作動していない。 */
	constexpr float Locked = 1.0f;

	/** ステアリングアシストが作動している。 */
	constexpr float Assisting = 2.0f;
}
