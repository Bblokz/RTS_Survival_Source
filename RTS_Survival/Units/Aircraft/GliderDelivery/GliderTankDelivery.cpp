// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GliderTankDelivery.h"

#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "RTS_Survival/Environment/GroundSurface/GroundSurfaceSubsystem.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace GliderTankDeliveryConstants
{
	// The engine cylinder is 100 cm tall and 50 cm in radius.
	constexpr float CableMeshHeight = 100.f;
	constexpr float CableMeshRadius = 50.f;
	constexpr float StopSpeed = 25.f;
	constexpr int32 TankSpawnId = 0;
	constexpr int32 PlayerOwner = 1;
	// User parameters of NS_Track_Smokes, see UChassisAnimInstance.
	const FName SmokeSurfaceLookParameter = TEXT("SmokeSurfaceLook");
	const FName SmokeSpeedParameter = TEXT("XSmokeSpeedMultiplier");
}

AGliderTankDelivery::AGliderTankDelivery()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = M_Root;

	M_GliderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GliderBody"));
	M_GliderBody->SetupAttachment(M_Root);

	M_GliderNose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GliderNose"));
	M_GliderNose->SetupAttachment(M_GliderBody);

	M_TugBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TugBody"));
	M_TugBody->SetupAttachment(M_Root);

	M_TowCable = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowCable"));
	M_TowCable->SetupAttachment(M_Root);

	M_SkidDust = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SkidDust"));
	M_SkidDust->SetupAttachment(M_GliderBody);
	M_SkidDust->SetAutoActivate(false);

	M_FowComponent = CreateDefaultSubobject<UFowComp>(TEXT("FowComponent"));
}

void AGliderTankDelivery::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupNoCollision();
	BeginPlay_CreateTugProps();
	if (GetIsValidGliderNose())
	{
		M_GliderNose->SetRelativeLocation(M_Layout.NoseHingeOffset);
	}
	if (IsValid(M_SkidDust))
	{
		M_SkidDust->SetRelativeLocation(M_Layout.SkidDustOffset);
	}
	PlaceTugOnCable();
}

void AGliderTankDelivery::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LingerTimerHandle);
	}
	if (IsValid(M_FowComponent))
	{
		M_FowComponent->StopFowParticipation();
	}
	if (not M_Run.bM_HasBroadcastDelivered)
	{
		BroadcastDelivered(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

void AGliderTankDelivery::BeginPlay_SetupNoCollision() const
{
	// Pure spectacle: units drive through the glider and nothing changes the navmesh.
	TArray<UPrimitiveComponent*> Primitives;
	GetComponents<UPrimitiveComponent>(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetCanEverAffectNavigation(false);
	}
}

void AGliderTankDelivery::BeginPlay_CreateTugProps()
{
	if (not GetIsValidTugBody() || not IsValid(M_TugPropMesh))
	{
		return;
	}
	for (int32 PropIndex = 0; PropIndex < M_Layout.TugPropOffsets.Num(); ++PropIndex)
	{
		UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("TugProp%d"), PropIndex));
		Prop->SetStaticMesh(M_TugPropMesh);
		Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prop->SetCanEverAffectNavigation(false);
		Prop->SetupAttachment(M_TugBody);
		Prop->SetRelativeLocation(M_Layout.TugPropOffsets[PropIndex]);
		Prop->RegisterComponent();
		M_TugProps.Add(Prop);
	}
}

void AGliderTankDelivery::PlaceTugOnCable() const
{
	if (not GetIsValidTugBody())
	{
		return;
	}
	const FVector HookLocation = M_Layout.TowAttachOffset
		+ FVector(M_Tuning.TowCableLength, 0.f, M_Tuning.TugHeightAboveGlider);
	M_TugBody->SetRelativeLocationAndRotation(HookLocation - M_Layout.TugTowHookOffset, FRotator::ZeroRotator);
}

