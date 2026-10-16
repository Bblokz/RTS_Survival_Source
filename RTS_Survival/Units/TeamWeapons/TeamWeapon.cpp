// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeapon.h"

#include "TeamWeaponAnimationInstance.h"
#include "TeamWeaponController.h"
#include "TeamWeaponMover.h"
#include "CrewPositions/CrewPosition.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/RTSComponents/VehicleFireFeedbackComponent/AnimMeshFeedbackComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace TeamWeaponGroundStatics
{
	// Short traces keep the weapon from snapping onto a roof or bridge above it; long enough for spawn heights.
	constexpr float MinGroundTraceStartHeightCm = 200.f;
	constexpr float MinGroundTraceLengthCm = 1200.f;
	// Units block the landscape channel as well; the trace steps through this many of them.
	constexpr int32 MaxUnitsToStepThrough = 4;
	// Footprint derived from the mesh bounds: a share of the shorter reach to the front or back and the sides.
	constexpr float FootprintLengthFraction = 0.5f;
	constexpr float FootprintWidthFraction = 0.6f;
	constexpr float MinFootprintHalfLengthCm = 40.f;
	constexpr float MaxFootprintHalfLengthCm = 220.f;
	constexpr float MinFootprintHalfWidthCm = 30.f;
	constexpr float MaxFootprintHalfWidthCm = 130.f;

	bool GetIsUnitBlockingGroundTrace(const AActor* HitActor)
	{
		return IsValid(HitActor) && (HitActor->IsA<APawn>() || HitActor->IsA<ATeamWeapon>());
	}
}

void FTeamWeaponDecalManager::CreateDecal(ATeamWeapon* TeamWeapon)
{
	if (not IsValid(TeamWeapon))
	{
		RTSFunctionLibrary::ReportError("TeamWeapon not valid in FTeamWeaponDecalManager::CreateDecal");
		return;
	}

	if (M_TeamWeaponDecalComponent.Get() != nullptr)
	{
		return;
	}

	UMaterialInterface* DecalMaterial = M_DecalMaterial.Get();
	if (DecalMaterial == nullptr)
	{
		RTSFunctionLibrary::ReportError("M_DecalMaterial not set in FTeamWeaponDecalManager::CreateDecal");
		return;
	}

	USceneComponent* RootComponent = TeamWeapon->GetRootComponent();
	if (not IsValid(RootComponent))
	{
		RTSFunctionLibrary::ReportError("RootComponent not valid in FTeamWeaponDecalManager::CreateDecal");
		return;
	}

	UDecalComponent* DecalComponent = NewObject<UDecalComponent>(TeamWeapon);
	if (not IsValid(DecalComponent))
	{
		RTSFunctionLibrary::ReportError("Failed to create decal component in FTeamWeaponDecalManager::CreateDecal");
		return;
	}

	DecalComponent->DecalSize = M_DecalSize;
	DecalComponent->SetDecalMaterial(DecalMaterial);

	TeamWeapon->AddInstanceComponent(DecalComponent);
	DecalComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	DecalComponent->SetRelativeLocation(M_DecalOffset);
	DecalComponent->RegisterComponent();

	M_TeamWeaponDecalComponent = DecalComponent;
}

void FTeamWeaponDecalManager::DestroyDecal(const ATeamWeapon* TeamWeapon)
{
	if (not IsValid(TeamWeapon))
	{
		RTSFunctionLibrary::ReportError("TeamWeapon not valid in FTeamWeaponDecalManager::DestroyDecal");
		return;
	}

	UDecalComponent* DecalComponent = M_TeamWeaponDecalComponent.Get();
	if (DecalComponent == nullptr)
	{
		M_TeamWeaponDecalComponent.Reset();
		return;
	}

	DecalComponent->DestroyComponent();
	M_TeamWeaponDecalComponent.Reset();
}

ATeamWeapon::ATeamWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	M_OptimizationComponent = CreateDefaultSubobject<URTSOptimizer>(TEXT("RTSOptimizer"));
}

void ATeamWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Always first!
	BeginPlay_InitAnimInstance();
	BeginPlay_InitTeamWeaponCollision();
	BeginPlay_ApplyTeamWeaponConfig(M_TeamWeaponConfig);
	BeginPlay_ForceStartPackedState();
}

void ATeamWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_HealthComponent = FindComponentByClass<UHealthComponent>();
	M_RTSComponent = FindComponentByClass<URTSComponent>();
	M_TeamWeaponMover = FindComponentByClass<UTeamWeaponMover>();
	M_TeamWeaponMesh = FindComponentByClass<USkeletalMeshComponent>();
	M_AnimMeshFeedbackComponent = FindComponentByClass<UAnimMeshFeedbackComponent>();

	(void)GetIsValidHealthComponent();
	(void)GetIsValidRTSComponent();
	(void)GetIsValidTeamWeaponMover();
	(void)GetIsValidTeamWeaponMesh();
	(void)GetIsValidOptimizationComponent();

	if (not GetIsValidAnimMeshFeedbackComponent())
	{
		return;
	}

	if (not GetIsValidOptimizationComponent())
	{
		return;
	}

	M_AnimMeshFeedbackComponent->SetOptimizationComponent(M_OptimizationComponent);
}


void ATeamWeapon::BeginPlay_ApplyTeamWeaponConfig(const FTeamWeaponConfig& NewConfig)
{
	M_TeamWeaponConfig = NewConfig;
	if (GetIsValidHealthComponent())
	{
		M_HealthComponent->InitHealthAndResistance(M_TeamWeaponConfig.M_ResistanceData, M_TeamWeaponConfig.TeamWeaponHp);
	}
	if (GetIsValidTeamWeaponMesh())
	{
		// A yaw arc of 0 means the gun traverses all the way round on its mount (the flak guns); only guns with an arc
		// are turned by their crew once a target leaves it.
		const bool bIsCrewTurnedWithinArc = not FMath::IsNearlyZero(M_TeamWeaponConfig.M_YawArc.M_MaxYaw);
		InitEmbeddedTurret(M_TeamWeaponMesh.Get(),
		                   M_TeamWeaponConfig.M_EmbeddedSettings.TurretHeight,
		                   bIsCrewTurnedWithinArc,
		                   M_TeamWeaponConfig.M_YawArc.M_MinYaw,
		                   M_TeamWeaponConfig.M_YawArc.M_MaxYaw,
		                   this,
		                   M_TeamWeaponConfig.M_EmbeddedSettings.bIsArtillery,
		                   M_TeamWeaponConfig.M_EmbeddedSettings.ArtilleryDistanceUseMaxPitch);
		if (GetIsValidAnimInstance())
		{
			M_AnimInstance->InitTeamWeaponAnimInst(M_TeamWeaponConfig.M_DeploymentTime);
		}
		InitTurretsMaster(M_TeamWeaponMesh.Get());
		InitChildTurret(
			M_TeamWeaponConfig.WeaponYawRotationSpeed,
			M_TeamWeaponConfig.M_PitchSettings.M_MaxPitch,
			M_TeamWeaponConfig.M_PitchSettings.M_MinPitch,
			EIdleRotation::Idle_Animate);
	}
}

