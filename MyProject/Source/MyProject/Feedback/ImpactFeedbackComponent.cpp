// 踏みつけ加速メカゲーム — 衝突の手応えの演出

#include "Feedback/ImpactFeedbackComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Tuning/FeedbackTuningDataAsset.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"
#include "Vehicle/ImpactVehiclePawn.h"

namespace
{
	/** 通常の時間の進み方。 */
	constexpr float NormalTimeDilation = 1.0f;
}

UImpactFeedbackComponent::UImpactFeedbackComponent()
{
	// 出来事の通知とタイマーで動くため、毎フレームの処理は持たない。
	PrimaryComponentTick.bCanEverTick = false;
}

void UImpactFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller)
	{
		return;
	}

	// 機体は BeginPlay より後に操作を始めることもあるため、操作対象の変化を購読する。
	Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &UImpactFeedbackComponent::HandlePossessedPawnChanged);
	BindToPawn(Controller->GetPawn());
}

void UImpactFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// レベル遷移などで途中で終わっても、時間の進み方を遅いまま残さない。
	EndHitStop();

	Super::EndPlay(EndPlayReason);
}

void UImpactFeedbackComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void UImpactFeedbackComponent::BindToPawn(APawn* Pawn)
{
	if (UImpactVehicleMovementComponent* Previous = BoundMovement.Get())
	{
		Previous->OnGameplayEvent.RemoveDynamic(this, &UImpactFeedbackComponent::HandleVehicleEvent);
	}
	BoundMovement.Reset();

	const AImpactVehiclePawn* Vehicle = Cast<AImpactVehiclePawn>(Pawn);
	UImpactVehicleMovementComponent* Movement = Vehicle ? Vehicle->GetVehicleMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	Movement->OnGameplayEvent.AddUniqueDynamic(this, &UImpactFeedbackComponent::HandleVehicleEvent);
	BoundMovement = Movement;
}

void UImpactFeedbackComponent::HandleVehicleEvent(EVehicleGameplayEvent Event, int32 Amount)
{
	PlayFeedback(Event);
}

void UImpactFeedbackComponent::PlayFeedback(EVehicleGameplayEvent Event)
{
	const FImpactFeedbackPreset* Preset = FeedbackTuning ? FeedbackTuning->FindPreset(Event) : nullptr;
	if (!Preset)
	{
		return;
	}

	// 手応えは操作している本人の画面にだけ出す。
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	if (Preset->ShakeScale > 0.0f && FeedbackTuning->ShakeClass && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraShake(FeedbackTuning->ShakeClass, Preset->ShakeScale);
	}

	StartHitStop(Preset->HitStopDuration, Preset->HitStopTimeDilation);
}

bool UImpactFeedbackComponent::IsHitStopActive() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(HitStopTimer);
}

void UImpactFeedbackComponent::StartHitStop(float RealDuration, float TimeDilation)
{
	UWorld* World = GetWorld();
	if (!World || RealDuration <= 0.0f || TimeDilation >= NormalTimeDilation)
	{
		return;
	}

	// 実行中のものと合成する。建物を壊した直後に小物を壊しても、長く深いヒットストップは縮まない。
	const double Now = World->GetRealTimeSeconds();
	HitStop = MergeHitStop(HitStop, Now, RealDuration, TimeDilation);

	// ソロモード専用: ゲーム全体の時間を遅くする。解除のタイマー自体も遅くなるため、残りの実時間を換算して設定する。
	UGameplayStatics::SetGlobalTimeDilation(this, HitStop.TimeDilation);
	World->GetTimerManager().SetTimer(
		HitStopTimer, this, &UImpactFeedbackComponent::EndHitStop,
		UFeedbackTuningDataAsset::ToDilatedTimerDelay(static_cast<float>(HitStop.EndRealTime - Now), HitStop.TimeDilation), false);
}

FHitStopState UImpactFeedbackComponent::MergeHitStop(
	const FHitStopState& Current, double NowRealTime, float RealDuration, float TimeDilation)
{
	FHitStopState Requested;
	Requested.TimeDilation = FMath::Clamp(TimeDilation, KINDA_SMALL_NUMBER, NormalTimeDilation);
	Requested.EndRealTime = NowRealTime + FMath::Max(RealDuration, 0.0f);

	if (!Current.IsActiveAt(NowRealTime))
	{
		return Requested;
	}

	FHitStopState Merged;
	Merged.TimeDilation = FMath::Min(Current.TimeDilation, Requested.TimeDilation);
	Merged.EndRealTime = FMath::Max(Current.EndRealTime, Requested.EndRealTime);
	return Merged;
}

void UImpactFeedbackComponent::EndHitStop()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(HitStopTimer);
	HitStop = FHitStopState();
	UGameplayStatics::SetGlobalTimeDilation(this, NormalTimeDilation);
}
