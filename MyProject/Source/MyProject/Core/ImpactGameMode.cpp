// 踏みつけ加速メカゲーム — ゲームモード

#include "Core/ImpactGameMode.h"

#include "AI/EnemyVehicleAIController.h"
#include "AI/EnemyVehiclePawn.h"
#include "Core/ImpactPlayerController.h"
#include "Debug/ImpactDebugHUD.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/DestructibleComponent.h"
#include "Field/FieldSubsystem.h"
#include "Field/GoalComponent.h"
#include "Field/TeamComponent.h"
#include "Field/TeamPlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Score/ImpactScoreSubsystem.h"
#include "Score/ImpactScoreTypes.h"
#include "TimerManager.h"
#include "Tuning/MatchTuningDataAsset.h"
#include "Tuning/ScoreTuningDataAsset.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"
#include "Vehicle/ImpactVehiclePawn.h"

AImpactGameMode::AImpactGameMode()
{
	DefaultPawnClass = AImpactVehiclePawn::StaticClass();
	PlayerControllerClass = AImpactPlayerController::StaticClass();
	HUDClass = AImpactDebugHUD::StaticClass();
	GameStateClass = AImpactGameState::StaticClass();

	// 試合の進行（カウントダウン・経過時間・終了判定）と最高到達速度の記録に用いる。
	// 一時停止中は Tick しないため、リザルト表示中に時間が進むことはない。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AImpactGameMode::StartPlay()
{
	// 拠点は各自の BeginPlay（Super::StartPlay の中）で UFieldSubsystem に登録される。
	// 登録の通知を先に購読しておき、初期化の順序に関係なく攻める拠点を受け取る。
	UFieldSubsystem* Field = UFieldSubsystem::Get(this);
	if (Field)
	{
		Field->OnGoalRegistered.AddUniqueDynamic(this, &AImpactGameMode::HandleGoalRegistered);

		// 建物の破壊は GameState に記録する（道が開いたか・壊した数）。
		Field->OnObjectBroken.AddUniqueDynamic(this, &AImpactGameMode::HandleFieldObjectBroken);

		// 購読より前に登録済みの拠点（GameMode より先に初期化された場合）も拾う。
		HandleGoalRegistered(Field->FindHostileGoal(PlayerTeamId));
	}

	Super::StartPlay();

	if (UImpactScoreSubsystem* Score = GetScoreSubsystem())
	{
		Score->ResetScore();
	}

	if (!ScoreTuning)
	{
		UE_LOG(LogImpactMatch, Warning, TEXT("ScoreTuning is not set on %s; score will not be recorded"), *GetName());
	}

	if (CachedGoal.IsValid())
	{
		UE_LOG(LogImpactMatch, Log, TEXT("goal found: %s (durability %d)"),
			*GetNameSafe(CachedGoal->GetOwner()), CachedGoal->GetDurability());

		const int32 GoalCount = Field ? Field->GetGoalCount() : 1;
		if (GoalCount > 1)
		{
			UE_LOG(LogImpactMatch, Log, TEXT("%d goals on this map; attacking %s"), GoalCount, *GetNameSafe(CachedGoal->GetOwner()));
		}
	}
	else
	{
		UE_LOG(LogImpactMatch, Log, TEXT("no goal on this map"));
	}

	// 画面は GameMode ではなく GameState を読むため、配点とゴールの状態を GameState へ渡しておく。
	if (AImpactGameState* State = GetImpactGameState())
	{
		State->SetScoreTuning(ScoreTuning);
	}
	PublishGoalState();

	SpawnEnemy();
	BeginMatchFlow();

	bMatchStartedUp = true;
}

void AImpactGameMode::BeginMatchFlow()
{
	AImpactGameState* State = GetImpactGameState();
	if (!State)
	{
		UE_LOG(LogImpactMatch, Error, TEXT("GameState of %s is not an AImpactGameState; the match flow is disabled"), *GetName());
		return;
	}

	if (CachedGoal.IsValid() && MatchTuning)
	{
		State->ConfigureMatch(MatchTuning->CountdownSeconds, MatchTuning->MatchDurationSeconds);
		UE_LOG(LogImpactMatch, Log, TEXT("solo match: countdown %.1f s, time limit %.0f s"),
			MatchTuning->CountdownSeconds, MatchTuning->MatchDurationSeconds);
		SetMatchState(EImpactMatchState::Countdown);
		return;
	}

	if (CachedGoal.IsValid())
	{
		UE_LOG(LogImpactMatch, Warning, TEXT("MatchTuning is not set on %s; playing without a time limit"), *GetName());
	}

	// テストコースでは挙動の検証を途中で打ち切らないよう、時間を制限しない。
	State->ConfigureMatch(0.0f, 0.0f);
	UE_LOG(LogImpactMatch, Log, TEXT("free play: no time limit"));
	SetMatchState(EImpactMatchState::InProgress);
}

void AImpactGameMode::SetMatchState(EImpactMatchState NewState)
{
	AImpactGameState* State = GetImpactGameState();
	if (!State)
	{
		return;
	}

	State->SetMatchState(NewState);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (AImpactVehiclePawn* Vehicle = PlayerController ? Cast<AImpactVehiclePawn>(PlayerController->GetPawn()) : nullptr)
		{
			ApplyControlLock(*Vehicle);
		}
	}
}