void ATeamWeapon::PlayFireAnim() const
{
	if(not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->PlayFireAnimation();
}

void ATeamWeapon::OnTeamWeaponAbandoned()
{
	bM_IsAbandoned = true;
	M_TeamWeaponDecalManager.CreateDecal(this);
	ApplyAbandonedHealthBarCustomization();
	DisableTurret();
}

void ATeamWeapon::OnTeamWeaponRemanned()
{
	bM_IsAbandoned = false;
	RestoreCrewedHealthBarCustomization();
	M_TeamWeaponDecalManager.DestroyDecal(this);
}

bool ATeamWeapon::ForceSetAbandonedStateForCapture(
	TSubclassOf<ATeamWeaponController> DefaultLastTeamWeaponControllerClass)
{
	if (DefaultLastTeamWeaponControllerClass)
	{
		M_LastTeamWeaponControllerClass = DefaultLastTeamWeaponControllerClass;
	}
	else if (not M_LastTeamWeaponControllerClass)
	{
		ATeamWeaponController* TeamWeaponController = GetTeamWeaponController();
		if (IsValid(TeamWeaponController))
		{
			M_LastTeamWeaponControllerClass = TeamWeaponController->GetClass();
		}
	}

	ATeamWeaponController* TeamWeaponController = GetTeamWeaponController();
	if (IsValid(TeamWeaponController))
	{
		TeamWeaponController->OnControlledTeamWeaponDied();
	}
	else
	{
		OnTeamWeaponAbandoned();
		SetTeamWeaponController(nullptr);
	}

	if (not GetIsAbandoned())
	{
		OnTeamWeaponAbandoned();
	}

	if (not M_LastTeamWeaponControllerClass)
	{
		RTSFunctionLibrary::ReportError("Last team weapon controller class is not set after forcing abandoned state."
			"\n At function: ATeamWeapon::ForceSetAbandonedStateForCapture");
		return false;
	}

	if (GetSquadSubtypeFromRTSComponent() == ESquadSubtype::Squad_None)
	{
		RTSFunctionLibrary::ReportError("Team weapon squad subtype is invalid after forcing abandoned state."
			"\n At function: ATeamWeapon::ForceSetAbandonedStateForCapture");
		return false;
	}

	return true;
}

ESquadSubtype ATeamWeapon::GetSquadSubtypeFromRTSComponent() const
{
	if (not GetIsValidRTSComponent())
	{
		return ESquadSubtype::Squad_None;
	}

	return M_RTSComponent->GetSubtypeAsSquadSubtype();
}

bool ATeamWeapon::SetOwningPlayerRuntime(const uint8 NewOwningPlayer)
{
	if (not GetIsValidRTSComponent())
	{
		return false;
	}

	M_RTSComponent->SetOwningPlayerRuntime(NewOwningPlayer);
	BeginPlay_InitTeamWeaponCollision();
	return true;
}

void ATeamWeapon::SetTeamWeaponController(ATeamWeaponController* NewController)
{
	if (IsValid(NewController))
	{
		M_LastTeamWeaponControllerClass = NewController->GetClass();
	}

	M_TeamWeaponController = NewController;
}

void ATeamWeapon::ApplyAbandonedHealthBarCustomization()
{
	if (not GetIsValidHealthComponent())
	{
		return;
	}

	if (not M_AbandonedHealthBarState.bM_HasCachedCrewedHealthBarCustomization)
	{
		M_AbandonedHealthBarState.M_CachedCrewedHealthBarCustomization = M_HealthComponent->GetCustomizationSettings();
		M_AbandonedHealthBarState.bM_HasCachedCrewedHealthBarCustomization = true;
	}

	M_HealthComponent->ChangeCustomizationSettings(M_AbandonedHealthBarState.M_AbandonedHealthBarCustomization);
}

void ATeamWeapon::RestoreCrewedHealthBarCustomization()
{
	if (not GetIsValidHealthComponent())
	{
		return;
	}

	if (not M_AbandonedHealthBarState.bM_HasCachedCrewedHealthBarCustomization)
	{
		return;
	}

	M_HealthComponent->ChangeCustomizationSettings(M_AbandonedHealthBarState.M_CachedCrewedHealthBarCustomization);
	M_AbandonedHealthBarState.bM_HasCachedCrewedHealthBarCustomization = false;
}

void ATeamWeapon::BeginPlay_InitTeamWeaponCollision()
{
	if (not GetIsValidTeamWeaponMesh() || not GetIsValidRTSComponent())
	{
		return;
	}

	const TArray<UMeshComponent*> TeamWeaponMeshes = {M_TeamWeaponMesh.Get()};
	FRTS_CollisionSetup::SetupCollisionForTeamWeaponMeshes(
		TeamWeaponMeshes,
		M_RTSComponent->GetOwningPlayer()
	);
}

void ATeamWeapon::BeginPlay_ForceStartPackedState()
{
	PlayPackingMontage(false);
}

void ATeamWeapon::SetTurretOwnerActor(AActor* NewOwner)
{
	if (not IsValid(NewOwner))
	{
		RTSFunctionLibrary::ReportError("NewOwner not valid in ATeamWeapon::SetTurretOwnerActor");
		return;
	}

	InitTurretOwner(NewOwner);
	InitSelectionDelegatesOfOwner(NewOwner);
	OnSetupTurret(NewOwner);
}

void ATeamWeapon::GetCrewPositions(TArray<UCrewPosition*>& OutCrewPositions) const
{
	OutCrewPositions.Reset();
	GetComponents<UCrewPosition>(OutCrewPositions);
}


void ATeamWeapon::PlayPackingMontage(const bool bWaitForMontage) const
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}

	M_AnimInstance->PlayLegsWheelsSlotMontage(ETeamWeaponMontage::PackMontage, bWaitForMontage);
}

