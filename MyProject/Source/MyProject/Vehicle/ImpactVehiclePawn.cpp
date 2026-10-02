// 踏みつけ加速メカゲーム — プレイヤー機体 Pawn

#include "Vehicle/ImpactVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "Collision/Stompable.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnhancedInputComponent.h"
#include "Field/TeamComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "UI/HUD/TreadHUDDataComponent.h"
#include "Vehicle/ImpactLockOnComponent.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"
#include "Vehicle/ImpactVehicleTypes.h"

DEFINE_LOG_CATEGORY(LogImpactVehicle);

namespace
{
	/**
	 * 壁・床との当たり判定の既定の大きさ（半径、uu）。プレースホルダの Cube（100 uu 立方）に合わせる。
	 * 実際の大きさは BeginPlay で本体のモデル（BodyMesh）の境界に合わせる。
	 *
	 * ここに既定値が要るのは、スポーン時の干渉退避（ESpawnActorCollisionHandlingMethod）が
	 * Blueprint 既定値の形状で行われ、BeginPlay より前に済んでしまうため（CLAUDE.md §5-8）。
	 * 既定値とモデルの大きさがずれていないかは ApplyCollisionFromModel が起動時に検証する。
	 */
	constexpr float DefaultCollisionExtent = 50.0f;

	/** 当たり判定の大きさの一致を判定する許容誤差（uu）。 */
	constexpr float CollisionExtentTolerance = 0.5f;

	/** バブルの材質が持つ色のベクトルパラメータ名。 */
	const FName BubbleColorParameterName(TEXT("BubbleColor"));

	/** バブルの材質が持つ不透明度のスカラパラメータ名。 */
	const FName BubbleOpacityParameterName(TEXT("Opacity"));

	/** バブルの材質が持つ発光の強さのスカラパラメータ名。 */
	const FName BubbleEmissiveParameterName(TEXT("EmissiveStrength"));
}

AImpactVehiclePawn::AImpactVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 壁・床との当たり判定は本体のモデルを基準にする。
	// 機体同士の接触に使うバブルの半径（BubbleRadius）と正面の実効半径は、移動コンポーネントが論理的に判定し、
	// 壁や床を押し出さない。
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(DefaultCollisionExtent));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_Pawn);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionBox->SetSimulatePhysics(false);
	// 踏み台は機体を Block せず重なるため、踏みつけ検出には重なりイベントが必要。
	CollisionBox->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionBox);

	// 見た目は当たり判定を持たせず、衝突解決はルートの CollisionBox に一本化する。
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionBox);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// バブルの見た目。大きさは BeginPlay で機体同士の接触半径（BubbleRadius）に合わせる。
	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	BubbleMesh->SetupAttachment(CollisionBox);
	BubbleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BubbleMesh->SetCastShadow(false);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionBox);
	SpringArm->TargetArmLength = 700.0f;
	SpringArm->bUsePawnControlRotation = false;
	// 追従の遅れの速さと基準俯角は DA_VehicleTuning の値を BeginPlay で適用する（ApplyCameraTuning）。
	SpringArm->bEnableCameraLag = true;
	SpringArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->FieldOfView = 90.0f;

	VehicleMovement = CreateDefaultSubobject<UImpactVehicleMovementComponent>(TEXT("VehicleMovement"));
	VehicleMovement->UpdatedComponent = CollisionBox;

	// 捕捉とアシストの判断は移動より先に行う（順序は LockOn の BeginPlay で前提条件として設定する）。
	LockOn = CreateDefaultSubobject<UImpactLockOnComponent>(TEXT("LockOn"));

	// 既定はプレイヤーの陣営。NPC は派生クラスで、プレイヤーの機体は出現時に GameMode が改めて割り当てる。
	Team = CreateDefaultSubobject<UTeamComponent>(TEXT("Team"));
	Team->SetTeamId(ImpactTeam::Player);

	HUDData = CreateDefaultSubobject<UTreadHUDDataComponent>(TEXT("HUDData"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void AImpactVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	// 調整値が未割り当てだと機体は動かない。エラーなしで症状だけ出る問題にしないよう、ここで知らせる。
	if (!Tuning || !ImpactTuning)
	{
		UE_LOG(LogImpactVehicle, Error, TEXT("%s: Tuning (DA_VehicleTuning) = %s, ImpactTuning (DA_ImpactTuning) = %s; the vehicle will not move"),
			*GetName(), *GetNameSafe(Tuning.Get()), *GetNameSafe(ImpactTuning.Get()));
	}
	else
	{
		UE_LOG(LogImpactVehicle, Log, TEXT("%s: tuning %s, rules %s"), *GetName(), *Tuning->GetName(), *ImpactTuning->GetName());
	}

	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AImpactVehiclePawn::OnBodyBeginOverlap);

	ApplyCollisionFromModel();
	ApplyBubbleSize();
	ApplyBubbleVisual();
	ApplyCameraTuning();
}