void AImpactGameMode::ApplyControlLock(AImpactVehiclePawn& Vehicle) const
{
	const AImpactGameState* State = GetImpactGameState();
	Vehicle.SetControlsLocked(State && !State->IsControlAllowed());
}

void AImpactGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	// 自機が出現するたびに、その機体の出来事を得点へつなぐ。
	// 敵機の出来事はつながないため、得点になるのは自機の行動だけになる。
	AImpactVehiclePawn* Vehicle = NewPlayer ? Cast<AImpactVehiclePawn>(NewPlayer->GetPawn()) : nullptr;
	if (UTeamComponent* Team = Vehicle ? Vehicle->GetTeam() : nullptr)
	{
		// プレイヤーの陣営はここで決める。拠点へのダメージ可否・ロックオン対象はこの陣営を基準に判定される。
		Team->SetTeamId(PlayerTeamId);
	}

	UImpactVehicleMovementComponent* Movement = Vehicle ? Vehicle->GetVehicleMovement() : nullptr;
	if (!Movement)
	{
		UE_LOG(LogImpactMatch, Warning, TEXT("player vehicle is missing on restart; its events will not be scored"));
		return;
	}

	Movement->OnGameplayEvent.AddUniqueDynamic(this, &AImpactGameMode::HandlePlayerVehicleEvent);
	UE_LOG(LogImpactMatch, Log, TEXT("scoring events from %s"), *Vehicle->GetName());

	// 自機は試合の開始（StartPlay）より前に出現することがあるため、出現時点の段階に合わせる。
	ApplyControlLock(*Vehicle);
}

void AImpactGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AImpactGameState* State = GetImpactGameState();
	if (!State)
	{
		return;
	}

	switch (State->GetMatchState())
	{
	case EImpactMatchState::Countdown:
		State->AdvanceCountdown(DeltaSeconds);
		if (State->GetCountdownRemaining() <= 0.0f)
		{
			SetMatchState(EImpactMatchState::InProgress);
		}
		break;

	case EImpactMatchState::InProgress:
		State->AdvanceMatchTime(DeltaSeconds);
		RecordPlayerSpeed();
		CheckEndConditions(*State);
		break;

	default:
		break;
	}
}

void AImpactGameMode::CheckEndConditions(const AImpactGameState& State)
{
	// ソロモードは NPC を相手にしたスコアアタックで残機の概念がないため、終了条件は制限時間のみ。
	// ゴールを破壊しても試合は続き、残り時間で他の得点を稼ぐ。
	if (State.HasTimeLimit() && State.GetRemainingTime() <= 0.0f)
	{
		FinishMatch(EImpactMatchResult::TimeUp);
	}
}

void AImpactGameMode::FinishMatch(EImpactMatchResult Result)
{
	AImpactGameState* State = GetImpactGameState();
	if (!State)
	{
		return;
	}

	const UImpactScoreSubsystem* Score = GetScoreSubsystem();
	const int32 FinalScore = (Score && ScoreTuning) ? ScoreTuning->ComputeTotal(Score->GetTally()) : 0;
	UE_LOG(LogImpactMatch, Log, TEXT("match finished: %s (elapsed %.1f s, crashes %d, final score %d)"),
		LexToDisplayString(Result), State->GetElapsedTime(), State->GetCrashCount(), FinalScore);

	State->SetMatchResult(Result);
	SetMatchState(EImpactMatchState::Finished);

	// 結果が確定した時点の盤面で止める。リザルト画面の操作は一時停止中でも受け付けられる。
	UGameplayStatics::SetGamePaused(this, true);
}