void AGliderTankDelivery::StartDelivery(const FGliderDeliveryOrder& Order)
{
	M_Order = Order;
	M_Run = FGliderDeliveryRunState();

	FVector Direction = Order.StopLocation - Order.TouchdownLocation;
	Direction.Z = 0.f;
	M_Run.Direction = Direction.GetSafeNormal();
	if (M_Run.Direction.IsNearlyZero())
	{
		RTSFunctionLibrary::ReportError(TEXT("AGliderTankDelivery got a landing run without length."));
		Destroy();
		return;
	}
	M_Run.SkidLength = Direction.Size();
	M_Run.ReleaseLocation = Order.TouchdownLocation - M_Run.Direction * M_Tuning.GlideDistance;
	M_Run.ReleaseLocation.Z = Order.TouchdownLocation.Z + M_Tuning.ReleaseAltitude;

	FVector StartLocation = M_Run.ReleaseLocation - M_Run.Direction * Order.ApproachDistance;
	StartLocation.Z = M_Run.ReleaseLocation.Z;
	SetGliderTransform(StartLocation, 0.f);
	M_Run.CurrentSpeed = M_Tuning.TowSpeed;
	UpdateTowCable();
	StartFowForOwner();
	EnterPhase(EGliderDeliveryPhase::TowedApproach);
	SetActorTickEnabled(true);
}

