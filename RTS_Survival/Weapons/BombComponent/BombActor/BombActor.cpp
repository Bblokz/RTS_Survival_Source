#include "BombActor.h"

#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Physics/FRTS_PhysicsHelper.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

ABombActor::ABombActor()
{
	PrimaryActorTick.bCanEverTick = false;

	M_BombMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombMesh"));
	M_BombMeshComp->SetMobility(EComponentMobility::Movable);
	M_BombMeshComp->SetSimulatePhysics(false);
	M_BombMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // we use traces instead
	RootComponent = M_BombMeshComp;

	M_ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	M_ProjectileMovementComp->UpdatedComponent = M_BombMeshComp;
	M_ProjectileMovementComp->bRotationFollowsVelocity = true;
	M_ProjectileMovementComp->bShouldBounce = false;
	M_ProjectileMovementComp->ProjectileGravityScale = M_GravityFallSpeedMlt;
	M_ProjectileMovementComp->bAutoActivate = false; // activate when we drop
}
void ABombActor::BeginPlay()
{
	Super::BeginPlay();
	// start hidden/inactive
	SetBombDormant();
}

void ABombActor::InitBombActor(const FWeaponData& NewWeaponData,
                               const uint8 OwningPlayer,
                               UStaticMesh* BombMesh,
                               UBombComponent* OwningComponent,
                               const float GravitySpeedMlt, const float InitialSpeed, const FBombFXSettings& BombSettings)
{
	if (OwningPlayer <= 0)
	{
		RTSFunctionLibrary::ReportError("Invalid owning player provided to init bomb with!"
			"\n OwningPlayer: <=0");
	}
	if (!M_BombMeshComp)
	{
		RTSFunctionLibrary::ReportError("No bomb mesh component on: " + GetName());
		return;
	}

	M_WeaponData = NewWeaponData;
	M_OwningPlayer = OwningPlayer;
	M_Owner = OwningComponent;
	M_BombSettings = BombSettings;

	if (!BombMesh)
	{
		RTSFunctionLibrary::ReportError("Invalid BombMesh on init of: " + GetName());
	}
	else
	{
		M_BombMeshComp->SetStaticMesh(BombMesh);
	}

	(void)EnsureOwnerIsValid();

	if (GravitySpeedMlt > 0.f)
	{
		M_GravityFallSpeedMlt = GravitySpeedMlt;
		M_ProjectileMovementComp->ProjectileGravityScale = M_GravityFallSpeedMlt;
	}
	M_InitialSpeed = InitialSpeed;

	M_TraceChannel = OwningPlayer == 1 ? COLLISION_TRACE_ENEMY : COLLISION_TRACE_PLAYER;
}

void ABombActor::ActivateBomb(const FTransform& LaunchTransform, const TWeakObjectPtr<AActor> TargetActor)
{
	if (!M_bIsDormant)
	{
		RTSFunctionLibrary::ReportError("ActivateBomb called but already active: " + GetName());
		return;
	}

	SetActorTransform(LaunchTransform);
	SetActorScale3D(M_BombSettings.M_BombScale);

	M_bIsDormant = false;
	SetActorHiddenInGame(false);

	// Ensure movement is ready and fresh
	if (M_ProjectileMovementComp)
	{
		M_ProjectileMovementComp->SetUpdatedComponent(M_BombMeshComp);
		M_ProjectileMovementComp->ProjectileGravityScale = M_GravityFallSpeedMlt;
		M_ProjectileMovementComp->StopMovementImmediately();
		M_ProjectileMovementComp->InitialSpeed = M_InitialSpeed;
		M_ProjectileMovementComp->MaxSpeed = 0.f; // no cap
		M_ProjectileMovementComp->Activate(true);

		// OPTION B: forward release (keep plane forward speed + gravity)
		const FVector ForwardVel = GetActorForwardVector() * M_InitialSpeed;

		// If we have a target, nudge the XY direction toward it; otherwise keep forward
		const FVector StartLoc = GetActorLocation();
		const FVector ChosenVel = TargetActor.IsValid()
			? ComputeNudgedVelocityXY(ForwardVel, StartLoc, TargetActor)
			: ForwardVel;

		M_ProjectileMovementComp->Velocity = ChosenVel;
	}

	// Start tracing slightly later to avoid immediate self-hits / zero-length sweeps
	if (const UWorld* W = GetWorld())
	{
		M_LastTraceTime = W->GetTimeSeconds();
		W->GetTimerManager().SetTimer(
			M_TraceTimerHandle,
			this,
			&ABombActor::PreformAsyncLineTrace,
			0.2f,     // interval
			true,     // looping
			0.05f     // initial delay
		);
	}
}