void AImpactGameMode::RecordPlayerSpeed()
{
	// 最高到達速度は出来事ではなく状態なので、毎フレーム自機の速度を見て更新する。
	UImpactScoreSubsystem* Score = GetScoreSubsystem();
	const AImpactVehiclePawn* Vehicle = GetPlayerVehicle();
	if (Score && Vehicle)
	{
		Score->RecordSpeed(Vehicle->GetCurrentSpeed());
	}
}

void AImpactGameMode::RestartMatch()
{
	// PIE ではパッケージ名に PIE 用の接頭辞が付くため、取り除いてから開き直す。
	const FString MapPath = UWorld::RemovePIEPrefix(GetWorld()->GetPackage()->GetName());
	UE_LOG(LogImpactMatch, Log, TEXT("restarting match: %s"), *MapPath);
	UGameplayStatics::OpenLevel(this, FName(*MapPath));
}

void AImpactGameMode::ReturnToTitle()
{
	if (!MatchTuning || MatchTuning->TitleMap.IsNull())
	{
		UE_LOG(LogImpactMatch, Error, TEXT("cannot return to title: TitleMap is not set on %s"), *GetName());
		return;
	}

	UE_LOG(LogImpactMatch, Log, TEXT("returning to title: %s"), *MatchTuning->TitleMap.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, MatchTuning->TitleMap);
}

AEnemyVehiclePawn* AImpactGameMode::GetCurrentEnemy() const
{
	return CurrentEnemy.Get();
}

UGoalComponent* AImpactGameMode::GetGoal() const
{
	return CachedGoal.Get();
}

AActor* AImpactGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 陣営付きの開始地点のうち、プレイヤーの陣営と一致する最初のもの。開始地点は数個のため素直に走査する。
	for (TActorIterator<ATeamPlayerStart> It(GetWorld()); It; ++It)
	{
		if (UTeamComponent::GetTeamIdOf(*It) == PlayerTeamId)
		{
			return *It;
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

AImpactVehiclePawn* AImpactGameMode::GetPlayerVehicle() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (AImpactVehiclePawn* Vehicle = PlayerController ? Cast<AImpactVehiclePawn>(PlayerController->GetPawn()) : nullptr)
		{
			return Vehicle;
		}
	}

	return nullptr;
}

void AImpactGameMode::PublishGoalState()
{
	AImpactGameState* State = GetImpactGameState();
	if (!State)
	{
		return;
	}

	const UGoalComponent* Goal = CachedGoal.Get();
	State->SetGoalState(Goal != nullptr, Goal ? Goal->GetDurability() : 0, Goal ? Goal->GetMaxDurability() : 0);
}

UImpactScoreSubsystem* AImpactGameMode::GetScoreSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UImpactScoreSubsystem>() : nullptr;
}

AImpactGameState* AImpactGameMode::GetImpactGameState() const
{
	return GetGameState<AImpactGameState>();
}

void AImpactGameMode::SpawnEnemy()
{
	if (!EnemyVehicleClass)
	{
		UE_LOG(LogImpactAI, Warning, TEXT("EnemyVehicleClass is not set on %s; no enemy will spawn"), *GetName());
		return;
	}

	// ゴールがあれば、守備側として守備位置（防衛ラインのマーカー、無ければゴールの正面）に出す。
	const FTransform SpawnTransform = CachedGoal.IsValid()
		? CachedGoal->GetGuardTransform()
		: EnemySpawnTransform;

	FActorSpawnParameters Params;
	// 出現位置で干渉しても押し出して出す。リスポーン時に自機が居座っていても出現できるようにする。
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyVehiclePawn* Enemy = GetWorld()->SpawnActor<AEnemyVehiclePawn>(EnemyVehicleClass, SpawnTransform, Params);
	CurrentEnemy = Enemy;

	UE_LOG(LogImpactAI, Log, TEXT("enemy spawned: %s at %s"), *GetNameSafe(Enemy), *SpawnTransform.GetLocation().ToString());
}

