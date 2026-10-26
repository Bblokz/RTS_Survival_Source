// Copyright (C) Bas Blokzijl - All rights reserved.


#include "Wanderer.h"

#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Components/StaticMeshComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

namespace WandererConstants
{
	// Height above / below the actor the ground trace covers.
	constexpr float GroundTraceUp = 300.f;
	constexpr float GroundTraceDown = 1000.f;

	// Path points closer than this count as reached.
	constexpr float AcceptanceRadius = 40.f;

	// Beyond this yaw error the wanderer mostly turns on the spot instead of walking forward.
	constexpr float SharpTurnDegrees = 50.f;
	constexpr float SharpTurnSpeedFactor = 0.25f;

	// A walk may take this many times its straight-line duration before it is considered stuck.
	constexpr float WalkTimeoutFactor = 2.5f;
	constexpr float MinWalkTimeout = 3.f;

	constexpr int32 DestinationAttempts = 6;

	// Aim points relative to the mesh bounds: centre of the body plus front and rear.
	constexpr float AimHeightFraction = 0.6f;
	constexpr float AimLengthFraction = 0.3f;
}

float FWandererVatClip::GetDuration(const float FramesPerSecond) const
{
	return FramesPerSecond > 0.f ? NumFrames / FramesPerSecond : 0.f;
}

AWanderer::AWanderer(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	AnimalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnimalMesh"));
	RootComponent = AnimalMesh;
	AnimalMesh->SetMobility(EComponentMobility::Movable);
	AnimalMesh->SetCanEverAffectNavigation(false);

	Health = 60.f;
}

void AWanderer::BeginPlay()
{
	Super::BeginPlay();
	M_WanderOrigin = GetActorLocation();
	BeginPlay_SetupCollision();
	BeginPlay_SetupAimOffsets();
	BeginPlay_ApplyRandomMaterialVariant();

	FVector Location = GetActorLocation();
	Location.Z = GetGroundHeight(Location);
	SetActorLocation(Location);
	StartIdle(true);
}

void AWanderer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_IdleTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AWanderer::BeginPlay_SetupCollision()
{
	if (not GetIsValidAnimalMesh())
	{
		return;
	}
	FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision(AnimalMesh, DeathSettings.bDieWhenCrushedByTanks);
	if (DeathSettings.bDieWhenCrushedByTanks)
	{
		SetupCrushDestructionOverlap(AnimalMesh, ECrushDestructionType::AnyTank, ECrushDeathType::RegularDeath);
	}
}

void AWanderer::BeginPlay_SetupAimOffsets()
{
	if (not GetIsValidAnimalMesh() || not AnimalMesh->GetStaticMesh())
	{
		return;
	}
	const FBox LocalBounds = AnimalMesh->GetStaticMesh()->GetBoundingBox();
	const float AimHeight = LocalBounds.Max.Z * WandererConstants::AimHeightFraction;
	const float AimForward = LocalBounds.GetExtent().X * WandererConstants::AimLengthFraction;
	AimOffsetPoints = {
		FVector(0.f, 0.f, AimHeight),
		FVector(AimForward, 0.f, AimHeight),
		FVector(-AimForward, 0.f, AimHeight)
	};
}

void AWanderer::BeginPlay_ApplyRandomMaterialVariant() const
{
	if (MaterialVariants.IsEmpty() || not GetIsValidAnimalMesh())
	{
		return;
	}
	UMaterialInterface* const Variant = MaterialVariants[FMath::RandRange(0, MaterialVariants.Num() - 1)];
	if (not IsValid(Variant))
	{
		return;
	}
	AnimalMesh->SetMaterial(0, Variant);
}

void AWanderer::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (M_State == EWandererState::Walking)
	{
		TickWalking(DeltaSeconds);
	}
}

