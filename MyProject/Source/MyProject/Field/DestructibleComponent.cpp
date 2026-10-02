// 踏みつけ加速メカゲーム — 破壊可能オブジェクト

#include "Field/DestructibleComponent.h"

#include "AI/NavigationSystemBase.h"
#include "Collision/ImpactResolver.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Field/FieldSubsystem.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Score/ImpactScoreTypes.h"
#include "TimerManager.h"
#include "Tuning/FeedbackTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"

UDestructibleComponent::UDestructibleComponent()
{
	// 状態は衝突を受けたときだけ変わる。毎フレームの処理を持たない。
	PrimaryComponentTick.bCanEverTick = false;
}

void UDestructibleComponent::BeginPlay()
{
	Super::BeginPlay();

	// フィールド上の建物はすべて分割する（FIELD-02）。破片が無いと壊れても消えるだけになるため、配置の漏れを知らせる。
	if (!DestructionGC)
	{
		UE_LOG(LogImpactMatch, Warning,
			TEXT("%s: DestructionGC is not assigned; buildings must fracture (use UBreakablePropComponent for non-fracturing props)"),
			*GetNameSafe(GetOwner()));
	}
	else if (!FeedbackTuning)
	{
		// 破片の初速・崩す範囲・寿命は FeedbackTuning から読む。未設定だと破片が何も言わずに出ないため知らせる。
		UE_LOG(LogImpactMatch, Warning,
			TEXT("%s: DestructionGC is assigned but FeedbackTuning (DA_FeedbackTuning) is not; no debris will spawn"),
			*GetNameSafe(GetOwner()));
	}

	VisualMesh = FindVisualMesh();
	if (!VisualMesh)
	{
		if (!VisualMeshName.IsNone())
		{
			UE_LOG(LogImpactMatch, Warning, TEXT("%s: visual mesh '%s' is not found; the look will not change on damage"),
				*GetNameSafe(GetOwner()), *VisualMeshName.ToString());
		}
		return;
	}

	// Static のメッシュはプレイ中に差し替えられない。ADestructibleObstacleActor は構築時に Movable にしているが、
	// レベルに置いた StaticMeshActor や Blueprint だけで作った建物に付けた場合に備えて、ここでも補う。
	if (VisualMesh->Mobility == EComponentMobility::Static)
	{
		VisualMesh->SetMobility(EComponentMobility::Movable);
	}
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UDestructibleComponent::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);

	if (!DestructionGC)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s: DestructionGC is not assigned. Buildings must fracture; use UBreakablePropComponent for non-fracturing props."),
			*GetPathName())));
	}
	else if (!FeedbackTuning)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s: DestructionGC is assigned but FeedbackTuning (DA_FeedbackTuning) is not; no debris will spawn."),
			*GetPathName())));
	}

	return Result;
}
#endif

UStaticMeshComponent* UDestructibleComponent::FindVisualMesh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (VisualMeshName.IsNone())
	{
		return Owner->FindComponentByClass<UStaticMeshComponent>();
	}

	TInlineComponentArray<UStaticMeshComponent*> Meshes(Owner);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetFName() == VisualMeshName)
		{
			return Mesh;
		}
	}

	return nullptr;
}

bool UDestructibleComponent::IsDirectionAccepted(
	bool bInFrontOnly, float InFrontOnlyAngle, const FVector& OwnerForward, const FVector& ImpactDirection)
{
	if (!bInFrontOnly)
	{
		return true;
	}

	// 正面に当たる機体は、対象の前方向と逆向きに進んでくる。
	const FVector Forward = OwnerForward.GetSafeNormal2D();
	const FVector Incoming = (-ImpactDirection).GetSafeNormal2D();
	if (Forward.IsNearlyZero() || Incoming.IsNearlyZero())
	{
		return false;
	}

	const float Angle = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, Incoming), -1.0f, 1.0f)));
	return Angle <= InFrontOnlyAngle;
}

EImpactReceiveOutcome UDestructibleComponent::ResolveHit(int32 HitsTakenBefore, int32 InMaxHP)
{
	return HitsTakenBefore + 1 >= InMaxHP ? EImpactReceiveOutcome::Destroyed : EImpactReceiveOutcome::Weakened;
}

int32 UDestructibleComponent::GetDamagedMeshIndex(int32 InHitsTaken, int32 DamagedMeshCount)
{
	if (InHitsTaken <= 0 || DamagedMeshCount <= 0)
	{
		return INDEX_NONE;
	}

	return FMath::Min(InHitsTaken, DamagedMeshCount) - 1;
}