void AImpactGameMode::HandlePlayerVehicleEvent(EVehicleGameplayEvent Event, int32 Amount)
{
	// ゴールの耐久値は命中の時点で既に減っている。表示が実際の値とずれないよう、段階に関係なく反映する。
	if (Event == EVehicleGameplayEvent::GoalHit)
	{
		PublishGoalState();
	}

	// 試合中の出来事だけを数える。カウントダウン中や終了後の出来事は結果に含めない。
	AImpactGameState* State = GetImpactGameState();
	if (State && !State->IsControlAllowed())
	{
		return;
	}

	// 自滅回数は画面に表示するため、配点の有無に関係なく数える。
	if (State && Event == EVehicleGameplayEvent::Crash)
	{
		State->AddCrash();
	}

	UImpactScoreSubsystem* Score = GetScoreSubsystem();
	if (!Score || !ScoreTuning)
	{
		return;
	}

	Score->RecordVehicleEvent(Event, Amount, *ScoreTuning, GetWorld()->GetTimeSeconds());
}

void AImpactGameMode::HandleGoalDestroyed(UGoalComponent* DestroyedGoal)
{
	// 破壊しても試合は終わらない。守るゴールを失った敵機は攻撃に切り替わる（AEnemyVehicleAIController::FindGoal）。
	UE_LOG(LogImpactMatch, Log, TEXT("goal destroyed: %s"), *GetNameSafe(DestroyedGoal ? DestroyedGoal->GetOwner() : nullptr));
	PublishGoalState();
}

void AImpactGameMode::HandleGoalRegistered(UGoalComponent* Goal)
{
	// ソロモードで攻める拠点は1つ。最初に見つかった敵陣の拠点を使う。
	if (CachedGoal.IsValid() || !Goal
		|| !UTeamComponent::AreTeamsHostile(PlayerTeamId, UTeamComponent::GetTeamIdOf(Goal->GetOwner())))
	{
		return;
	}

	CachedGoal = Goal;
	Goal->OnGoalDestroyed.AddUniqueDynamic(this, &AImpactGameMode::HandleGoalDestroyed);

	// 試合の開始後に登録された拠点（途中で出現・ストリーミングされたもの）は、表示に使う状態だけをすぐ反映する。
	// 試合の形式（ソロモードかフリープレイか）は開始時に決まるため、途中で切り替えない。
	// 敵機は次の出現から、この拠点の守備位置に出る（SpawnEnemy が CachedGoal を読む）。
	if (bMatchStartedUp)
	{
		UE_LOG(LogImpactMatch, Log, TEXT("goal registered after the match started: %s"), *GetNameSafe(Goal->GetOwner()));
		PublishGoalState();
	}
}

void AImpactGameMode::HandleFieldObjectBroken(UDestructibleComponent* Destructible)
{
	if (AImpactGameState* State = GetImpactGameState())
	{
		State->RecordFieldObjectBroken(Destructible ? Destructible->GetOpenedRouteTag() : NAME_None);
	}
}

void AImpactGameMode::NotifyEnemyDefeated(AEnemyVehiclePawn* Enemy)
{
	++EnemiesDefeated;
	CurrentEnemy.Reset();

	UE_LOG(LogImpactAI, Log, TEXT("enemy defeated: %s (total %d)"), *GetNameSafe(Enemy), EnemiesDefeated);

	const AImpactGameState* State = GetImpactGameState();
	const bool bCountsForScore = !State || State->IsControlAllowed();
	UImpactScoreSubsystem* Score = GetScoreSubsystem();
	if (bCountsForScore && Score && ScoreTuning)
	{
		Score->RecordEnemyDefeated(*ScoreTuning, GetWorld()->GetTimeSeconds());
	}

	// 間隔 0 のタイマーは登録されず発火しないため、最小値を保証する。
	const float Delay = FMath::Max(EnemyRespawnDelay, KINDA_SMALL_NUMBER);
	GetWorldTimerManager().SetTimer(EnemyRespawnTimer, this, &AImpactGameMode::SpawnEnemy, Delay, false);
}