void AImpactVehiclePawn::ApplyCollisionFromModel()
{
	const UStaticMesh* Model = BodyMesh->GetStaticMesh();
	if (!Model)
	{
		UE_LOG(LogImpactVehicle, Warning, TEXT("%s: body mesh is not assigned; wall and floor collision keeps its default size"), *GetName());
		return;
	}

	// 壁・床との当たり判定はモデルの境界そのものにする。モデルを差し替えても当たり判定が追従する。
	const FBoxSphereBounds ModelBounds = Model->GetBounds();
	const FVector ModelScale = BodyMesh->GetRelativeScale3D().GetAbs();
	const FVector ModelExtent = ModelBounds.BoxExtent * ModelScale;

	// モデルの原点が中心からずれていても、見た目の中心を当たり判定の中心にそろえる（足元が原点のモデルなど）。
	BodyMesh->SetRelativeLocation(-ModelBounds.Origin * ModelScale);

	// スポーン時の干渉退避は Blueprint 既定値の形状で行われ、BeginPlay より前に済んでいる。
	// ずれていると出現位置だけが別の大きさで決まるため、黙って進めず知らせる。
	const AImpactVehiclePawn* DefaultVehicle = GetDefault<AImpactVehiclePawn>(GetClass());
	const FVector DefaultExtent = DefaultVehicle ? DefaultVehicle->CollisionBox->GetUnscaledBoxExtent() : FVector::ZeroVector;
	if (!DefaultExtent.Equals(ModelExtent, CollisionExtentTolerance))
	{
		UE_LOG(LogImpactVehicle, Warning,
			TEXT("%s: blueprint collision extent %s differs from the body model %s; spawn displacement uses the blueprint value (run Scripts/Setup_PROTO02Assets.py)"),
			*GetName(), *DefaultExtent.ToCompactString(), *ModelExtent.ToCompactString());
	}

	CollisionBox->SetBoxExtent(ModelExtent);
}

void AImpactVehiclePawn::ApplyBubbleSize()
{
	if (!Tuning)
	{
		return;
	}

	// バブルは機体同士の接触半径を見せるだけで、当たり判定は持たない。
	// 見た目はメッシュ本来の大きさから倍率を求める。メッシュを差し替えても接触半径と一致させるため。
	const UStaticMesh* Mesh = BubbleMesh->GetStaticMesh();
	const float MeshRadius = Mesh ? Mesh->GetBounds().BoxExtent.GetMax() : 0.0f;
	if (MeshRadius > KINDA_SMALL_NUMBER)
	{
		BubbleMesh->SetRelativeScale3D(FVector(Tuning->BubbleRadius / MeshRadius));
	}
	else
	{
		UE_LOG(LogImpactVehicle, Warning, TEXT("%s: bubble mesh is not assigned; the bubble will be invisible"), *GetName());
	}
}

void AImpactVehiclePawn::ApplyBubbleVisual()
{
	if (!BubbleMesh->GetStaticMesh())
	{
		return;
	}

	BubbleMaterial = BubbleMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (!BubbleMaterial)
	{
		UE_LOG(LogImpactVehicle, Warning, TEXT("%s: could not create the bubble material"), *GetName());
		return;
	}

	BubbleMaterial->SetVectorParameterValue(BubbleColorParameterName, BubbleColor);
	BubbleMaterial->SetScalarParameterValue(BubbleOpacityParameterName, BubbleOpacity);
	BubbleMaterial->SetScalarParameterValue(BubbleEmissiveParameterName, BubbleEmissive);
	bOverdriveVisualActive = false;

	// 存在しないパラメータ名を指定しても Set*ParameterValue は黙って何もしない。
	// 「色が変わらない」という分かりにくい形でしか現れないため、読み直して検証する。
	FLinearColor Applied;
	if (!BubbleMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(BubbleColorParameterName), Applied))
	{
		UE_LOG(LogImpactVehicle, Warning, TEXT("%s: bubble material has no '%s' parameter; the bubble color is not applied"),
			*GetName(), *BubbleColorParameterName.ToString());
	}
}