void AWanderer::StartIdle(const bool bRandomizeDuration)
{
	M_State = EWandererState::Idle;
	SetActorTickEnabled(false);

	const float ClipDuration = VatSettings.IdleClip.GetDuration(VatSettings.FramesPerSecond);
	const int32 MinLoops = FMath::Max(1, MovementSettings.MinIdleLoops);
	const int32 Loops = FMath::RandRange(MinLoops, FMath::Max(MinLoops, MovementSettings.MaxIdleLoops));
	float IdleDuration = ClipDuration * Loops;
	float PhaseSeconds = 0.f;
	if (bRandomizeDuration)
	{
		// Start somewhere inside the idle so a herd placed together does not move in lockstep.
		PhaseSeconds = FMath::FRandRange(0.f, IdleDuration);
		IdleDuration -= PhaseSeconds;
	}
	PlayVatClip(VatSettings.IdleClip, true, PhaseSeconds);

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	constexpr float MinTimerDelay = 0.05f;
	World->GetTimerManager().SetTimer(M_IdleTimerHandle, this, &AWanderer::OnIdleFinished,
	                                  FMath::Max(IdleDuration, MinTimerDelay), false);
}

void AWanderer::OnIdleFinished()
{
	if (M_State != EWandererState::Idle)
	{
		return;
	}
	if (not TryStartWalking())
	{
		StartIdle(false);
	}
}

bool AWanderer::TryStartWalking()
{
	FVector Destination;
	if (not PickDestination(Destination) || not BuildPathTo(Destination))
	{
		return false;
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_IdleTimerHandle);
	}
	const float StraightDistance = FVector::Dist2D(GetActorLocation(), Destination);
	M_WalkTimeLeft = FMath::Max(WandererConstants::MinWalkTimeout,
	                            StraightDistance / MovementSettings.WalkSpeed * WandererConstants::WalkTimeoutFactor);
	M_State = EWandererState::Walking;
	PlayVatClip(VatSettings.WalkClip, true, 0.f);
	SetActorTickEnabled(true);
	return true;
}

UNavigationSystemV1* AWanderer::GetNavigationSystemIfOnNavMesh() const
{
	if (not MovementSettings.bUseNavigation)
	{
		return nullptr;
	}
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not NavigationSystem)
	{
		return nullptr;
	}
	FNavLocation NavLocation;
	if (not NavigationSystem->ProjectPointToNavigation(GetActorLocation(), NavLocation))
	{
		return nullptr;
	}
	return NavigationSystem;
}

bool AWanderer::PickDestination(FVector& OutDestination) const
{
	const UNavigationSystemV1* NavigationSystem = GetNavigationSystemIfOnNavMesh();
	const FVector Location = GetActorLocation();
	for (int32 Attempt = 0; Attempt < WandererConstants::DestinationAttempts; ++Attempt)
	{
		const FVector2D Offset = FMath::RandPointInCircle(MovementSettings.WanderRadius);
		FVector Candidate = M_WanderOrigin + FVector(Offset.X, Offset.Y, 0.f);
		if (NavigationSystem)
		{
			FNavLocation NavLocation;
			if (not NavigationSystem->ProjectPointToNavigation(Candidate, NavLocation))
			{
				continue;
			}
			Candidate = NavLocation.Location;
		}
		if (FVector::Dist2D(Candidate, Location) >= MovementSettings.MinWalkDistance)
		{
			OutDestination = Candidate;
			return true;
		}
	}
	return false;
}

bool AWanderer::BuildPathTo(const FVector& Destination)
{
	M_PathPoints.Reset();
	M_PathIndex = 0;
	// Without a navmesh under us walk in a straight line; ambient animals must not freeze on maps without nav.
	if (not GetNavigationSystemIfOnNavMesh())
	{
		M_PathPoints.Add(Destination);
		return true;
	}
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		GetWorld(), GetActorLocation(), Destination, this);
	if (not Path || not Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
	{
		return false;
	}
	// The first point is our own location.
	for (int32 PointIndex = 1; PointIndex < Path->PathPoints.Num(); ++PointIndex)
	{
		M_PathPoints.Add(Path->PathPoints[PointIndex]);
	}
	return true;
}