float ABombActor::ComputeNudgeAlpha(const float PlanarDist) const
{
	// Max possible alpha (hard cap)
	constexpr float K_MaxAlpha = 0.95f;

	// How much of alpha comes from distance vs. accuracy
	constexpr float K_DistanceFrac = 0.80f;               // 80% from distance
	constexpr float K_AccuracyFrac = 1.0f - K_DistanceFrac; // 20% from accuracy

	// Distance scaling: reach full distance contribution at this range
	constexpr float K_FullDistUU = 4000.0f;

	// Accuracy determined by weapon data.
	const float WeaponDataAcc  = FMath::Clamp(M_WeaponData.Accuracy / 100.0f, 0.0f, 1.0f);
	
	if(M_WeaponData.Accuracy >=90 || PlanarDist <= 1500)
	{
		return K_MaxAlpha * WeaponDataAcc;
	}

	// Normalize terms to [0,1]
	const float DistFactor  = FMath::Clamp(PlanarDist / K_FullDistUU, 0.0f, 1.0f);

	// Weighted contributions
	const float DistAlpha      = DistFactor * K_DistanceFrac;   // up to 0.80
	const float AccuracyAlpha  = WeaponDataAcc * K_AccuracyFrac;   // up to 0.20 at 100 accuracy

	// Combine and apply cap
	const float Alpha01 = FMath::Clamp(DistAlpha + AccuracyAlpha, 0.0f, 1.0f);
	return Alpha01 * K_MaxAlpha; // max is 0.95
}


FVector ABombActor::ComputeNudgedVelocityXY(const FVector& BaseVel, const FVector& StartLoc, const TWeakObjectPtr<AActor> TargetActor) const
{
	if (!TargetActor.IsValid())
	{
		return BaseVel;
	}

	const FVector TargetLoc = TargetActor->GetActorLocation();
	const float PlanarDist  = FVector::Dist2D(StartLoc, TargetLoc);
	if (PlanarDist < 1.f)
	{
		// nearly overhead → no nudge
		return BaseVel;
	}
	const float Alpha = ComputeNudgeAlpha(PlanarDist);

	// Effective gravity from world and projectile settings (UE gravity is negative Z)
	const float GravityZ = (GetWorld() ? GetWorld()->GetGravityZ() : -980.f) *
		(M_ProjectileMovementComp ? M_ProjectileMovementComp->ProjectileGravityScale : 1.f);

	// 1) First try: closed-form ballistic that hits using current InitialSpeed (low arc)
	bool bSolvedWithFixedSpeed = false;
	FVector IdealVel = SolveBallisticVelocity_FixedSpeed(
		StartLoc, TargetLoc, M_InitialSpeed, GravityZ, /*bPreferLowArc=*/true, bSolvedWithFixedSpeed);

	// 2) Fallback: exact time-based solution (adjusts speed as needed) → guaranteed hit path
	if (!bSolvedWithFixedSpeed)
	{
		const float t = SuggestFlightTime(StartLoc, TargetLoc, M_InitialSpeed);
		IdealVel = SolveBallisticVelocity_ByTime(StartLoc, TargetLoc, t, GravityZ);
	}
	// 3) Blend base forward-release toward the ideal “hit” velocity using alpha
	return BlendVelocityByAlpha(BaseVel, IdealVel, Alpha);
}



