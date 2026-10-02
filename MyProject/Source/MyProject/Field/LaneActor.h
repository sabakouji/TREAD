// 踏みつけ加速メカゲーム — 踏み台の行進ルート

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LaneActor.generated.h"

class USplineComponent;

/**
 * 踏み台（戦車）が行進するルート。スプラインで地面の上の経路を描く。
 *
 * NavMesh で歩かせるよりスプライン追従のほうが挙動が読みやすく、
 * プレイヤーが「ここに踏み台が来る」と予測しやすいため、この方式を採る（フィールド設計 2.3）。
 * スプラインの制御点は地面の高さに置く。踏み台は底面がスプラインに乗るように進む。
 */
UCLASS()
class MYPROJECT_API ALaneActor : public AActor
{
	GENERATED_BODY()

public:
	ALaneActor();

	/** ルートの全長（uu）。 */
	UFUNCTION(BlueprintPure, Category = "Lane")
	float GetLength() const;

	/** 始点からの距離（uu）にある地面上の位置。 */
	UFUNCTION(BlueprintPure, Category = "Lane")
	FVector GetGroundLocationAtDistance(float Distance) const;

	/** 始点からの距離（uu）における進行方向（水平・正規化済み）。 */
	UFUNCTION(BlueprintPure, Category = "Lane")
	FVector GetDirectionAtDistance(float Distance) const;

	USplineComponent* GetSpline() const { return Spline; }

protected:
	/** 行進ルート。ルートコンポーネント。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> Spline;
};