void ATeamWeapon::PlayDeployingMontage(const bool bWaitForMontage) const
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}

	M_AnimInstance->PlayLegsWheelsSlotMontage(ETeamWeaponMontage::DeployMontage, bWaitForMontage);
}

void ATeamWeapon::NotifyMoverMovementState(const bool bIsMoving, const FVector& WorldVelocity)
{
	if (not GetUsesWheelMovementMontage())
	{
		bM_IsMoverMoving = bIsMoving;
		return;
	}

	if (not GetIsValidAnimInstance())
	{
		bM_IsMoverMoving = bIsMoving;
		return;
	}

	if (not bIsMoving)
	{
		bM_IsMoverMoving = false;
		M_AnimInstance->StopMoveLoop();
		return;
	}

	if (WorldVelocity.IsNearlyZero())
	{
		bM_IsMoverMoving = false;
		M_AnimInstance->StopMoveLoop();
		return;
	}

	const FVector CurrentActorForwardVector = GetActorForwardVector();
	const float ForwardDirectionDot = FVector::DotProduct(CurrentActorForwardVector, WorldVelocity.GetSafeNormal());
	const bool bForwardMovement = ForwardDirectionDot >= 0.0f;
	if (bM_IsMoverMoving == bIsMoving && bForwardMovement == bM_LastForwardMovement)
	{
		return;
	}

	bM_IsMoverMoving = bIsMoving;
	bM_LastForwardMovement = bForwardMovement;
	M_AnimInstance->StartMoveLoop(bForwardMovement);
}

bool ATeamWeapon::GetUsesWheelMovementMontage() const
{
	return false;
}

void ATeamWeapon::SetWeaponsEnabledForTeamWeaponState(const bool bEnableWeapons)
{
	if (not bEnableWeapons || not GetIsControllerReadyDeployed())
	{
		DisableTurret();
		return;
	}

	AActor* SpecificEngageTarget = M_SpecificEngageTarget.Get();
	if (SpecificEngageTarget == nullptr)
	{
		M_SpecificEngageTarget.Reset();
		SetAutoEngageTargets(true);
		return;
	}

	if (IsValid(SpecificEngageTarget))
	{
		SetEngageSpecificTarget(SpecificEngageTarget);
		return;
	}

	M_SpecificEngageTarget.Reset();
	SetAutoEngageTargets(true);
}

void ATeamWeapon::SetSpecificEngageTarget(AActor* TargetActor)
{
	M_SpecificEngageTarget = TargetActor;
	if (not GetIsControllerReadyDeployed())
	{
		DisableTurret();
		return;
	}

	if (not IsValid(TargetActor))
	{
		SetAutoEngageTargets(true);
		return;
	}

	SetEngageSpecificTarget(TargetActor);
}