void ABombActor::SetBombDormant()
{
	if (M_bIsDormant)
	{
		RTSFunctionLibrary::ReportError("SetBombDormant called but already dormant: " + GetName());
		return;
	}

	M_bIsDormant = true;
	SetActorHiddenInGame(true);

	if (M_ProjectileMovementComp)
	{
		M_ProjectileMovementComp->StopMovementImmediately();
		M_ProjectileMovementComp->Deactivate();
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(M_TraceTimerHandle);
	}
}


bool ABombActor::EnsureOwnerIsValid() const
{
	if (!M_Owner.IsValid())
	{
		RTSFunctionLibrary::ReportError("Bomb owner invalid for: " + GetName());
		return false;
	}
	return true;
}

void ABombActor::PreformAsyncLineTrace()
{
	if (!GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const double DeltaTime = Now - M_LastTraceTime;
	M_LastTraceTime = Now;

	const FVector StartLoc = GetActorLocation();
	const FVector Vel = M_ProjectileMovementComp->Velocity;
	const FVector EndLoc = StartLoc + Vel * DeltaTime * 1.1f; // margin

	FCollisionQueryParams Params(FName(TEXT("BombTrace")), false, this);
	Params.bTraceComplex = false;
	Params.bReturnPhysicalMaterial = true;

	FTraceDelegate TraceDel;
	TraceDel.BindUObject(this, &ABombActor::OnAsyncTraceComplete);

	GetWorld()->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		StartLoc,
		EndLoc,
		M_TraceChannel,
		Params,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDel
	);
}

void ABombActor::OnAsyncTraceComplete(const FTraceHandle& /*Handle*/, FTraceDatum& Datum)
{
	if (Datum.OutHits.Num() <= 0)

	{
		return;
	}
	const FHitResult Result = Datum.OutHits[0];
	const ERTSSurfaceType HitSurfaceType = FRTS_PhysicsHelper::GetRTSSurfaceType(Result.PhysMaterial);
	OnHitActor(Result.GetActor(), Result.Location, HitSurfaceType);
	SetBombDormant();
}

void ABombActor::OnHitActor(
	AActor* HitActor,
	const FVector& HitLocation,
	const ERTSSurfaceType HitSurface)
{
	SpawnExplosion(HitLocation, HitSurface);
	HitActor = FRTSWeaponHelpers::GetHitActorAdjustedForChildActorComponents(HitActor);
	if (not HitActor)
	{
		return;
	}

	const float FluxRatio = M_WeaponData.DamageFlux / 100;
	M_DamageEvent.HitInfo.Location = HitLocation;
	const float FullDamage = M_WeaponData.BaseDamage * (1 + FMath::RandRange(-FluxRatio, FluxRatio));
	if (HitActor->TakeDamage(FullDamage, M_DamageEvent, nullptr, this) == 0)
	{
		if (M_Owner.IsValid())
		{
			M_Owner->OnBombKilledActor(HitActor);
		}
	}
}

void ABombActor::SpawnExplosion(const FVector& Location, const ERTSSurfaceType HitSurface) const
{
	const UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	if (M_BombSettings.M_ImpactVfx.Contains(HitSurface))
	{
		const FRTSSurfaceImpactData Data = M_BombSettings.M_ImpactVfx.FindRef(HitSurface);
		USoundCue* Sound = Data.ImpactSound;
		UNiagaraSystem* Vfx = Data.ImpactEffect;
		if (IsValid(Vfx))
		{
			// Spawn the explosion effect
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Vfx, Location,
			                                               FRotator(1.f), M_BombSettings.M_ImpactScale);
		}
		if (IsValid(Sound))
		{
			UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, 1,
			                                      1, 0, M_BombSettings.M_ImpactAttenuation,
			                                      M_BombSettings.M_ImpactConcurrency);
		}
		return;
	}
	const FString SurfaceString = FRTS_PhysicsHelper::GetSurfaceTypeString(HitSurface);
	RTSFunctionLibrary::ReportError(
		"The impact vfx for bomb: " + GetName() + " does not contain a surface: " +
		SurfaceString + " in the impact vfx map.");
}