FImpactReceiveResult UDestructibleComponent::ReceiveVehicleImpact_Implementation(const FImpactReceiveContext& Context)
{
	FImpactReceiveResult Result;
	Result.Rank = Rank;

	if (bBroken || !Context.Tuning)
	{
		return Result;
	}

	const AActor* Owner = GetOwner();
	const FVector OwnerForward = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	if (!IsDirectionAccepted(bFrontOnly, FrontOnlyAngle, OwnerForward, Context.ImpactDirection))
	{
		return Result;
	}

	if (Context.ImpactSpeed < FImpactResolver::GetRequiredSpeed(Rank, *Context.Tuning))
	{
		return Result;
	}

	Result.Outcome = ResolveHit(HitsTaken, MaxHP);
	++HitsTaken;

	if (Result.Outcome == EImpactReceiveOutcome::Destroyed)
	{
		Break(Context);
	}
	else
	{
		Result.Damage = 1;
		ApplyDamagedMesh();
	}

	return Result;
}

void UDestructibleComponent::ApplyDamagedMesh()
{
	const int32 Index = GetDamagedMeshIndex(HitsTaken, DamagedMeshes.Num());
	if (VisualMesh && DamagedMeshes.IsValidIndex(Index) && DamagedMeshes[Index])
	{
		VisualMesh->SetStaticMesh(DamagedMeshes[Index]);
	}
}

void UDestructibleComponent::Break(const FImpactReceiveContext& Context)
{
	bBroken = true;
	AActor* Owner = GetOwner();

	// [ロジック] 当たり判定を消して道を開ける。ここで「通れる」がゲーム上確定する。
	if (Owner)
	{
		Owner->SetActorEnableCollision(false);

		// 当たり判定を失った建物は NavMesh の障害物ではなくなる。実行時生成が Dynamic のときに経路が更新される。
		FNavigationSystem::UpdateActorAndComponentData(*Owner);
	}

	// [演出] Chaos の破片。差し替える前の見た目の位置・大きさで出す。
	if (VisualMesh)
	{
		SpawnDebris(VisualMesh->GetComponentTransform(), Context);
	}

	// [演出] 瓦礫へ差し替える。瓦礫が無ければ見た目ごと消す。
	if (VisualMesh && RubbleMesh)
	{
		VisualMesh->SetStaticMesh(RubbleMesh);
		VisualMesh->SetRelativeScale3D(RubbleRelativeScale);
	}
	else if (Owner)
	{
		Owner->SetActorHiddenInGame(true);
	}

	// [演出] 粉塵・火花を突進方向へ向けて出す。
	if (DestructionFX && Owner)
	{
		const FRotator Facing = Context.ImpactVelocity.IsNearlyZero()
			? Owner->GetActorRotation()
			: Context.ImpactVelocity.Rotation();
		if (UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DestructionFX, Context.ImpactPoint, Facing))
		{
			FX->SetVariableVec3(FXVelocityParameter, Context.ImpactVelocity);
		}
	}

	UE_LOG(LogImpactMatch, Log, TEXT("%s broken at impact speed %.0f (route %s)"),
		*GetNameSafe(Owner), Context.ImpactSpeed, *OpenedRouteTag.ToString());

	OnBroken.Broadcast(this, Context.ImpactVelocity);

	if (UFieldSubsystem* Field = UFieldSubsystem::Get(this))
	{
		Field->NotifyObjectBroken(this);
	}
}