void ATeamWeapon::SetDigInHullRotationLocked(const bool bLocked)
{
	bM_IsHullRotationLocked = bLocked;
}

bool ATeamWeapon::ConformToGround(const float DeltaSeconds, const bool bInstant)
{
	const FVector Location = GetActorLocation();
	const float HeadingYaw = GetActorRotation().Yaw;
	const FQuat HeadingRotation = FRotator(0.f, HeadingYaw, 0.f).Quaternion();
	float CentreGroundZ = 0.f;
	if (not TryGetGroundHeightAt(Location, CentreGroundZ))
	{
		return false;
	}

	FVector NormalInHeadingSpace = FVector::UpVector;
	float PlaneCentreZ = CentreGroundZ;
	const bool bHasGroundPlane = M_GroundConformSettings.bAlignToSlope &&
		TryGetFootprintGroundPlane(Location, HeadingRotation, NormalInHeadingSpace, PlaneCentreZ);
	// The wheels sit near the origin: never let a bump under them push the origin into the ground.
	const float OriginZ = bHasGroundPlane ? FMath::Max(CentreGroundZ, PlaneCentreZ) : CentreGroundZ;
	const FRotator TargetTilt = bHasGroundPlane ? GetClampedGroundTilt(NormalInHeadingSpace) : FRotator::ZeroRotator;
	if (bInstant || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		M_GroundTilt = TargetTilt;
	}
	else
	{
		const float TiltAlpha = 1.f - FMath::Exp(-M_GroundConformSettings.TiltFollowSpeed * DeltaSeconds);
		M_GroundTilt.Pitch = FMath::Lerp(M_GroundTilt.Pitch, TargetTilt.Pitch, TiltAlpha);
		M_GroundTilt.Roll = FMath::Lerp(M_GroundTilt.Roll, TargetTilt.Roll, TiltAlpha);
	}

	// Composed as one rotator so its yaw is exactly the heading; pitch and roll lay the gun on the slope.
	const FRotator GroundedRotation(M_GroundTilt.Pitch, HeadingYaw, M_GroundTilt.Roll);
	SetActorLocationAndRotation(FVector(Location.X, Location.Y, OriginZ), GroundedRotation, false, nullptr,
	                            ETeleportType::TeleportPhysics);
	return true;
}

bool ATeamWeapon::TryGetGroundHeightAt(const FVector& Location, float& OutGroundZ) const
{
	using namespace TeamWeaponGroundStatics;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const float TraceStartHeight = FMath::Max(M_TeamWeaponConfig.M_GroundTraceStartHeightCm, MinGroundTraceStartHeightCm);
	const float TraceLength = FMath::Max(M_TeamWeaponConfig.M_GroundTraceLengthCm, MinGroundTraceLengthCm);
	const FVector TraceStart = Location + FVector::UpVector * TraceStartHeight;
	const FVector TraceEnd = TraceStart - FVector::UpVector * TraceLength;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TeamWeaponGroundTrace), false, this);
	for (int32 Attempt = 0; Attempt <= MaxUnitsToStepThrough; ++Attempt)
	{
		FHitResult GroundHit;
		if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
		{
			return false;
		}
		if (not GetIsUnitBlockingGroundTrace(GroundHit.GetActor()))
		{
			OutGroundZ = GroundHit.ImpactPoint.Z;
			return true;
		}
		QueryParams.AddIgnoredActor(GroundHit.GetActor());
	}
	return false;
}

