// 踏みつけ加速メカゲーム — NPC 用マーカー

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FieldMarkerActor.generated.h"

class UArrowComponent;
class UFieldPointComponent;
class UTeamComponent;

/**
 * マップに置く NPC 用マーカー。地点（UFieldPointComponent）と陣営（UTeamComponent）を組み合わせるだけの Actor。
 * 役割・半径・陣営は配置したインスタンスごとに設定する。
 */
UCLASS()
class MYPROJECT_API AFieldMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	AFieldMarkerActor();

	UFieldPointComponent* GetFieldPoint() const { return FieldPoint; }

protected:
	/** 地点。ルートコンポーネント。位置と向きがマーカーの位置と向きになる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFieldPointComponent> FieldPoint;

	/** 陣営。中立ならどの陣営の NPC からも使える。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTeamComponent> Team;

	/** エディタ上で向きを確認するための矢印。ゲーム中は表示されない。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> DirectionArrow;
};