void UDestructibleComponent::SpawnDebris(const FTransform& MeshTransform, const FImpactReceiveContext& Context) const
{
	UWorld* World = GetWorld();
	if (!DestructionGC || !FeedbackTuning || !World)
	{
		return;
	}

	// 破片は位置と向きだけで出し、スケールは掛けない。Geometry Collection は見た目と同じ大きさ（拡大済み）の形から作る。
	// 縦横高さで比率の違うスケールを実行時に掛けると、破片の見た目と物理形状がずれるおそれがあるため。
	const FTransform SpawnTransform(MeshTransform.GetRotation(), MeshTransform.GetLocation());

	// 破片の設定（見た目・当たり判定）を登録前に済ませるため、生成を遅延させる。
	AGeometryCollectionActor* Debris = World->SpawnActorDeferred<AGeometryCollectionActor>(
		AGeometryCollectionActor::StaticClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	UGeometryCollectionComponent* Pieces = Debris ? Debris->GetGeometryCollectionComponent() : nullptr;
	if (!Pieces)
	{
		return;
	}

	Pieces->SetRestCollection(DestructionGC);

	// 破片はゲームに関与させない。機体・踏み台（WorldDynamic）・カメラとは当たらず、床や壁の上にだけ落ちる。
	// 破片に引っかかって減速する理不尽と、オンライン化時の破片の同期を避けるため（フィールド設計 3.3）。
	// 別の建物の破片同士（Destructible）の衝突も省く。数秒で消える演出のため、連続破壊時の物理の負荷を抑えることを優先する。
	// 同じ建物の破片同士の衝突は Geometry Collection の内部で扱われるため、この設定の影響を受けない。
	Pieces->SetCollisionObjectType(ECC_Destructible);
	Pieces->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Pieces->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	Pieces->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Pieces->SetCollisionResponseToChannel(ECC_Destructible, ECR_Ignore);

	Debris->FinishSpawning(SpawnTransform);
	Debris->SetLifeSpan(FeedbackTuning->DebrisLifetime);

	// 崩す処理は、破片の物理側の初期化を待ってから行う（FIX-07）。
	// Geometry Collection の物理側の粒子は、登録（FPBDRigidsSolver::RegisterObject の AddDirtyProxy）の後、
	// 次の物理ステップで初めて作られる。生成したフレームで CrumbleActiveClusters を呼ぶと、割る対象がまだ無いため
	// 何も起きず、建物がひと塊のまま出現して、崩れないか床に落ちた衝撃で遅れて崩れていた。
	//
	// N+1: 最上位の塊（建物全体）を必ず割り、大きな塊にする。衝突点からの距離で弱まる歪みでは、
	//      建物の中心にある最上位の塊まで届かないため。
	// N+2: 割れた塊のうち衝突点の近くだけを細かく崩し、突進方向へ飛ばす。機体が通った部分に穴が開き、
	//      外側は大きな塊のまま崩れる（フィールド設計 3.4）。1段目の割れが物理に反映された後に当てる。
	const TWeakObjectPtr<UGeometryCollectionComponent> WeakPieces(Pieces);
	const TWeakObjectPtr<const UFeedbackTuningDataAsset> WeakTuning(FeedbackTuning);
	const FVector ImpactPoint = Context.ImpactPoint;
	const FVector ImpactVelocity = Context.ImpactVelocity;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Debris,
		[Debris, WeakPieces, WeakTuning, ImpactPoint, ImpactVelocity]()
		{
			UGeometryCollectionComponent* PiecesNow = WeakPieces.Get();
			UWorld* WorldNow = PiecesNow ? PiecesNow->GetWorld() : nullptr;
			if (!WorldNow)
			{
				return;
			}

			PiecesNow->CrumbleActiveClusters();

			WorldNow->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Debris,
				[WeakPieces, WeakTuning, ImpactPoint, ImpactVelocity]()
				{
					UGeometryCollectionComponent* PiecesLater = WeakPieces.Get();
					const UFeedbackTuningDataAsset* TuningLater = WeakTuning.Get();
					if (PiecesLater && TuningLater)
					{
						ApplyDebrisImpact(*PiecesLater, *TuningLater, ImpactPoint, ImpactVelocity);
					}
				}));
		}));
}

void UDestructibleComponent::ApplyDebrisImpact(
	UGeometryCollectionComponent& Pieces, const UFeedbackTuningDataAsset& Tuning, const FVector& ImpactPoint, const FVector& ImpactVelocity)
{
	UObject* Outer = Pieces.GetOwner();

	// 衝突点の周りの塊だけに歪みを与えて細かく崩す。
	URadialFalloff* HoleStrain = NewObject<URadialFalloff>(Outer);
	HoleStrain->SetRadialFalloff(Tuning.DebrisHoleStrain, 0.0f, 1.0f, 0.0f,
		Tuning.DebrisHoleRadius, ImpactPoint, EFieldFalloffType::Field_Falloff_Linear);
	Pieces.ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, HoleStrain);

	// 突進方向へ初速を与える。衝突点から離れた破片ほど弱くする。
	const FVector DebrisVelocity = Tuning.ComputeDebrisVelocity(ImpactVelocity);
	if (DebrisVelocity.IsNearlyZero())
	{
		return;
	}

	UUniformVector* Push = NewObject<UUniformVector>(Outer);
	Push->SetUniformVector(DebrisVelocity.Size(), DebrisVelocity.GetSafeNormal());

	URadialFalloff* PushFalloff = NewObject<URadialFalloff>(Outer);
	PushFalloff->SetRadialFalloff(1.0f, 0.0f, 1.0f, 0.0f,
		Tuning.DebrisPushRadius, ImpactPoint, EFieldFalloffType::Field_Falloff_Linear);

	UOperatorField* PushNearImpact = NewObject<UOperatorField>(Outer);
	PushNearImpact->SetOperatorField(1.0f, Push, PushFalloff, EFieldOperationType::Field_Multiply);
	Pieces.ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_LinearVelocity, nullptr, PushNearImpact);
}