FVector2D ATeamWeapon::GetFootprintHalfExtent() const
{
	using namespace TeamWeaponGroundStatics;
	FVector2D HalfExtent(M_GroundConformSettings.FootprintHalfLengthCm, M_GroundConformSettings.FootprintHalfWidthCm);
	if (HalfExtent.X > 0.f && HalfExtent.Y > 0.f)
	{
		return HalfExtent;
	}
	// Silent: the mesh is validated at begin play; a missing mesh falls back to the smallest footprint.
	const USkeletalMeshComponent* Mesh = M_TeamWeaponMesh.Get();
	if (not IsValid(Mesh))
	{
		return FVector2D(MinFootprintHalfLengthCm, MinFootprintHalfWidthCm);
	}
	const FBoxSphereBounds LocalBounds = Mesh->CalcBounds(FTransform::Identity);
	const FVector BoundsMin = LocalBounds.Origin - LocalBounds.BoxExtent;
	const FVector BoundsMax = LocalBounds.Origin + LocalBounds.BoxExtent;
	// The barrel reaches far forward; the shorter reach is the part that actually rests on the ground.
	const float ShorterLengthReach = FMath::Min(FMath::Abs(BoundsMax.X), FMath::Abs(BoundsMin.X));
	const float ShorterWidthReach = FMath::Min(FMath::Abs(BoundsMax.Y), FMath::Abs(BoundsMin.Y));
	if (HalfExtent.X <= 0.f)
	{
		HalfExtent.X = FMath::Clamp(ShorterLengthReach * FootprintLengthFraction, MinFootprintHalfLengthCm,
		                            MaxFootprintHalfLengthCm);
	}
	if (HalfExtent.Y <= 0.f)
	{
		HalfExtent.Y = FMath::Clamp(ShorterWidthReach * FootprintWidthFraction, MinFootprintHalfWidthCm,
		                            MaxFootprintHalfWidthCm);
	}
	return HalfExtent;
}

bool ATeamWeapon::TryGetFootprintGroundPlane(const FVector& Location, const FQuat& HeadingRotation,
                                             FVector& OutNormalInHeadingSpace, float& OutPlaneCentreZ) const
{
	const FVector2D HalfExtent = GetFootprintHalfExtent();
	// Corner order: front right, front left, back right, back left (X forward, Y right).
	constexpr int32 CornerCount = 4;
	const FVector2D CornerSigns[CornerCount] = {{1.f, 1.f}, {1.f, -1.f}, {-1.f, 1.f}, {-1.f, -1.f}};
	float CornerHeights[CornerCount];
	for (int32 CornerIndex = 0; CornerIndex < CornerCount; ++CornerIndex)
	{
		const FVector LocalCorner(CornerSigns[CornerIndex].X * HalfExtent.X, CornerSigns[CornerIndex].Y * HalfExtent.Y,
		                          0.f);
		if (not TryGetGroundHeightAt(Location + HeadingRotation.RotateVector(LocalCorner), CornerHeights[CornerIndex]))
		{
			return false;
		}
	}
	const float FrontHeight = (CornerHeights[0] + CornerHeights[1]) * 0.5f;
	const float BackHeight = (CornerHeights[2] + CornerHeights[3]) * 0.5f;
	const float RightHeight = (CornerHeights[0] + CornerHeights[2]) * 0.5f;
	const float LeftHeight = (CornerHeights[1] + CornerHeights[3]) * 0.5f;
	const FVector LengthTangent(2.f * HalfExtent.X, 0.f, FrontHeight - BackHeight);
	const FVector WidthTangent(0.f, 2.f * HalfExtent.Y, RightHeight - LeftHeight);
	OutNormalInHeadingSpace = FVector::CrossProduct(LengthTangent, WidthTangent).GetSafeNormal();
	OutPlaneCentreZ = (FrontHeight + BackHeight) * 0.5f;
	return not OutNormalInHeadingSpace.IsNearlyZero();
}

FRotator ATeamWeapon::GetClampedGroundTilt(const FVector& NormalInHeadingSpace) const
{
	// Forward along the slope inside the heading's vertical plane: no sideways component, so no yaw.
	const FVector SlopeForward = FVector::CrossProduct(FVector::RightVector, NormalInHeadingSpace).GetSafeNormal();
	if (SlopeForward.IsNearlyZero())
	{
		return FRotator::ZeroRotator;
	}
	const FRotator FullTilt = FRotationMatrix::MakeFromXZ(SlopeForward, NormalInHeadingSpace).Rotator();
	const float MaxTiltDegrees = M_GroundConformSettings.MaxTiltDegrees;
	return FRotator(FMath::Clamp(FullTilt.Pitch, -MaxTiltDegrees, MaxTiltDegrees), 0.f,
	                FMath::Clamp(FullTilt.Roll, -MaxTiltDegrees, MaxTiltDegrees));
}

