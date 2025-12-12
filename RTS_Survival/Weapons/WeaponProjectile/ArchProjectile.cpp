#include "ArchProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Physics/FRTS_PhysicsHelper.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "Sound/SoundCue.h"

AArchProjectile::AArchProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  M_ProjectileOwner(nullptr),
	  M_ProjectileSpawn(FVector::ZeroVector),
	  M_Range(0),
	  M_FullDamage(0),
	  M_ArmorPen(0),
	  M_ArmorPenAtMaxRange(0),
	  M_DamageType(nullptr),
	  M_ShrapnelParticles(0),
	  M_ShrapnelRange(0),
	  M_ShrapnelDamage(0),
	  M_ShrapnelArmorPen(0),
	  M_BounceVfx(nullptr),
	  M_BounceSound(nullptr),
	  M_ImpactScale(FVector(1, 1, 1)),
	  M_BounceScale(FVector(1, 1, 1)),
	  M_TargetLocation(FVector::ZeroVector),
	  M_ArchStretch(1.0f),
	  M_TotalTime(0),
	  M_ElapsedTime(0),
	  M_Gravity(980.f),
	  M_ProjectileSpeed(0),
	  M_TraceChannel(),
	  bIsAscending(true),
	  M_AscentDamping(0.5f)
{
	PrimaryActorTick.bCanEverTick = true;

	// Create the ProjectileMovementComponent
	M_ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	M_ProjectileMovement->bRotationFollowsVelocity = true;
	M_ProjectileMovement->bShouldBounce = false;
	M_ProjectileMovement->ProjectileGravityScale = 0.0f; // No gravity during ascent
}

void AArchProjectile::InitProjectile(
	UWeaponStateArchProjectile* NewProjectileOwner,
	const float Range,
	const float Damage,
	const float ArmorPen,
	const float ArmorPenMaxRange,
	const uint32 ShrapnelParticles,
	const float ShrapnelRange,
	const float ShrapnelDamage,
	const float ShrapnelArmorPen,
	const int32 OwningPlayer,
	const TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& ImpactSurfaceData,
	UNiagaraSystem* BounceEffect,
	USoundCue* BounceSound,
	const FVector& ImpactScale,
	const FVector& BounceScale,
	const float ProjectileSpeed,
	const FRotator& LaunchRotation,
	const FVector& TargetLocation,
	const float ArchStretch,
	USoundConcurrency* ImpactConcurrency,
	USoundAttenuation* ImpactAttenuation)
{
	M_ProjectileOwner = NewProjectileOwner;
	M_Range = Range;
	M_FullDamage = Damage;
	M_ArmorPen = ArmorPen;
	M_ArmorPenAtMaxRange = ArmorPenMaxRange;
	M_ShrapnelParticles = ShrapnelParticles;
	M_ShrapnelRange = ShrapnelRange;
	M_ShrapnelDamage = ShrapnelDamage;
	M_ShrapnelArmorPen = ShrapnelArmorPen;
	M_SurfaceImpactData = ImpactSurfaceData;
	M_ImpactScale = ImpactScale;
	M_BounceSound = BounceSound;
	M_BounceVfx = BounceEffect;
	M_BounceScale = BounceScale;
	M_ProjectileSpeed = ProjectileSpeed;
	M_ArchStretch = ArchStretch;
	M_TargetLocation = TargetLocation;
	M_ImpactAttenuation = ImpactAttenuation;
	M_ImpactConcurrency = ImpactConcurrency;

	SetActorLocation(GetActorLocation());
	SetActorRotation(LaunchRotation);

	M_StartLocation = GetActorLocation();
	M_ElapsedTime = 0.0f;
	bIsAscending = true;

	// Adjust gravity scale if needed (we'll set it during descent)
	M_ProjectileMovement->ProjectileGravityScale = 0.0f; // No gravity during ascent

	// Set the damping factor (adjust this value to control ascent steepness)
	M_AscentDamping = 0.5f; // Value between 0 and 1; lower values make ascent less steep

	CalculateAscent();

	SetupTraceChannel(OwningPlayer);
}

void AArchProjectile::CalculateAscent()
{
	// Calculate the apex height based on ArchStretch
	float ApexHeight = M_StartLocation.Z + (M_ArchStretch * 500.0f); // Adjust 500.0f as needed

	// Calculate direction to target
	FVector DirectionToTarget = (M_TargetLocation - M_StartLocation).GetSafeNormal2D();

	// Adjust ascent direction using damping factor
	FVector AscentDirection = (DirectionToTarget * M_AscentDamping) + FVector(0.0f, 0.0f, 1.0f - M_AscentDamping);
	AscentDirection = AscentDirection.GetSafeNormal();

	// Set ascent velocity
	FVector AscentVelocity = AscentDirection * M_ProjectileSpeed;

	// Set the velocity
	M_ProjectileMovement->Velocity = AscentVelocity;

	// Store the apex height
	M_ApexHeight = ApexHeight;
}

void AArchProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void AArchProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsAscending)
	{
		// Check if we've reached the apex
		if (GetActorLocation().Z >= M_ApexHeight)
		{
			// Begin descent
			bIsAscending = false;
			BeginDescent();
		}
	}
	else
	{
		// Check for impact when close to target
		float DistanceToTarget = (GetActorLocation() - M_TargetLocation).Size();
		if (DistanceToTarget <= 100.0f)
		{
			CheckForImpact();
		}

		// Destroy if we have passed the total time or if we are below ground
		if (GetActorLocation().Z <= 0.f)
		{
			HandleTimedExplosion();
		}
	}
}

void AArchProjectile::BeginDescent()
{
	// Set gravity scale back to normal
	M_ProjectileMovement->ProjectileGravityScale = 0.0f; // No gravity during descent

	// Calculate direction to target
	FVector DirectionToTarget = (M_TargetLocation - GetActorLocation()).GetSafeNormal();

	// Set velocity towards target
	M_ProjectileMovement->Velocity = DirectionToTarget * M_ProjectileSpeed;
}

void AArchProjectile::CheckForImpact()
{
	// Perform a trace for enemies
	FVector StartLocation = GetActorLocation();
	FVector EndLocation = M_TargetLocation;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult HitResult;
	bool bHit = GetWorld()->
		LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, M_TraceChannel, QueryParams);
	ERTSSurfaceType SurfaceHit = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);

	if (bHit && IsValid(HitResult.GetActor()))
	{
		// Hit an actor
		OnHitActor(HitResult.GetActor(), HitResult.ImpactPoint, SurfaceHit);
	}
	else
	{
		// No enemy hit, do a visibility trace to find ground
		FHitResult GroundHitResult;
		bool bGroundHit = GetWorld()->LineTraceSingleByChannel(GroundHitResult, StartLocation, EndLocation,
		                                                       ECC_Visibility, QueryParams);

		if (bGroundHit)
		{
			// Spawn explosion at ground location
			SpawnExplosion(GroundHitResult.ImpactPoint, ERTSSurfaceType::Sand);
			Destroy();
		}
		else
		{
			// Spawn explosion at target location
			SpawnExplosion(M_TargetLocation, ERTSSurfaceType::Sand);
			Destroy();
		}
	}
}

void AArchProjectile::HandleTimedExplosion()
{
	SpawnExplosion(GetActorLocation(), ERTSSurfaceType::Air);
	// Handle shrapnel damage
	if (M_ShrapnelParticles > 0)
	{
		// Implement shrapnel logic here
	}
	Destroy();
}

void AArchProjectile::OnHitActor(AActor* HitActor, const FVector& HitLocation, const ERTSSurfaceType HitSurface)
{
	if (RTSFunctionLibrary::RTSIsValid(HitActor))
	{
		if (HitActor->TakeDamage(M_FullDamage, M_DamageEvent, nullptr, this) == 0)
		{
			if (IsValid(M_ProjectileOwner))
			{
				M_ProjectileOwner->OnProjectileKilledActor(HitActor);
			}
		}
		SpawnExplosion(HitLocation, HitSurface);
		// Handle shrapnel damage
		if (M_ShrapnelParticles > 0)
		{
			// Implement shrapnel logic here
		}
		Destroy();
	}
}

void AArchProjectile::SpawnExplosion(const FVector Location,
	const ERTSSurfaceType SurfaceHit) const
{
	if(UWorld* World = GetWorld())
	{
		
		if (M_SurfaceImpactData.Contains(SurfaceHit))
		{
			FRTSSurfaceImpactData Data = M_SurfaceImpactData.FindRef(SurfaceHit);
			USoundCue* sound = Data.ImpactSound;
			UNiagaraSystem* Vfx = Data.ImpactEffect;
			if (IsValid(Vfx))
			{
				// Spawn the explosion effect
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Vfx, Location,
				                                               FRotator(1.f), M_ImpactScale);
			}
			if (IsValid(sound))
			{
				UGameplayStatics::PlaySoundAtLocation(World, sound, Location, FRotator::ZeroRotator, 1,
				                                      1, 0, M_ImpactAttenuation, M_ImpactConcurrency);
			}
			return;
		}
	}
	const FString SurfaceString = FRTS_PhysicsHelper::GetSurfaceTypeString(SurfaceHit);
	RTSFunctionLibrary::ReportError("The impact vfx for ARCH projectile: " + GetName() + " does not contain a surface: " +
		SurfaceString + " in the impact vfx map.");
}

void AArchProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AArchProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (!IsValid(M_ProjectileMovement))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "ProjectileMovement", "PostInitComponents");
	}
}

void AArchProjectile::SetupTraceChannel(int NewOwningPlayer)
{
	if (NewOwningPlayer == 1)
	{
		M_TraceChannel = COLLISION_TRACE_ENEMY;
	}
	else
	{
		M_TraceChannel = COLLISION_TRACE_PLAYER;
	}
}
