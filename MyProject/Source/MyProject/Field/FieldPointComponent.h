// 踏みつけ加速メカゲーム — NPC 用の地点

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FieldPointComponent.generated.h"

/** NPC の役割傾向ごとの地点の種類。 */
UENUM(BlueprintType)
enum class EFieldPointRole : uint8
{
	/** 攻撃役が突撃の機会を待つ地点。 */
	AttackerStandby,

	/** 守備役が拠点を守る地点。置かれていれば、守備側の NPC の出現・待機位置になる。 */
	DefenseLine,

	/** 踏み台を確保しやすい地点。 */
	StompSupply,

	/** 加速を始めるのに適した直線の起点。向き（前方）が直線の方向を表す。 */
	AccelerationStart
};

/**
 * NPC がフィールドを理解する手がかりとなる地点。位置と向きはこの部品の Transform で表す。
 *
 * 陣営は同じ Actor に付けた UTeamComponent から読む（無ければ中立で、どの陣営からも使える）。
 * BeginPlay で UFieldSubsystem に登録され、NPC や GameMode は役割と陣営で検索する。
 */
UCLASS(ClassGroup = (Impact), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UFieldPointComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFieldPointComponent();

	UFUNCTION(BlueprintPure, Category = "Field Point")
	EFieldPointRole GetRole() const { return Role; }

	/** この地点の有効範囲の半径（uu）。NPC が「地点に着いた」とみなす距離。 */
	UFUNCTION(BlueprintPure, Category = "Field Point")
	float GetRadius() const { return Radius; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 地点の役割。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field Point")
	EFieldPointRole Role = EFieldPointRole::DefenseLine;

	/** 有効範囲の半径（uu）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field Point", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Radius = 500.0f;
};