void AGliderTankDelivery::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	M_Run.PhaseElapsed += DeltaSeconds;
	SpinProps(DeltaSeconds);
	if (M_Run.bM_TugReleased)
	{
		TickReleasedTug(DeltaSeconds);
	}

	switch (M_Run.Phase)
	{
	case EGliderDeliveryPhase::TowedApproach:
		TickTowedApproach(DeltaSeconds);
		break;
	case EGliderDeliveryPhase::Gliding:
		TickGliding(DeltaSeconds);
		break;
	case EGliderDeliveryPhase::Skidding:
		TickSkidding(DeltaSeconds);
		break;
	case EGliderDeliveryPhase::OpeningNose:
		TickOpeningNose(DeltaSeconds);
		break;
	case EGliderDeliveryPhase::Sinking:
		TickSinking(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AGliderTankDelivery::EnterPhase(const EGliderDeliveryPhase NewPhase)
{
	M_Run.Phase = NewPhase;
	M_Run.PhaseElapsed = 0.f;
}

void AGliderTankDelivery::TickTowedApproach(const float DeltaSeconds)
{
	const FVector NewLocation = GetActorLocation() + M_Run.Direction * M_Run.CurrentSpeed * DeltaSeconds;
	SetGliderTransform(NewLocation, 0.f);
	UpdateTowCable();

	const float PastRelease = FVector::DotProduct(NewLocation - M_Run.ReleaseLocation, M_Run.Direction);
	if (PastRelease < 0.f)
	{
		return;
	}
	ReleaseTow();
	EnterPhase(EGliderDeliveryPhase::Gliding);
}

void AGliderTankDelivery::TickGliding(const float DeltaSeconds)
{
	const FVector Touchdown = M_Order.TouchdownLocation;
	const FVector Current = GetActorLocation();
	const float RemainingHorizontal = FVector::DotProduct(Touchdown - Current, M_Run.Direction);
	const float GlideFraction = 1.f - FMath::Clamp(RemainingHorizontal / M_Tuning.GlideDistance, 0.f, 1.f);
	M_Run.CurrentSpeed = FMath::Lerp(M_Tuning.TowSpeed, M_Tuning.TouchdownSpeed, GlideFraction);

	const float Step = M_Run.CurrentSpeed * DeltaSeconds;
	if (RemainingHorizontal <= Step)
	{
		M_Run.SkidDeceleration = FMath::Square(M_Run.CurrentSpeed) / (2.f * FMath::Max(M_Run.SkidLength, 1.f));
		PlaceGliderOnGround(Touchdown);
		StartSkidDust();
		EnterPhase(EGliderDeliveryPhase::Skidding);
		return;
	}

	// Straight glide slope from the release point down to the touchdown point, never below the ground.
	const float SlopeHeight = M_Tuning.ReleaseAltitude * (RemainingHorizontal - Step) / M_Tuning.GlideDistance;
	FVector NewLocation = Current + M_Run.Direction * Step;
	NewLocation.Z = FMath::Max(Touchdown.Z + SlopeHeight, GetGroundHeight(NewLocation, Touchdown.Z));
	const float GlidePitch = -FMath::RadiansToDegrees(FMath::Atan2(M_Tuning.ReleaseAltitude, M_Tuning.GlideDistance));
	SetGliderTransform(NewLocation, GlidePitch);
}

void AGliderTankDelivery::TickSkidding(const float DeltaSeconds)
{
	M_Run.CurrentSpeed = FMath::Max(0.f, M_Run.CurrentSpeed - M_Run.SkidDeceleration * DeltaSeconds);
	M_Run.SkidTravelled = FMath::Min(M_Run.SkidTravelled + M_Run.CurrentSpeed * DeltaSeconds, M_Run.SkidLength);
	PlaceGliderOnGround(M_Order.TouchdownLocation + M_Run.Direction * M_Run.SkidTravelled);

	const bool bHasStopped = M_Run.CurrentSpeed <= GliderTankDeliveryConstants::StopSpeed
		|| M_Run.SkidTravelled >= M_Run.SkidLength;
	if (not bHasStopped)
	{
		return;
	}
	if (IsValid(M_SkidDust))
	{
		M_SkidDust->Deactivate();
	}
	EnterPhase(EGliderDeliveryPhase::OpeningNose);
	RequestTankSpawn();
}

void AGliderTankDelivery::TickOpeningNose(float DeltaSeconds)
{
	const float OpenAlpha = FMath::Clamp(M_Run.PhaseElapsed / M_Tuning.NoseOpenSeconds, 0.f, 1.f);
	if (GetIsValidGliderNose())
	{
		const float Pitch = M_Tuning.NoseOpenPitchDegrees * FMath::SmoothStep(0.f, 1.f, OpenAlpha);
		M_GliderNose->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
	}
	if (OpenAlpha < 1.f)
	{
		return;
	}
	ReleaseTankIfReady();
}

void AGliderTankDelivery::TickSinking(float DeltaSeconds)
{
	const float SinkAlpha = FMath::Clamp(M_Run.PhaseElapsed / M_Tuning.SinkSeconds, 0.f, 1.f);
	SetActorLocation(M_Run.SinkStartLocation - FVector(0.f, 0.f, M_Tuning.SinkDepth * SinkAlpha));
	if (SinkAlpha >= 1.f)
	{
		Destroy();
	}
}

void AGliderTankDelivery::TickReleasedTug(const float DeltaSeconds)
{
	if (not GetIsValidTugBody())
	{
		return;
	}
	M_Run.TugElapsedSinceRelease += DeltaSeconds;
	if (M_Run.TugElapsedSinceRelease >= M_Tuning.TugLifetimeAfterRelease)
	{
		M_TugBody->SetVisibility(false, true);
		return;
	}

	// The tug climbs away and turns off to the right so it clears the glider's path.
	const float TurnStep = M_Tuning.TugTurnRateDegrees * DeltaSeconds;
	M_Run.TugYawTurned += TurnStep;
	M_Run.TugVelocity = M_Run.TugVelocity.RotateAngleAxis(TurnStep, FVector::UpVector);
	M_Run.TugVelocity.Z = M_Tuning.TugClimbRate;
	const float BankAlpha = FMath::Clamp(M_Run.TugElapsedSinceRelease, 0.f, 1.f);
	const FRotator TugRotation(0.f, M_Run.TugVelocity.Rotation().Yaw, M_Tuning.TugBankDegrees * BankAlpha);
	M_TugBody->SetWorldLocationAndRotation(M_TugBody->GetComponentLocation() + M_Run.TugVelocity * DeltaSeconds,
	                                       TugRotation);
}

void AGliderTankDelivery::SpinProps(const float DeltaSeconds) const
{
	if (M_Run.bM_TugReleased && M_Run.TugElapsedSinceRelease >= M_Tuning.TugLifetimeAfterRelease)
	{
		return;
	}
	const FRotator Spin(0.f, 0.f, M_Tuning.PropDegreesPerSecond * DeltaSeconds);
	for (UStaticMeshComponent* Prop : M_TugProps)
	{
		if (IsValid(Prop))
		{
			Prop->AddLocalRotation(Spin);
		}
	}
}

void AGliderTankDelivery::UpdateTowCable() const
{
	if (not GetIsValidTowCable())
	{
		return;
	}
	const FVector CableStart = GetTowAttachWorldLocation();
	const FVector CableEnd = GetTugHookWorldLocation();
	const FVector Cable = CableEnd - CableStart;
	const float RadiusScale = M_Layout.CableRadius / GliderTankDeliveryConstants::CableMeshRadius;
	M_TowCable->SetWorldLocationAndRotation((CableStart + CableEnd) * 0.5f,
	                                       FRotationMatrix::MakeFromZ(Cable.GetSafeNormal()).Rotator());
	M_TowCable->SetWorldScale3D(FVector(RadiusScale, RadiusScale,
	                                    Cable.Size() / GliderTankDeliveryConstants::CableMeshHeight));
}

void AGliderTankDelivery::ReleaseTow()
{
	M_Run.bM_TugReleased = true;
	if (GetIsValidTowCable())
	{
		M_TowCable->SetVisibility(false);
	}
	if (not GetIsValidTugBody())
	{
		return;
	}
	M_TugBody->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	M_Run.TugVelocity = M_Run.Direction * M_Tuning.TowSpeed;
}

void AGliderTankDelivery::StartFowForOwner() const
{
	if (not IsValid(M_FowComponent))
	{
		return;
	}
	// The owner sees around the glider; for the other side it is hidden in the fog like an enemy unit.
	const EFowBehaviour FowBehaviour = M_Order.OwningPlayer == GliderTankDeliveryConstants::PlayerOwner
		                                   ? EFowBehaviour::Fow_Active
		                                   : EFowBehaviour::Fow_PassiveEnemyVision;
	M_FowComponent->SetVisionRadius(M_Tuning.VisionRadius);
	M_FowComponent->StartFow(FowBehaviour, true);
}

void AGliderTankDelivery::StartSkidDust() const
{
	if (not IsValid(M_SkidDust))
	{
		return;
	}
	// The track smoke has no tint of its own; it takes the look of the ground like a tank's tracks do.
	if (const UGroundSurfaceSubsystem* GroundSurface = GetWorld()->GetSubsystem<UGroundSurfaceSubsystem>())
	{
		const FGroundSurfaceSmokeLook& Look = GroundSurface->GetSmokeLookAt(M_Order.TouchdownLocation);
		M_SkidDust->SetVariableVec4(GliderTankDeliveryConstants::SmokeSurfaceLookParameter,
		                            FVector4(Look.SmokeTint.R, Look.SmokeTint.G, Look.SmokeTint.B, Look.SmokeOpacity));
	}
	M_SkidDust->SetVariableFloat(GliderTankDeliveryConstants::SmokeSpeedParameter, M_Tuning.SkidDustSpeedMultiplier);
	M_SkidDust->Activate(true);
}

void AGliderTankDelivery::PlaceGliderOnGround(const FVector& RunLocation)
{
	const float CentreHeight = GetGroundHeight(RunLocation, RunLocation.Z);
	const FVector Ahead = RunLocation + M_Run.Direction * M_Tuning.GroundSampleDistance;
	const FVector Behind = RunLocation - M_Run.Direction * M_Tuning.GroundSampleDistance;
	const float Rise = GetGroundHeight(Ahead, CentreHeight) - GetGroundHeight(Behind, CentreHeight);
	const float SlopePitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, 2.f * M_Tuning.GroundSampleDistance));
	SetGliderTransform(FVector(RunLocation.X, RunLocation.Y, CentreHeight), SlopePitch + M_Tuning.SkidPitchDegrees);
}