void AImpactVehiclePawn::ApplyCameraTuning()
{
	if (!Tuning)
	{
		return;
	}

	SpringArm->CameraLagSpeed = Tuning->CameraLagSpeed;
	UpdateCameraRotation();
}

void AImpactVehiclePawn::UpdateCameraRotation()
{
	if (!Tuning)
	{
		return;
	}

	// 視点操作の回転に、ロックオンの寄せを足した向きへアームを向ける。
	SpringArm->SetRelativeRotation(FRotator(
		Tuning->CameraBasePitch + CameraArmOffset.Pitch,
		CameraArmOffset.Yaw + LockOnCameraYaw,
		0.0f));
}

void AImpactVehiclePawn::UpdateLockOnCamera(float DeltaSeconds)
{
	if (!ImpactTuning || !LockOn)
	{
		return;
	}

	// 捕捉が外れたら 0 へ戻す。急に振り戻さないよう、寄せるときと同じ速さで補間する。
	const float TargetYaw = LockOn->GetTarget() ? LockOn->GetCameraYawOffset() : 0.0f;
	LockOnCameraYaw = FMath::FInterpTo(LockOnCameraYaw, TargetYaw, DeltaSeconds, ImpactTuning->LockOnCameraInterpSpeed);

	UpdateCameraRotation();
}

void AImpactVehiclePawn::HandleDefeatedInClash_Implementation()
{
	// 自機は即撃破にせず行動不能で済ませる。試行回数を稼ぐことを優先する暫定判断。
	VehicleMovement->ForceCrash();
}

void AImpactVehiclePawn::UpdateOverdriveVisual()
{
	if (!BubbleMaterial || !ImpactTuning)
	{
		return;
	}

	// 超加速は「相手を破壊できる状態」であり、駆け引きの前提になる情報なので両者に見せる（仕様 5章）。
	const bool bOverdrive = ImpactTuning->IsOverdrive(GetCurrentSpeed());
	if (bOverdrive == bOverdriveVisualActive)
	{
		return;
	}
	bOverdriveVisualActive = bOverdrive;

	BubbleMaterial->SetVectorParameterValue(BubbleColorParameterName, bOverdrive ? BubbleOverdriveColor : BubbleColor);
	BubbleMaterial->SetScalarParameterValue(
		BubbleEmissiveParameterName, bOverdrive ? BubbleOverdriveEmissive : BubbleEmissive);
}

void AImpactVehiclePawn::SetControlsLocked(bool bLocked)
{
	bControlsLocked = bLocked;

	if (bLocked)
	{
		LastTurnInput = 0.0f;
		VehicleMovement->SetMoveInput(FVector2D::ZeroVector);
		VehicleMovement->SetBrakeInput(false);
	}
}

void AImpactVehiclePawn::OnBodyBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// 相手の具体的な型は問わず、踏みつけの対象（IStompable）であれば踏みつけを要求する。
	if (OtherActor && OtherActor->Implements<UStompable>())
	{
		// 踏みつけは Pawn が重なりを検出して要求するため、成立の通知も要求元の Pawn から行う。
		// デバッグ操作（Q）による加速は踏みつけではないので、ここを通らず通知されない。
		if (VehicleMovement->TryStompTarget(OtherActor))
		{
			VehicleMovement->OnGameplayEvent.Broadcast(EVehicleGameplayEvent::Stomp, 1);
		}
	}
}

void AImpactVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCameraForSpeed(GetCurrentSpeed());
	UpdateLockOnCamera(DeltaSeconds);
	UpdateOverdriveVisual();
	UpdateBodyRoll(DeltaSeconds);
}

void AImpactVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	// 入力の解除は必ず Completed で行う。Tick 末尾でクリアする方式は
	// 入力処理とコンポーネント Tick の順序に依存するため採用しない。
	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AImpactVehiclePawn::OnMoveInput);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &AImpactVehiclePawn::OnMoveReleased);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AImpactVehiclePawn::OnMoveReleased);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AImpactVehiclePawn::OnLookInput);
	}

	if (BrakeAction)
	{
		EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &AImpactVehiclePawn::OnBrakeInput);
		EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Completed, this, &AImpactVehiclePawn::OnBrakeReleased);
		EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Canceled, this, &AImpactVehiclePawn::OnBrakeReleased);
	}

#if !UE_BUILD_SHIPPING
	// デバッグ操作は押した瞬間に1回だけ反応させる。Triggered だと押している間毎フレーム発火する。
	if (DebugBoostAction)
	{
		EnhancedInput->BindAction(DebugBoostAction, ETriggerEvent::Started, this, &AImpactVehiclePawn::OnDebugBoost);
	}

	if (DebugToggleHoldAction)
	{
		EnhancedInput->BindAction(DebugToggleHoldAction, ETriggerEvent::Started, this, &AImpactVehiclePawn::OnDebugToggleHold);
	}
#endif
}

void AImpactVehiclePawn::OnDebugBoost(const FInputActionValue& Value)
{
	if (bControlsLocked)
	{
		return;
	}

	VehicleMovement->DebugApplyStompBoost();
}

void AImpactVehiclePawn::OnDebugToggleHold(const FInputActionValue& Value)
{
	if (bControlsLocked)
	{
		return;
	}

	VehicleMovement->ToggleDebugHoldBoost();
}

void AImpactVehiclePawn::OnMoveInput(const FInputActionValue& Value)
{
	if (bControlsLocked)
	{
		return;
	}

	const FVector2D MoveValue = Value.Get<FVector2D>();
	LastTurnInput = MoveValue.X;
	VehicleMovement->SetMoveInput(MoveValue);
}

void AImpactVehiclePawn::OnMoveReleased(const FInputActionValue& Value)
{
	LastTurnInput = 0.0f;
	VehicleMovement->SetMoveInput(FVector2D::ZeroVector);
}

void AImpactVehiclePawn::OnBrakeInput(const FInputActionValue& Value)
{
	if (bControlsLocked)
	{
		return;
	}

	VehicleMovement->SetBrakeInput(Value.Get<bool>());
}

void AImpactVehiclePawn::OnBrakeReleased(const FInputActionValue& Value)
{
	VehicleMovement->SetBrakeInput(false);
}

void AImpactVehiclePawn::OnLookInput(const FInputActionValue& Value)
{
	if (!Tuning)
	{
		return;
	}

	const FVector2D LookValue = Value.Get<FVector2D>();

	CameraArmOffset.Yaw += LookValue.X * Tuning->LookSensitivity;
	CameraArmOffset.Pitch = FMath::Clamp(
		CameraArmOffset.Pitch - LookValue.Y * Tuning->LookSensitivity,
		Tuning->CameraPitchMin - Tuning->CameraBasePitch,
		Tuning->CameraPitchMax - Tuning->CameraBasePitch);

	UpdateCameraRotation();
}

float AImpactVehiclePawn::GetCurrentSpeed() const
{
	return VehicleMovement->GetCurrentSpeed();
}

void AImpactVehiclePawn::UpdateCameraForSpeed(float Speed)
{
	if (!Tuning)
	{
		return;
	}

	SpringArm->TargetArmLength = Tuning->GetCameraArmLengthForSpeed(Speed);
	Camera->SetFieldOfView(Tuning->GetCameraFovForSpeed(Speed));
}

void AImpactVehiclePawn::UpdateBodyRoll(float DeltaSeconds)
{
	if (!Tuning)
	{
		return;
	}

	// ブレーキターン中のみ傾ける。旋回方向に対して外側へ振ることで、
	// 側面を晒している状態であることをプレイヤーに示す。
	const bool bBrakeTurning = VehicleMovement->GetDriveState() == EVehicleDriveState::BrakeTurning;
	const float TargetRoll = bBrakeTurning ? LastTurnInput * Tuning->BrakeTurnRollAngle : 0.0f;

	CurrentBodyRoll = FMath::FInterpConstantTo(
		CurrentBodyRoll, TargetRoll, DeltaSeconds, Tuning->BodyRollInterpSpeed);

	BodyMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, CurrentBodyRoll));
}