FRotator ATeamWeapon::GetTargetRotation(const FVector& TargetLocation, const FVector& TurretLocation) const
{
	const FRotator TiltedFrameRotation = Super::GetTargetRotation(TargetLocation, TurretLocation);
	const FTransform HeadingTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), GetActorLocation());
	const FVector TargetInHeadingFrame = HeadingTransform.InverseTransformPosition(TargetLocation);
	const FRotator HeadingFrameRotation = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, TargetInHeadingFrame);
	return FRotator(TiltedFrameRotation.Pitch, HeadingFrameRotation.Yaw, TiltedFrameRotation.Roll);
}

void ATeamWeapon::StartTowSway()
{
	M_TowSway.Reset();
	M_GroundTilt = FRotator::ZeroRotator;
}

void ATeamWeapon::TickTowSway(const float DeltaSeconds)
{
	M_TowSway.Tick(M_TowSwaySettings, *this, DeltaSeconds);
}

void ATeamWeapon::EndTowSway()
{
	M_TowSway.Reset();
	M_GroundTilt = FRotator::ZeroRotator;
	SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f), ETeleportType::TeleportPhysics);
}

float ATeamWeapon::TakeDamage(
	float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	(void)EventInstigator;
	(void)DamageCauser;
	if (not IsUnitAlive())
	{
		// Do not trigger death logic for already dead units.
		return 1.0f;
	}

	if (not GetIsValidHealthComponent())
	{
		return 1.0f;
	}

	const ERTSDamageType RTSDamageType = FRTSWeaponHelpers::GetRTSDamageType(DamageEvent);
	if (not M_HealthComponent->TakeDamage(DamageAmount, RTSDamageType))
	{
		return 1.0f;
	}

	const ERTSDeathType DeathType = FRTSWeaponHelpers::TranslateDamageIntoDeathType(RTSDamageType);
	UnitDies(DeathType);
	return 0.0f;
}

float ATeamWeapon::GetCurrentTurretAngle_Implementation() const
{
	if (GetIsValidAnimInstance())
	{
		return M_AnimInstance->GetCurrentYawAngle();
	}
	return 0.0f;
}


void ATeamWeapon::SetTurretAngle_Implementation(float NewAngle)
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->SetYaw(NewAngle);
}

void ATeamWeapon::UpdateTargetPitch_Implementation(float NewPitch)
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->SetPitch(NewPitch);
}

bool ATeamWeapon::TurnBase_Implementation(float Degrees)
{
	if (bM_IsHullRotationLocked)
	{
		return false;
	}

	if (not GetIsValidTeamWeaponController())
	{
		return false;
	}

	const float DesiredYaw = GetActorRotation().Yaw + Degrees;
	const FRotator DesiredRotation = FRotator(0.0f, DesiredYaw, 0.0f);
	return M_TeamWeaponController->RequestInternalRotateTowards(DesiredRotation);
}

void ATeamWeapon::PlaySingleFireAnimation_Implementation(int32 WeaponIndex)
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->PlayFireAnimation();
}

void ATeamWeapon::PlayBurstFireAnimation_Implementation(int32 WeaponIndex)
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->PlayBurstFireAnimation();
}


bool ATeamWeapon::GetIsTargetWithinYawArc(const FVector& TargetLocation) const
{
	const float MaxYaw = M_TeamWeaponConfig.M_YawArc.M_MaxYaw;
	if (FMath::IsNearlyZero(MaxYaw))
	{
		return true;
	}

	const FVector ToTarget = TargetLocation - GetActorLocation();
	const FRotator TargetRotator = ToTarget.Rotation();
	const float LocalYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, TargetRotator.Yaw);
	return LocalYaw >= M_TeamWeaponConfig.M_YawArc.M_MinYaw && LocalYaw <= MaxYaw;
}