float AGliderTankDelivery::GetGroundHeight(const FVector& Location, const float Fallback) const
{
	FVector GroundLocation;
	if (GlobalAbilityHelpers::TryFindGroundLocation(this, Location, GroundLocation))
	{
		return GroundLocation.Z;
	}
	return Fallback;
}

void AGliderTankDelivery::SetGliderTransform(const FVector& Location, const float PitchDegrees)
{
	SetActorLocationAndRotation(Location, FRotator(PitchDegrees, M_Run.Direction.Rotation().Yaw, 0.f));
}

void AGliderTankDelivery::RequestTankSpawn()
{
	if (M_Run.bM_TankRequested)
	{
		return;
	}
	M_Run.bM_TankRequested = true;
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError(TEXT("AGliderTankDelivery found no ARTSAsyncSpawner for its tank."));
		BroadcastDelivered(nullptr);
		StartLinger();
		return;
	}

	const FVector SpawnLocation = GetActorTransform().TransformPosition(M_Layout.TankSpawnOffset);
	const FRotator SpawnRotation(0.f, M_Run.Direction.Rotation().Yaw, 0.f);
	TWeakObjectPtr<AGliderTankDelivery> WeakThis(this);
	const auto OnSpawned = [WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnTankSpawned(SpawnedActor);
		}
	};

	if (M_Order.TankClassOverride.IsNull())
	{
		AsyncSpawner->AsyncSpawnOptionAtLocation(M_Order.TankOption, SpawnLocation, this,
		                                         GliderTankDeliveryConstants::TankSpawnId, OnSpawned, SpawnRotation);
		return;
	}
	AsyncSpawner->AsyncSpawnClassAtLocation(TSoftClassPtr<AActor>(M_Order.TankClassOverride.ToSoftObjectPath()),
	                                        M_Order.TankOption, SpawnLocation, this,
	                                        GliderTankDeliveryConstants::TankSpawnId, OnSpawned, SpawnRotation);
}