FVector ABombActor::SolveBallisticVelocity_FixedSpeed(
	const FVector& Start, const FVector& Target,
	const float Speed, const float GravityZ, const bool bPreferLowArc, bool& bOutSolved) const
{
	bOutSolved = false;

	const FVector Delta   = Target - Start;
	const FVector DeltaXY = FVector(Delta.X, Delta.Y, 0.f);
	const float   d       = DeltaXY.Size();
	const float   Dz      = Delta.Z;

	const float g = -GravityZ; // positive magnitude
	if (g <= KINDA_SMALL_NUMBER || Speed <= KINDA_SMALL_NUMBER || d <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float S2 = Speed * Speed;
	const float S4 = S2 * S2;

	// Discriminant for existence of fixed-speed ballistic solution
	const float RootTerm = S4 - g * (g * d * d + 2.f * Dz * S2);
	if (RootTerm < 0.f)
	{
		return FVector::ZeroVector; // no solution with this speed
	}

	const float SqrtTerm = FMath::Sqrt(FMath::Max(0.f, RootTerm));

	// tan(theta) solutions
	const float Denom = g * d;
	const float TanThetaLow  = (S2 - SqrtTerm) / Denom;
	const float TanThetaHigh = (S2 + SqrtTerm) / Denom;
	const float TanTheta     = bPreferLowArc ? TanThetaLow : TanThetaHigh;

	const float CosTheta = 1.f / FMath::Sqrt(1.f + TanTheta * TanTheta);
	const float SinTheta = TanTheta * CosTheta;

	const FVector DirXY = d > KINDA_SMALL_NUMBER ? (DeltaXY / d) : FVector(1, 0, 0);

	// v0 = s*(cosθ * dirXY + sinθ * ẑ)
	const FVector V0 = Speed * (DirXY * CosTheta + FVector(0, 0, 1) * SinTheta);
	bOutSolved = true;
	return V0;
}

FVector ABombActor::SolveBallisticVelocity_ByTime(
	const FVector& Start, const FVector& Target,
	const float Time, const float GravityZ) const
{
	// v0 = (Δ - 0.5*g*t^2) / t
	const FVector Delta = Target - Start;
	const FVector g(0.f, 0.f, GravityZ);
	return (Delta - 0.5f * g * Time * Time) / Time;
}

float ABombActor::SuggestFlightTime(const FVector& Start, const FVector& Target, const float BaseSpeed) const
{
	// Use planar range for an intuitive time guess and clamp to keep arcs sane.
	const float d = FVector::Dist2D(Start, Target);
	const float v = FMath::Max(BaseSpeed, 10.f);

	// rough guess: time ≈ planar_distance / base_speed
	float t = d / v;

	// clamp: short hops feel snappy, long throws don’t get extreme Z
	t = FMath::Clamp(t, 0.25f, 5.0f);
	return t;
}

FVector ABombActor::BlendVelocityByAlpha(const FVector& BaseVel, const FVector& IdealVel, const float Alpha) const
{
	const float BaseSpeed  = BaseVel.Size();
	const float IdealSpeed = IdealVel.Size();

	// If the ideal is degenerate for some reason, keep base
	if (IdealSpeed < KINDA_SMALL_NUMBER)
	{
		return BaseVel;
	}

	// Blend directions (approximate slerp via normalized lerp), then blend speeds
	const FVector BaseDir  = BaseSpeed  > KINDA_SMALL_NUMBER ? (BaseVel  / BaseSpeed)  : (IdealVel / IdealSpeed);
	const FVector IdealDir = IdealVel / IdealSpeed;

	const FVector BlendedDir = (BaseDir * (1.f - Alpha) + IdealDir * Alpha).GetSafeNormal();
	const float   BlendedSpd = FMath::Lerp(BaseSpeed, IdealSpeed, Alpha);

	return BlendedDir * BlendedSpd;
}