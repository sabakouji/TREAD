// 踏みつけ加速メカゲーム — 敵機 Pawn

#include "AI/EnemyVehiclePawn.h"

#include "AI/EnemyVehicleAIController.h"
#include "Components/StaticMeshComponent.h"
#include "Core/ImpactGameMode.h"
#include "Engine/World.h"
#include "Field/TeamComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tuning/AITuningDataAsset.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"

namespace
{
	/** エンジンの BasicShapeMaterial が持つ色のベクトルパラメータ名。 */
	const FName BodyColorParameterName(TEXT("Color"));
}

AEnemyVehiclePawn::AEnemyVehiclePawn()
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AEnemyVehicleAIController::StaticClass();

	// バブルも自機と見分けられる色にする。超加速時の色は基底クラスの設定を共用する。
	BubbleColor = FLinearColor(1.0f, 0.2f, 0.15f, 1.0f);

	// ソロモードの NPC は拠点と同じ陣営に属し、自陣の拠点を守る。
	Team->SetTeamId(ImpactTeam::Opponent);
}

void AEnemyVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyBodyColor();
}

void AEnemyVehiclePawn::ApplyBodyColor()
{
	// 追加のマテリアルアセットを作らず、メッシュ既定のマテリアルのパラメータを動的に変える。
	UMaterialInstanceDynamic* Material = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (!Material)
	{
		UE_LOG(LogImpactAI, Warning, TEXT("%s: could not create a dynamic material; body color not applied"), *GetName());
		return;
	}

	Material->SetVectorParameterValue(BodyColorParameterName, BodyColor);

	// 存在しないパラメータ名を指定しても SetVectorParameterValue は黙って何もしない。
	// 自機と見分けがつかないという分かりにくい形でしか現れないため、読み直して検証する。
	FLinearColor Applied;
	if (!Material->GetVectorParameterValue(FHashedMaterialParameterInfo(BodyColorParameterName), Applied))
	{
		UE_LOG(LogImpactAI, Warning, TEXT("%s: material has no '%s' parameter; body color not applied"),
			*GetName(), *BodyColorParameterName.ToString());
	}
}

void AEnemyVehiclePawn::HandleDefeatedInClash_Implementation()
{
	if (bDefeated)
	{
		return;
	}
	bDefeated = true;

	// 撃破は移動コンポーネントの衝突処理の最中に呼ばれるため、その場で Destroy すると
	// 処理中のコンポーネントが登録解除されて危険。当たり判定と表示を切ってから遅延消滅させる。
	VehicleMovement->SetMoveInput(FVector2D::ZeroVector);
	VehicleMovement->SetBrakeInput(false);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(DefeatDespawnDelay);

	// 操作していた AI も一緒に片付ける。残すとリスポーンのたびに操作対象のない AI が溜まる。
	if (AController* OwningController = GetController())
	{
		OwningController->SetLifeSpan(DefeatDespawnDelay);
	}

	if (UWorld* World = GetWorld())
	{
		if (AImpactGameMode* GameMode = World->GetAuthGameMode<AImpactGameMode>())
		{
			GameMode->NotifyEnemyDefeated(this);
		}
	}
}