void AGliderTankDelivery::OnTankSpawned(AActor* SpawnedActor)
{
	ATankMaster* Tank = Cast<ATankMaster>(SpawnedActor);
	if (not IsValid(Tank))
	{
		RTSFunctionLibrary::ReportError(TEXT("AGliderTankDelivery: the tank to deliver failed to spawn."));
		if (IsValid(SpawnedActor))
		{
			SpawnedActor->Destroy();
		}
		BroadcastDelivered(nullptr);
		StartLinger();
		return;
	}
	M_DeliveredTank = Tank;
	PrepareTankInsideHull(Tank);
	if (M_Run.Phase == EGliderDeliveryPhase::OpeningNose && M_Run.PhaseElapsed >= M_Tuning.NoseOpenSeconds)
	{
		ReleaseTankIfReady();
	}
}

void AGliderTankDelivery::PrepareTankInsideHull(ATankMaster* Tank) const
{
	// Same parking as an aircraft-carried tank: not selectable, guns quiet, until the nose is open.
	if (USelectionComponent* TankSelectionComponent = Tank->GetSelectionComponent())
	{
		TankSelectionComponent->SetCanBeSelected(false);
	}
	Tank->SetTurretsDisabled();
	Tank->SetAudioCompsDisabled(true);
}

void AGliderTankDelivery::ReleaseTankIfReady()
{
	if (M_Run.bM_TankReleased || not M_DeliveredTank.IsValid())
	{
		return;
	}
	M_Run.bM_TankReleased = true;
	ATankMaster* Tank = M_DeliveredTank.Get();
	Tank->SetAudioCompsDisabled(false);
	Tank->EnableWeaponsAfterAircraftDrop();
	if (USelectionComponent* TankSelectionComponent = Tank->GetSelectionComponent())
	{
		TankSelectionComponent->SetCanBeSelected(true);
	}

	const FVector ExitLocation = GetActorLocation() + M_Run.Direction * M_Tuning.TankExitDistance;
	Tank->MoveToLocation(ExitLocation, true, FRotator(0.f, M_Run.Direction.Rotation().Yaw, 0.f));
	BroadcastDelivered(Tank);
	StartLinger();
}

void AGliderTankDelivery::BroadcastDelivered(ATankMaster* Tank)
{
	if (M_Run.bM_HasBroadcastDelivered)
	{
		return;
	}
	M_Run.bM_HasBroadcastDelivered = true;
	OnTankDelivered.Broadcast(this, Tank);
}

void AGliderTankDelivery::StartLinger()
{
	EnterPhase(EGliderDeliveryPhase::Lingering);
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AGliderTankDelivery> WeakThis(this);
	World->GetTimerManager().SetTimer(M_LingerTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->M_Run.SinkStartLocation = WeakThis->GetActorLocation();
		WeakThis->EnterPhase(EGliderDeliveryPhase::Sinking);
	}), FMath::Max(M_Tuning.LingerSeconds, KINDA_SMALL_NUMBER), false);
}

FVector AGliderTankDelivery::GetTugHookWorldLocation() const
{
	if (not GetIsValidTugBody())
	{
		return GetActorLocation();
	}
	return M_TugBody->GetComponentTransform().TransformPosition(M_Layout.TugTowHookOffset);
}

FVector AGliderTankDelivery::GetTowAttachWorldLocation() const
{
	return GetActorTransform().TransformPosition(M_Layout.TowAttachOffset);
}

bool AGliderTankDelivery::GetIsValidGliderBody() const
{
	if (IsValid(M_GliderBody))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_GliderBody", "GetIsValidGliderBody", this);
	return false;
}

bool AGliderTankDelivery::GetIsValidGliderNose() const
{
	if (IsValid(M_GliderNose))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_GliderNose", "GetIsValidGliderNose", this);
	return false;
}

bool AGliderTankDelivery::GetIsValidTugBody() const
{
	if (IsValid(M_TugBody))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TugBody", "GetIsValidTugBody", this);
	return false;
}

bool AGliderTankDelivery::GetIsValidTowCable() const
{
	if (IsValid(M_TowCable))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TowCable", "GetIsValidTowCable", this);
	return false;
}