void ATeamWeapon::SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter)
{
	M_DefaultQueryFilter = NewDefaultFilter;
}

void ATeamWeapon::GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const
{
	OutLocalOffsets.Reset();
	OutLocalOffsets.Add(FVector::ZeroVector);
}

void ATeamWeapon::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	BP_OnHealthChanged(PercentageLeft, bIsHealing);
}

void ATeamWeapon::UnitDies(const ERTSDeathType DeathType)
{
	if (GetIsValidTeamWeaponController())
	{
		M_TeamWeaponController->OnControlledTeamWeaponDied();
	}

	SetUnitDying();
	OnUnitDies.Broadcast();
	// Assume destroy is called in here in some way.
	BP_OnUnitDies();
}

void ATeamWeapon::DestroyAndSpawnActors(const FDestroySpawnActorsParameters& SpawnParams,
                                          const FCollapseFX CollapseFX)
{
	FRTS_Collapse::OnDestroySpawnActors(this,
	                                    SpawnParams,
	                                    CollapseFX);
}



bool ATeamWeapon::GetIsValidHealthComponent() const
{
	if (IsValid(M_HealthComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_HealthComponent",
	                                                      "ATeamWeapon::GetIsValidHealthComponent", this);
	return false;
}

bool ATeamWeapon::GetIsValidRTSComponent() const
{
	if (IsValid(M_RTSComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_RTSComponent",
	                                                      "ATeamWeapon::GetIsValidRTSComponent", this);
	return false;
}

bool ATeamWeapon::GetIsValidTeamWeaponMover() const
{
	if (IsValid(M_TeamWeaponMover))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponMover",
	                                                      "ATeamWeapon::GetIsValidTeamWeaponMover", this);
	return false;
}

bool ATeamWeapon::GetIsValidOptimizationComponent() const
{
	if (IsValid(M_OptimizationComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_OptimizationComponent",
	                                                      "ATeamWeapon::GetIsValidOptimizationComponent", this);
	return false;
}

bool ATeamWeapon::GetIsValidAnimMeshFeedbackComponent() const
{
	if (M_AnimMeshFeedbackComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_AnimMeshFeedbackComponent",
	                                                      "ATeamWeapon::GetIsValidAnimMeshFeedbackComponent", this);
	return false;
}

bool ATeamWeapon::GetIsValidTeamWeaponMesh() const
{
	if (not M_TeamWeaponMesh.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponMesh",
		                                                      "ATeamWeapon::GetIsValidTeamWeaponMesh", this);
		return false;
	}
	return true;
}

bool ATeamWeapon::GetIsValidTeamWeaponController() const
{
	if (M_TeamWeaponController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponController",
	                                                      "ATeamWeapon::GetIsValidTeamWeaponController", this);
	return false;
}

bool ATeamWeapon::GetIsControllerReadyDeployed() const
{
	if (not GetIsValidTeamWeaponController())
	{
		return false;
	}

	return M_TeamWeaponController->GetIsReadyDeployed();
}

void ATeamWeapon::BeginPlay_InitAnimInstance()
{
	if (not GetIsValidTeamWeaponMesh())
	{
		return;
	}
	UTeamWeaponAnimationInstance* AnimInst = Cast<UTeamWeaponAnimationInstance>(M_TeamWeaponMesh->GetAnimInstance());
	M_AnimInstance = AnimInst;
	(void)GetIsValidAnimInstance();
}

bool ATeamWeapon::GetIsValidAnimInstance() const
{
	if (not M_AnimInstance.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_AnimInstance",
		                                                      "ATeamWeapon::GetIsValidAnimInstance", this);
		return false;
	}
	return true;
}