void AWanderer::TickWalking(const float DeltaSeconds)
{
	M_WalkTimeLeft -= DeltaSeconds;
	if (not M_PathPoints.IsValidIndex(M_PathIndex) || M_WalkTimeLeft <= 0.f)
	{
		StopWalking();
		return;
	}
	FVector Location = GetActorLocation();
	const FVector Target = M_PathPoints[M_PathIndex];
	const FVector ToTarget = FVector(Target.X - Location.X, Target.Y - Location.Y, 0.f);
	if (ToTarget.Size() <= WandererConstants::AcceptanceRadius)
	{
		++M_PathIndex;
		return;
	}

	const float DesiredYaw = ToTarget.Rotation().Yaw;
	const float CurrentYaw = GetActorRotation().Yaw;
	const float YawError = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
	const float MaxYawStep = MovementSettings.TurnSpeedDegrees * DeltaSeconds;
	const float NewYaw = CurrentYaw + FMath::Clamp(YawError, -MaxYawStep, MaxYawStep);

	const float SpeedFactor = FMath::Abs(YawError) > WandererConstants::SharpTurnDegrees
		                          ? WandererConstants::SharpTurnSpeedFactor
		                          : 1.f;
	const FRotator NewRotation(0.f, NewYaw, 0.f);
	Location += NewRotation.Vector() * MovementSettings.WalkSpeed * SpeedFactor * DeltaSeconds;
	Location.Z = GetGroundHeight(Location);
	SetActorLocationAndRotation(Location, NewRotation);
}

void AWanderer::StopWalking()
{
	M_PathPoints.Reset();
	M_PathIndex = 0;
	StartIdle(false);
}

void AWanderer::PlayVatClip(const FWandererVatClip& Clip, const bool bLoop, const float PhaseSeconds) const
{
	if (not GetIsValidAnimalMesh())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	AnimalMesh->SetCustomPrimitiveDataFloat(WandererVatPrimitiveData::StartRow, Clip.StartRow);
	AnimalMesh->SetCustomPrimitiveDataFloat(WandererVatPrimitiveData::NumFrames, Clip.NumFrames);
	AnimalMesh->SetCustomPrimitiveDataFloat(WandererVatPrimitiveData::StartTime, Now - PhaseSeconds);
	AnimalMesh->SetCustomPrimitiveDataFloat(WandererVatPrimitiveData::Loop, bLoop ? 1.f : 0.f);
	AnimalMesh->SetCustomPrimitiveDataFloat(WandererVatPrimitiveData::FramesPerSecond, VatSettings.FramesPerSecond);
}

float AWanderer::GetGroundHeight(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	if (not World)
	{
		return Location.Z;
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WandererGround), false, this);
	FHitResult Hit;
	const FVector Start = Location + FVector::UpVector * WandererConstants::GroundTraceUp;
	const FVector End = Location - FVector::UpVector * WandererConstants::GroundTraceDown;
	if (World->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return Hit.ImpactPoint.Z;
	}
	return Location.Z;
}

float AWanderer::TakeDamage(const float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
                            AActor* DamageCauser)
{
	const float DamageTaken = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (M_State == EWandererState::Idle && MovementSettings.bStartWalkingWhenDamaged && IsUnitAlive())
	{
		(void)TryStartWalking();
	}
	return DamageTaken;
}

void AWanderer::OnUnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}
	M_State = EWandererState::Dead;
	SetActorTickEnabled(false);
	M_PathPoints.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_IdleTimerHandle);
	}
	PlayVatClip(VatSettings.DeathClip, false, 0.f);
	DisableCorpseCollision();
	if (DeathSettings.CorpseLifeTime > 0.f)
	{
		SetLifeSpan(VatSettings.DeathClip.GetDuration(VatSettings.FramesPerSecond) + DeathSettings.CorpseLifeTime);
	}
	// Sets the unit dying and fires the BP event; must come after our logic (see ADestructableEnvActor).
	Super::OnUnitDies(DeathType);
}

void AWanderer::DisableCorpseCollision() const
{
	if (not GetIsValidAnimalMesh())
	{
		return;
	}
	// Corpses should not soak up bullets or get crushed again.
	AnimalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimalMesh->SetGenerateOverlapEvents(false);
}

bool AWanderer::GetIsValidAnimalMesh() const
{
	if (IsValid(AnimalMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "AnimalMesh", "GetIsValidAnimalMesh", this);
	return false;
}
