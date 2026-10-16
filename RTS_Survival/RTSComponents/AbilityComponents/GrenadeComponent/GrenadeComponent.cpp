// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GrenadeComponent.h"

#include "Engine/OverlapResult.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

namespace GrenadeComponentConstants
{
	constexpr float ThrowTime = 2.f;
	constexpr float ThrowInterpInterval = 0.02f;
	constexpr float ArcHeight = 200.f;
	const FName GrenadeSocketName = TEXT("grenade");
	// Socket on the grenade static mesh that carries the attached effect (molotov rag flame).
	const FName GrenadeEffectSocketName = TEXT("GrenadeEffect");
	// A sticky grenade attaches to a vehicle hull within this distance of its landing spot.
	constexpr float StickToVehicleSearchRadius = 150.f;
	// A sticky grenade ordered at a point this close to an enemy vehicle hull is thrown at that vehicle.
	constexpr float TargetVehicleSearchRadius = 300.f;
	// A target vehicle that drove further than this many throw ranges from the thrower is no longer followed.
	constexpr float MaxTrackedVehicleRangeMultiplier = 2.f;

	ECollisionChannel GetVictimObjectType(const int32 OwningPlayer)
	{
		return OwningPlayer == 1 ? COLLISION_OBJ_ENEMY : COLLISION_OBJ_PLAYER;
	}

	/**
	 * @brief Closest hull of an armored vehicle of the victim side around a location.
	 * @param World World to search.
	 * @param Location Search center.
	 * @param SearchRadius Search radius.
	 * @param VictimObjectType Collision object type of the side the grenade hurts.
	 * @param IgnoredActor Actor to leave out (the grenade itself).
	 * @return The hull component, or null when no armored vehicle is that close.
	 */
	UPrimitiveComponent* FindClosestVehicleHull(UWorld& World, const FVector& Location, const float SearchRadius,
	                                            const ECollisionChannel VictimObjectType, const AActor* IgnoredActor)
	{
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(VictimObjectType);
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GrenadeFindVehicleHull), false, IgnoredActor);
		TArray<FOverlapResult> Overlaps;
		World.OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, ObjectQueryParams,
		                               FCollisionShape::MakeSphere(SearchRadius), QueryParams);
		UPrimitiveComponent* ClosestHull = nullptr;
		float ClosestDistance = TNumericLimits<float>::Max();
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* OverlapActor = Overlap.GetActor();
			UPrimitiveComponent* OverlapComponent = Overlap.GetComponent();
			// Only armored vehicles take a sticky grenade; soldiers and buildings do not.
			if (not IsValid(OverlapActor) || not IsValid(OverlapComponent) ||
				not IsValid(OverlapActor->FindComponentByClass<UArmorCalculation>()))
			{
				continue;
			}
			EAoeDistanceSource DistanceSource = EAoeDistanceSource::ActorOrigin;
			const float Distance = FRTS_AOE::GetFalloffDistance(*OverlapActor, OverlapComponent, Location,
			                                                    DistanceSource);
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestHull = OverlapComponent;
			}
		}
		return ClosestHull;
	}
	constexpr float GroundSettleTraceUp = 100.f;
	constexpr float GroundSettleTraceDown = 2000.f;

	// Right-hand bone names of the skeletons in the project, tried when a mesh has no grenade socket.
	const FName RightHandBoneNames[] = {TEXT("hand_r"), TEXT("Hand_R"), TEXT("RightHand"), TEXT("r_hand")};

	// Not const: USkinnedMeshComponent::GetBoneNames is not.
	FName FindRightHandBone(USkeletalMeshComponent& MeshComponent)
	{
		for (const FName& BoneName : RightHandBoneNames)
		{
			if (MeshComponent.GetBoneIndex(BoneName) != INDEX_NONE)
			{
				return BoneName;
			}
		}
		TArray<FName> BoneNames;
		MeshComponent.GetBoneNames(BoneNames);
		for (const FName& BoneName : BoneNames)
		{
			const FString Name = BoneName.ToString().ToLower();
			const bool bIsRightHand = Name.Contains(TEXT("hand")) && (Name.Contains(TEXT("right")) ||
				Name.EndsWith(TEXT("_r")) || Name.StartsWith(TEXT("r_")));
			if (bIsRightHand)
			{
				return BoneName;
			}
		}
		return NAME_None;
	}

	/**
	 * @brief Where a thrower holds and releases the grenade: the grenade socket, else the right hand.
	 * A skeleton without the socket (SK_Ger_HeavilyArmored had none) used to cancel every throw of its squad.
	 */
	FName GetGrenadeHoldPoint(USkeletalMeshComponent& MeshComponent)
	{
		if (MeshComponent.DoesSocketExist(GrenadeSocketName))
		{
			return GrenadeSocketName;
		}
		const FName RightHand = FindRightHandBone(MeshComponent);
		const USkeletalMesh* SkeletalMesh = MeshComponent.GetSkeletalMeshAsset();
		static TSet<FString> MeshesReported;
		const FString MeshName = IsValid(SkeletalMesh) ? SkeletalMesh->GetPathName() : TEXT("no mesh");
		if (not MeshesReported.Contains(MeshName))
		{
			MeshesReported.Add(MeshName);
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Skeletal mesh %s has no \"%s\" socket; its soldiers hold grenades at %s instead."
					" Add the socket to its skeleton for the right grip."),
				*MeshName, *GrenadeSocketName.ToString(),
				RightHand.IsNone() ? TEXT("the mesh origin") : *RightHand.ToString()));
		}
		return RightHand;
	}
}

AGrenadeActor::AGrenadeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	M_StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeMesh"));
	M_StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(M_StaticMeshComponent);
	M_AttachedEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AttachedGrenadeEffect"));
	M_AttachedEffectComponent->SetupAttachment(M_StaticMeshComponent, GrenadeComponentConstants::GrenadeEffectSocketName);
	M_AttachedEffectComponent->SetAutoActivate(false);
	// Flames burn upwards whatever angle the hand or the flight holds the grenade at.
	M_AttachedEffectComponent->SetUsingAbsoluteRotation(true);
	M_AttachedEffectComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_ExplosionEffectScale = FVector::OneVector;
	M_ThrowDuration = GrenadeComponentConstants::ThrowTime;
}

void AGrenadeActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);
}

void AGrenadeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearThrowTimers();
	StopBurnArea();
	Super::EndPlay(EndPlayReason);
}

void AGrenadeActor::ThrowAndExplode(const FGrenadeComponentSettings& DamageParams, const FVector& StartLocation,
                                    const FVector& EndLocation, const int32 OwningPlayer, UStaticMesh* OverrideMesh)
{
	CacheEffectData(DamageParams);
	M_ThrowSettings = DamageParams;
	M_ThrowOwningPlayer = OwningPlayer;
	M_StartLocation = StartLocation;
	M_EndLocation = EndLocation;
	M_TrackedVehicle = nullptr;
	M_State = EGrenadeActorState::Flying;

	SetGrenadeMesh(OverrideMesh);

	SetActorLocation(StartLocation);
	SetActorHiddenInGame(false);
	StartThrowTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_LandingTimerHandle,
			this,
			&AGrenadeActor::OnLanded,
			M_ThrowDuration,
			false);
	}
}

void AGrenadeActor::TrackVehicle(AActor* Vehicle, const FVector& LocalTargetOffset)
{
	M_TrackedVehicle = Vehicle;
	M_TrackedLocalOffset = LocalTargetOffset;
}

void AGrenadeActor::SetInHand()
{
	M_State = EGrenadeActorState::InHand;
}

void AGrenadeActor::ResetGrenade()
{
	ClearThrowTimers();
	M_TrackedVehicle = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	DeactivateAttachedEffect();
	SetActorHiddenInGame(true);
	M_State = M_BurnState.TicksRemaining > 0 ? EGrenadeActorState::Burning : EGrenadeActorState::Idle;
}

void AGrenadeActor::ClearThrowTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_ThrowTimerHandle);
	World->GetTimerManager().ClearTimer(M_LandingTimerHandle);
	World->GetTimerManager().ClearTimer(M_FuseTimerHandle);
}

void AGrenadeActor::ActivateAttachedEffect(UNiagaraSystem* AttachedEffect, const FVector& EffectScale)
{
	if (not GetIsValidAttachedEffectComponent())
	{
		return;
	}

	if (not AttachedEffect)
	{
		DeactivateAttachedEffect();
		return;
	}

	// Re-attach so the socket of the mesh set for this throw is used.
	M_AttachedEffectComponent->AttachToComponent(
		M_StaticMeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		GrenadeComponentConstants::GrenadeEffectSocketName);
	M_AttachedEffectComponent->SetRelativeScale3D(EffectScale);
	if (M_AttachedEffectComponent->GetAsset() != AttachedEffect)
	{
		M_AttachedEffectComponent->SetAsset(AttachedEffect);
	}
	M_AttachedEffectComponent->Activate(true);
}

void AGrenadeActor::DeactivateAttachedEffect()
{
	if (not GetIsValidAttachedEffectComponent())
	{
		return;
	}

	M_AttachedEffectComponent->DeactivateImmediate();
}

bool AGrenadeActor::GetIsAvailableForThrow() const
{
	return M_State == EGrenadeActorState::Idle && not bM_DestroyWhenDone;
}

void AGrenadeActor::ReleaseFromPool()
{
	const bool bIsBusy = M_State == EGrenadeActorState::Flying || M_State == EGrenadeActorState::Fusing ||
		M_State == EGrenadeActorState::Burning;
	if (bIsBusy)
	{
		bM_DestroyWhenDone = true;
		return;
	}
	Destroy();
}

void AGrenadeActor::DestroyIfReleased()
{
	if (bM_DestroyWhenDone && M_State == EGrenadeActorState::Idle)
	{
		Destroy();
	}
}

void AGrenadeActor::SetGrenadeMesh(UStaticMesh* OverrideMesh)
{
	if (not GetIsValidStaticMeshComponent())
	{
		return;
	}

	if (OverrideMesh)
	{
		M_StaticMeshComponent->SetStaticMesh(OverrideMesh);
	}
}

bool AGrenadeActor::GetIsValidStaticMeshComponent() const
{
	if (IsValid(M_StaticMeshComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_StaticMeshComponent", __func__, this);
	return false;
}

bool AGrenadeActor::GetIsValidAttachedEffectComponent() const
{
	if (IsValid(M_AttachedEffectComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_AttachedEffectComponent", __func__, this);
	return false;
}

void AGrenadeActor::CacheEffectData(const FGrenadeComponentSettings& DamageParams)
{
	M_ExplosionEffect = DamageParams.ExplosionEffect;
	M_ExplosionSound = DamageParams.ExplosionSound;
	M_ExplosionSoundAttenuation = DamageParams.ExplosionSoundAttenuation;
	M_ExplosionSoundConcurrency = DamageParams.ExplosionSoundConcurrency;
	M_ExplosionEffectScale = DamageParams.ExplosionEffectScale;
}

void AGrenadeActor::StartThrowTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_ThrowTimerHandle,
			this,
			&AGrenadeActor::OnTickThrow,
			GrenadeComponentConstants::ThrowInterpInterval,
			true);
	}
}

void AGrenadeActor::OnTickThrow()
{
	if (not GetIsValidStaticMeshComponent())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// Nullable by design: only sticky grenades thrown at a vehicle follow one.
		if (M_TrackedVehicle.IsValid())
		{
			M_EndLocation = M_TrackedVehicle->GetActorTransform().TransformPosition(M_TrackedLocalOffset);
		}
		const float Elapsed = World->GetTimerManager().GetTimerElapsed(M_LandingTimerHandle);
		const float Alpha = FMath::Clamp(Elapsed / M_ThrowDuration, 0.f, 1.f);
		const FVector NewLocation = CalculateArcLocation(Alpha);
		SetActorLocation(NewLocation);
	}
}

void AGrenadeActor::OnLanded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ThrowTimerHandle);
	}
	if (M_TrackedVehicle.IsValid())
	{
		M_EndLocation = M_TrackedVehicle->GetActorTransform().TransformPosition(M_TrackedLocalOffset);
	}
	M_TrackedVehicle = nullptr;
	SetActorLocation(M_EndLocation);

	AActor* StuckToVehicle = M_ThrowSettings.bStickToVehicles ? TryStickToVehicle() : nullptr;
	if (not IsValid(StuckToVehicle))
	{
		SettleOnGround();
	}
	OnGrenadeLanded.Broadcast(this, StuckToVehicle);

	const float FuseSeconds = M_ThrowSettings.FuseSecondsAfterLanding;
	UWorld* World = GetWorld();
	if (FuseSeconds <= 0.f || not IsValid(World))
	{
		Explode();
		return;
	}
	M_State = EGrenadeActorState::Fusing;
	World->GetTimerManager().SetTimer(M_FuseTimerHandle, this, &AGrenadeActor::Explode, FuseSeconds, false);
}

AActor* AGrenadeActor::TryStickToVehicle()
{
	using namespace GrenadeComponentConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	const FVector LandingLocation = GetActorLocation();
	UPrimitiveComponent* ClosestHull = FindClosestVehicleHull(
		*World, LandingLocation, StickToVehicleSearchRadius, GetVictimObjectType(M_ThrowOwningPlayer), this);
	if (not IsValid(ClosestHull))
	{
		return nullptr;
	}

	FVector PointOnHull;
	if (ClosestHull->GetClosestPointOnCollision(LandingLocation, PointOnHull) < 0.f)
	{
		PointOnHull = ClosestHull->Bounds.GetBox().GetClosestPointTo(LandingLocation);
	}
	SetActorLocation(PointOnHull);
	// Keeps the grenade on the hull while the vehicle drives on; the explosion happens where it is then.
	AttachToComponent(ClosestHull, FAttachmentTransformRules::KeepWorldTransform);
	return ClosestHull->GetOwner();
}

void AGrenadeActor::SettleOnGround()
{
	using namespace GrenadeComponentConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// The target can be a vehicle top that drove away during the flight; drop onto the ground below.
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, GroundSettleTraceUp);
	const FVector To = GetActorLocation() - FVector(0.f, 0.f, GroundSettleTraceDown);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GrenadeSettleOnGround), false, this);
	if (World->LineTraceSingleByObjectType(GroundHit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic),
	                                       QueryParams))
	{
		SetActorLocation(GroundHit.ImpactPoint);
	}
}

void AGrenadeActor::Explode()
{
	ClearThrowTimers();
	const FVector ExplosionLocation = GetActorLocation();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FGrenadeComponentSettings& DamageParams = M_ThrowSettings;
	UNiagaraComponent* ImpactEffect = PlayExplosionFX(ExplosionLocation);
	if (DamageParams.AoeRange > 0.f && DamageParams.Damage > 0.f)
	{
		const ETriggerOverlapLogic OverlapLogic = GetOverlapLogicForOwningPlayer(M_ThrowOwningPlayer);
		const TArray<TWeakObjectPtr<AActor>> ActorsToIgnore;
		// Measured to each victim's hull: a grenade on a tank's engine deck is inside the radius of that tank.
		constexpr EAoeFalloffDistance FalloffDistance = EAoeFalloffDistance::ClosestCollision;

		FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
			this,
			ExplosionLocation,
			DamageParams.AoeRange,
			DamageParams.Damage,
			DamageParams.AoeDamageExponentReduction,
			DamageParams.FullArmorPen,
			DamageParams.ArmorPenFallOff,
			DamageParams.MaxArmorPen,
			DamageParams.DamageType,
			OverlapLogic,
			EShieldDamageSource::Shrapnel,
			ActorsToIgnore,
			EVehicleModuleSplashPolicy::DamageExternalModules,
			static_cast<EWeaponShellType>(0),
			FalloffDistance);
	}

	StartBurnArea(DamageParams, ExplosionLocation, M_ThrowOwningPlayer, ImpactEffect);
	OnGrenadeExploded.Broadcast(this, ExplosionLocation);
	ResetGrenade();
	DestroyIfReleased();
}

ETriggerOverlapLogic AGrenadeActor::GetOverlapLogicForOwningPlayer(const int32 OwningPlayer)
{
	return OwningPlayer == 1 ? ETriggerOverlapLogic::OverlapEnemy : ETriggerOverlapLogic::OverlapPlayer;
}

bool AGrenadeActor::StartBurnArea(const FGrenadeComponentSettings& DamageParams, const FVector& BurnLocation,
                                  const int32 OwningPlayer, UNiagaraComponent* ImpactEffect)
{
	StopBurnArea();
	const FGrenadeBurnAreaSettings& BurnSettings = DamageParams.BurnArea;
	if (not BurnSettings.GetIsBurnAreaEnabled())
	{
		// No burn area: the impact effect plays out on its own.
		return false;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	M_BurnImpactEffectComponent = ImpactEffect;
	M_BurnState.BurnLocation = BurnLocation;
	M_BurnState.TicksRemaining = FMath::Max(
		1, FMath::FloorToInt(BurnSettings.BurnDuration / BurnSettings.BurnTickInterval));
	M_BurnState.DamagePerTick = BurnSettings.BurnDamagePerTick;
	M_BurnState.Radius = BurnSettings.BurnRadius > 0.f ? BurnSettings.BurnRadius : DamageParams.AoeRange;
	M_BurnState.DamageFalloffExponent = DamageParams.AoeDamageExponentReduction;
	M_BurnState.FullArmorPen = DamageParams.FullArmorPen;
	M_BurnState.ArmorPenFallOff = DamageParams.ArmorPenFallOff;
	M_BurnState.MaxArmorPen = DamageParams.MaxArmorPen;
	M_BurnState.DamageType = DamageParams.DamageType;
	M_BurnState.OverlapLogic = GetOverlapLogicForOwningPlayer(OwningPlayer);

	World->GetTimerManager().SetTimer(
		M_BurnState.BurnTickTimerHandle,
		this,
		&AGrenadeActor::OnBurnTick,
		BurnSettings.BurnTickInterval,
		true);
	return true;
}

void AGrenadeActor::OnBurnTick()
{
	if (M_BurnState.TicksRemaining <= 0)
	{
		StopBurnArea();
		return;
	}

	const TArray<TWeakObjectPtr<AActor>> ActorsToIgnore;
	FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
		this,
		M_BurnState.BurnLocation,
		M_BurnState.Radius,
		M_BurnState.DamagePerTick,
		M_BurnState.DamageFalloffExponent,
		M_BurnState.FullArmorPen,
		M_BurnState.ArmorPenFallOff,
		M_BurnState.MaxArmorPen,
		M_BurnState.DamageType,
		M_BurnState.OverlapLogic,
		EShieldDamageSource::Flamethrower,
		ActorsToIgnore,
		EVehicleModuleSplashPolicy::Ignore,
		static_cast<EWeaponShellType>(0),
		EAoeFalloffDistance::ClosestCollision);

	--M_BurnState.TicksRemaining;
	if (M_BurnState.TicksRemaining <= 0)
	{
		StopBurnArea();
	}
}

void AGrenadeActor::StopBurnArea()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_BurnState.BurnTickTimerHandle);
	}
	M_BurnState.TicksRemaining = 0;

	// Stop spawning new flames; live particles finish and the auto-destroying component cleans itself up.
	if (M_BurnImpactEffectComponent.IsValid())
	{
		M_BurnImpactEffectComponent->Deactivate();
	}
	M_BurnImpactEffectComponent = nullptr;
	if (M_State == EGrenadeActorState::Burning)
	{
		M_State = EGrenadeActorState::Idle;
		DestroyIfReleased();
	}
}

UNiagaraComponent* AGrenadeActor::PlayExplosionFX(const FVector& ExplosionLocation) const
{
	if (M_ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			M_ExplosionSound,
			ExplosionLocation,
			FRotator::ZeroRotator,
			1.f,
			1.f,
			0.f,
			M_ExplosionSoundAttenuation,
			M_ExplosionSoundConcurrency);
	}
	if (M_ExplosionEffect)
	{
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			M_ExplosionEffect,
			ExplosionLocation,
			FRotator::ZeroRotator,
			M_ExplosionEffectScale);
	}
	return nullptr;
}

FVector AGrenadeActor::CalculateArcLocation(const float Alpha) const
{
	const FVector FlatLerp = FMath::Lerp(M_StartLocation, M_EndLocation, Alpha);
	const float Parabola = -4 * GrenadeComponentConstants::ArcHeight * FMath::Square(Alpha - 0.5f)
		+ GrenadeComponentConstants::ArcHeight;
	FVector Offset = FlatLerp;
	Offset.Z += Parabola;
	return Offset;
}

UGrenadeComponent::UGrenadeComponent()
	: M_AbilityState(EGrenadeAbilityState::NotActive)
	  , M_GrenadesRemaining(0)
	  , M_OwningPlayer(INDEX_NONE)
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UGrenadeComponent::GetGrenadeAoeRange() const
{
	return M_Settings.AoeRange;
}

float UGrenadeComponent::GetThrowRange() const
{
	return M_Settings.ThrowRange;
}

void UGrenadeComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CacheOwningPlayer();
	BeginPlay_CreateGrenadePool();
	M_GrenadesRemaining = M_Settings.GrenadesPerSquad;
}

void UGrenadeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Not in BeginDestroy: destroying actors during garbage collection is unsafe.
	DestroyGrenadePool();
	Super::EndPlay(EndPlayReason);
}

void UGrenadeComponent::Init(ASquadController* SquadController)
{
	M_SquadController = SquadController;
	if (not GetIsValidSquadController())
	{
		return;
	}

	Init_SetAbilityWhenSquadDataLoaded();
}

void UGrenadeComponent::ExecuteThrowGrenade(const FVector& TargetLocation)
{
	if (not CanUseGrenades())
	{
		return;
	}
	ResetThrowSequenceState();
	ExecuteThrowGrenade_MoveOrThrow(TargetLocation);
}

void UGrenadeComponent::TerminateThrowGrenade()
{
	if (M_AbilityState == EGrenadeAbilityState::Throwing)
	{
		return;
	}

	if (M_AbilityState == EGrenadeAbilityState::MovingToThrowPosition)
	{
		if (GetIsValidSquadController())
		{
			M_SquadController->StopMovement();
		}
		ResetThrowSequenceState();
	}
	M_AbilityState = EGrenadeAbilityState::NotActive;
	SetAbilityToThrowGrenade();
}

void UGrenadeComponent::CancelThrowGrenade()
{
	if (M_AbilityState != EGrenadeAbilityState::Throwing)
	{
		TerminateThrowGrenade();
		return;
	}

	CancelThrowSequence();
	M_AbilityState = EGrenadeAbilityState::NotActive;
	SetAbilityToThrowGrenade();
}

EGrenadeAbilityType UGrenadeComponent::GetGrenadeAbilityType() const
{
	return M_Settings.GrenadeAbility;
}

bool UGrenadeComponent::GetIsValidSquadController() const
{
	if (M_SquadController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_SquadController", __func__, GetOwner());
	return false;
}

void UGrenadeComponent::OnSquadUnitArrivedAtThrowLocation(ASquadUnit* SquadUnit)
{
	if (M_AbilityState != EGrenadeAbilityState::MovingToThrowPosition)
	{
		return;
	}

	if (M_ThrowSequenceState.bM_HasStarted)
	{
		return;
	}

	if (not M_ThrowSequenceState.bM_IsWaitingForArrival)
	{
		return;
	}

	if (not IsValid(SquadUnit))
	{
		return;
	}

	StartThrowSequence();
}

void UGrenadeComponent::OnSquadUnitDied(ASquadUnit* DeadUnit)
{
	if (M_AbilityState != EGrenadeAbilityState::Throwing)
	{
		return;
	}

	if (not IsValid(DeadUnit))
	{
		return;
	}

	FGrenadeThrowerState* ThrowerState = FindThrowerState(DeadUnit);
	if (not ThrowerState)
	{
		return;
	}

	ResetThrowerState(*ThrowerState);
	ThrowerState->M_GrenadesRemaining = 0;
	ThrowerState->M_SquadUnit = nullptr;

	if (not HasPendingThrows())
	{
		OnThrowFinished();
	}
}

bool UGrenadeComponent::GetIsValidRTSComponent() const
{
	if (not M_RTSComponent.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_RTSComponent", __func__, GetOwner());
		return false;
	}
	return true;
}

void UGrenadeComponent::BeginPlay_CacheOwningPlayer()
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	M_RTSComponent = M_SquadController->GetRTSComponent();
	if (not GetIsValidRTSComponent())
	{
		return;
	}

	M_OwningPlayer = M_RTSComponent->GetOwningPlayer();
}

void UGrenadeComponent::Init_SetAbilityWhenSquadDataLoaded()
{
	TWeakObjectPtr<UGrenadeComponent> WeakThis(this);
	auto ApplyLambda = [WeakThis]() -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->SetAbilityToThrowGrenade();
	};

	M_SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(ApplyLambda, WeakThis);
}

void UGrenadeComponent::BeginPlay_CreateGrenadePool()
{
	const int32 GrenadesNeeded = M_Settings.SquadUnitsThrowing * M_Settings.GrenadesPerSquad;
	if (GrenadesNeeded <= 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (int32 i = 0; i < GrenadesNeeded; ++i)
		{
			AGrenadeActor* NewGrenade = World->SpawnActor<AGrenadeActor>();
			if (IsValid(NewGrenade))
			{
				M_GrenadePool.Add(NewGrenade);
				continue;
			}

			RTSFunctionLibrary::ReportError("Failed to spawn grenade actor during pool creation.");
		}
	}
}

void UGrenadeComponent::ExecuteThrowGrenade_MoveOrThrow(const FVector& TargetLocation)
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	CacheThrowSequenceLocations(TargetLocation);
	const float DistanceToTarget = FVector::Dist(M_SquadController->GetActorLocation(), TargetLocation);
	if (DistanceToTarget > M_Settings.ThrowRange)
	{
		M_AbilityState = EGrenadeAbilityState::MovingToThrowPosition;
		M_ThrowSequenceState.bM_IsWaitingForArrival = true;
		SetAbilityToCancel();
		M_SquadController->RequestSquadMoveForAbility(M_ThrowSequenceState.ThrowLocation, EAbilityID::IdThrowGrenade);
		return;
	}

	StartThrowSequence();
}

FVector UGrenadeComponent::CalculateThrowMoveLocation(const FVector& TargetLocation) const
{
	if (not GetIsValidSquadController())
	{
		return TargetLocation;
	}

	const FVector SquadLocation = M_SquadController->GetActorLocation();
	const FVector DirectionFromTarget = (SquadLocation - TargetLocation).GetSafeNormal();
	if (DirectionFromTarget.IsNearlyZero())
	{
		return SquadLocation;
	}

	return TargetLocation + (DirectionFromTarget * M_Settings.ThrowRange);
}

void UGrenadeComponent::CacheThrowSequenceLocations(const FVector& TargetLocation)
{
	M_ThrowSequenceState.TargetLocation = TargetLocation;
	M_ThrowSequenceState.ThrowLocation = CalculateThrowMoveLocation(TargetLocation);
	M_ThrowSequenceState.bM_IsWaitingForArrival = false;
	M_ThrowSequenceState.bM_HasStarted = false;
	CacheTargetVehicle(TargetLocation);
}

void UGrenadeComponent::CacheTargetVehicle(const FVector& TargetLocation)
{
	using namespace GrenadeComponentConstants;
	M_ThrowSequenceState.TargetVehicle = nullptr;
	UWorld* World = GetWorld();
	if (not M_Settings.bStickToVehicles || not IsValid(World))
	{
		return;
	}
	const UPrimitiveComponent* TargetHull = FindClosestVehicleHull(
		*World, TargetLocation, TargetVehicleSearchRadius, GetVictimObjectType(M_OwningPlayer), GetOwner());
	AActor* TargetVehicle = IsValid(TargetHull) ? TargetHull->GetOwner() : nullptr;
	if (not IsValid(TargetVehicle))
	{
		return;
	}
	// A vehicle drives on during the walk, the throw animation and the flight; aim at the clicked spot on it.
	M_ThrowSequenceState.TargetVehicle = TargetVehicle;
	M_ThrowSequenceState.TargetVehicleLocalOffset =
		TargetVehicle->GetActorTransform().InverseTransformPosition(TargetLocation);
}

void UGrenadeComponent::TrackTargetVehicle(AGrenadeActor& Grenade, const FVector& ThrowStartLocation) const
{
	using namespace GrenadeComponentConstants;
	// Nullable by design: only sticky grenades ordered at a vehicle have one.
	if (not M_ThrowSequenceState.TargetVehicle.IsValid())
	{
		return;
	}
	AActor* TargetVehicle = M_ThrowSequenceState.TargetVehicle.Get();
	const FVector VehicleTarget = TargetVehicle->GetActorTransform().TransformPosition(
		M_ThrowSequenceState.TargetVehicleLocalOffset);
	if (FVector::Dist2D(ThrowStartLocation, VehicleTarget) > M_Settings.ThrowRange * MaxTrackedVehicleRangeMultiplier)
	{
		return;
	}
	Grenade.TrackVehicle(TargetVehicle, M_ThrowSequenceState.TargetVehicleLocalOffset);
}

void UGrenadeComponent::StartThrowSequence()
{
	if (M_AbilityState == EGrenadeAbilityState::Throwing)
	{
		ReportIllegalStateTransition(TEXT("Throwing"), TEXT("Throwing"));
		return;
	}

	M_ThrowSequenceState.bM_HasStarted = true;
	M_ThrowSequenceState.bM_IsWaitingForArrival = false;
	M_AbilityState = EGrenadeAbilityState::Throwing;
	SetAbilityToResupplying();
	StartCooldown();
	M_GrenadesRemaining = FMath::Max(0, M_GrenadesRemaining - 1);

	if (M_OwningPlayer == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Owning player not cached before grenade throw. at function: StartThrowSequence"));
	}

	BuildThrowerStates();
	if (M_ThrowerStates.Num() <= 0)
	{
		OnThrowFinished();
		return;
	}

	for (FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		StartThrowForThrower(ThrowerState);
	}
}

void UGrenadeComponent::BuildThrowerStates()
{
	M_ThrowerStates.Reset();

	if (not GetIsValidSquadController())
	{
		return;
	}

	if (M_Settings.SquadUnitsThrowing <= 0 || M_Settings.GrenadesPerSquad <= 0)
	{
		return;
	}

	const TArray<ASquadUnit*> SquadUnits = M_SquadController->GetSquadUnitsChecked();
	int32 UnitsAdded = 0;

	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		FGrenadeThrowerState ThrowerState;
		ThrowerState.M_SquadUnit = SquadUnit;
		ThrowerState.M_GrenadesRemaining = M_Settings.GrenadesPerSquad;
		M_ThrowerStates.Add(ThrowerState);

		UnitsAdded++;
		if (UnitsAdded >= M_Settings.SquadUnitsThrowing)
		{
			break;
		}
	}
}

void UGrenadeComponent::StartThrowForThrower(FGrenadeThrowerState& ThrowerState)
{
	if (ThrowerState.M_GrenadesRemaining <= 0)
	{
		return;
	}

	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		ThrowerState.M_GrenadesRemaining = 0;
		return;
	}

	ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	DisableThrowerWeapon(ThrowerState);
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
		SquadUnit->GetActorLocation(),
		M_ThrowSequenceState.TargetLocation);
	SquadUnit->SetActorRotation(LookAtRotation);
	USquadUnitAnimInstance* AnimInstance = SquadUnit->GetAnimBP_SquadUnit();
	if (IsValid(AnimInstance))
	{
		AnimInstance->StopAllMontages();
		AnimInstance->PlayGrenadeThrowMontage(M_Settings.GrenadeThrowMontageDuration);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Invalid anim instance for grenade throw on unit %s."), *SquadUnit->GetName()));
	}

	AGrenadeActor* Grenade = AcquireGrenade();
	if (not IsValid(Grenade))
	{
		Grenade = SpawnFallbackGrenade();
	}

	if (not IsValid(Grenade))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("No grenade available to throw after attempting fallback spawn."));
		ThrowerState.M_GrenadesRemaining = 0;
		EnableThrowerWeapon(ThrowerState);
		return;
	}

	AttachGrenadeToThrower(ThrowerState, Grenade);
	if (not ThrowerState.M_AttachedGrenade.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to attach grenade to squad unit before throwing."));
		ThrowerState.M_GrenadesRemaining = 0;
		EnableThrowerWeapon(ThrowerState);
		return;
	}

	StartThrowTimer(ThrowerState);
}

void UGrenadeComponent::DisableThrowerWeapon(const FGrenadeThrowerState& ThrowerState) const
{
	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return;
	}

	ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	if (not SquadUnit->GetIsValidWeapon())
	{
		return;
	}

	AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
	InfantryWeapon->DisableWeaponSearch(true, true);
}

void UGrenadeComponent::EnableThrowerWeapon(const FGrenadeThrowerState& ThrowerState) const
{
	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return;
	}

	ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	if (not SquadUnit->GetIsValidWeapon())
	{
		return;
	}

	AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
	InfantryWeapon->SetAutoEngageTargets(true);
}

void UGrenadeComponent::CancelThrowSequence()
{
	for (FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		CancelThrowerState(ThrowerState);
	}

	M_ThrowerStates.Reset();
	M_ThrowSequenceState = FGrenadeThrowSequenceState();
}

void UGrenadeComponent::CancelThrowerState(FGrenadeThrowerState& ThrowerState)
{
	ClearThrowTimer(ThrowerState);
	EnableThrowerWeapon(ThrowerState);

	if (ThrowerState.M_AttachedGrenade.IsValid())
	{
		DetachGrenadeFromThrower(ThrowerState);
		ThrowerState.M_AttachedGrenade->ResetGrenade();
	}

	ThrowerState.M_AttachedGrenade = nullptr;
	ThrowerState.M_AttachedGrenadeScale = FVector::OneVector;
}

void UGrenadeComponent::StartThrowTimer(FGrenadeThrowerState& ThrowerState)
{
	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	ClearThrowTimer(ThrowerState);
	const TWeakObjectPtr<ASquadUnit> WeakSquadUnit = ThrowerState.M_SquadUnit;
	World->GetTimerManager().SetTimer(
		ThrowerState.M_ThrowTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGrenadeComponent::OnThrowMontageFinished, WeakSquadUnit),
		M_Settings.GrenadeThrowMontageDuration,
		false);
}

void UGrenadeComponent::OnThrowMontageFinished(TWeakObjectPtr<ASquadUnit> SquadUnit)
{
	FGrenadeThrowerState* ThrowerState = FindThrowerState(SquadUnit);
	if (not ThrowerState)
	{
		return;
	}

	ClearThrowTimer(*ThrowerState);
	EnableThrowerWeapon(*ThrowerState);

	if (not ThrowerState->M_AttachedGrenade.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Grenade missing when throw montage finished."));
		ThrowerState->M_GrenadesRemaining = 0;
		if (not HasPendingThrows())
		{
			OnThrowFinished();
		}
		return;
	}

	if (not ThrowerState->M_SquadUnit.IsValid())
	{
		ResetThrowerState(*ThrowerState);
		if (not HasPendingThrows())
		{
			OnThrowFinished();
		}
		return;
	}

	const FVector StartLocation = GetThrowStartLocation(*ThrowerState);
	AGrenadeActor* Grenade = ThrowerState->M_AttachedGrenade.Get();
	DetachGrenadeFromThrower(*ThrowerState);
	Grenade->ThrowAndExplode(M_Settings, StartLocation, M_ThrowSequenceState.TargetLocation,
	                         M_OwningPlayer, M_Settings.GrenadeMesh);
	TrackTargetVehicle(*Grenade, StartLocation);
	ThrowerState->M_AttachedGrenade = nullptr;
	ThrowerState->M_AttachedGrenadeScale = FVector::OneVector;
	ThrowerState->M_GrenadesRemaining = FMath::Max(0, ThrowerState->M_GrenadesRemaining - 1);

	if (ThrowerState->M_GrenadesRemaining > 0)
	{
		StartThrowForThrower(*ThrowerState);
		return;
	}

	if (not HasPendingThrows())
	{
		OnThrowFinished();
	}
}

void UGrenadeComponent::ClearThrowTimer(FGrenadeThrowerState& ThrowerState)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThrowerState.M_ThrowTimerHandle);
	}
}

void UGrenadeComponent::ResetThrowerState(FGrenadeThrowerState& ThrowerState)
{
	ClearThrowTimer(ThrowerState);

	if (ThrowerState.M_AttachedGrenade.IsValid())
	{
		DetachGrenadeFromThrower(ThrowerState);
		ThrowerState.M_AttachedGrenade->ResetGrenade();
	}

	ThrowerState.M_AttachedGrenade = nullptr;
	ThrowerState.M_AttachedGrenadeScale = FVector::OneVector;
}

void UGrenadeComponent::AttachGrenadeToThrower(FGrenadeThrowerState& ThrowerState, AGrenadeActor* Grenade)
{
	if (not IsValid(Grenade))
	{
		return;
	}

	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return;
	}

	ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	USkeletalMeshComponent* MeshComponent = SquadUnit->GetMesh();
	if (not IsValid(MeshComponent))
	{
		return;
	}

	const FName HoldPoint = GrenadeComponentConstants::GetGrenadeHoldPoint(*MeshComponent);
	ThrowerState.M_AttachedGrenade = Grenade;
	ThrowerState.M_AttachedGrenadeScale = Grenade->GetActorScale3D();
	Grenade->SetGrenadeMesh(M_Settings.GrenadeMesh);
	Grenade->SetInHand();
	Grenade->ActivateAttachedEffect(M_Settings.AttachedGrenadeEffect, M_Settings.AttachedGrenadeEffectScale);
	Grenade->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		HoldPoint);
	Grenade->SetActorScale3D(ThrowerState.M_AttachedGrenadeScale);
	Grenade->SetActorHiddenInGame(false);
}

void UGrenadeComponent::DetachGrenadeFromThrower(FGrenadeThrowerState& ThrowerState)
{
	if (not ThrowerState.M_AttachedGrenade.IsValid())
	{
		return;
	}

	AGrenadeActor* Grenade = ThrowerState.M_AttachedGrenade.Get();
	Grenade->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Grenade->SetActorScale3D(ThrowerState.M_AttachedGrenadeScale);
}

FVector UGrenadeComponent::GetThrowStartLocation(const FGrenadeThrowerState& ThrowerState) const
{
	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return FVector::ZeroVector;
	}

	const ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	USkeletalMeshComponent* MeshComponent = SquadUnit->GetMesh();
	if (not IsValid(MeshComponent))
	{
		return SquadUnit->GetActorLocation();
	}

	const FName HoldPoint = GrenadeComponentConstants::GetGrenadeHoldPoint(*MeshComponent);
	return HoldPoint.IsNone() ? MeshComponent->GetComponentLocation() : MeshComponent->GetSocketLocation(HoldPoint);
}

UGrenadeComponent::FGrenadeThrowerState* UGrenadeComponent::FindThrowerState(
	const TWeakObjectPtr<ASquadUnit>& SquadUnit)
{
	for (FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		if (ThrowerState.M_SquadUnit == SquadUnit)
		{
			return &ThrowerState;
		}
	}
	return nullptr;
}

bool UGrenadeComponent::HasPendingThrows() const
{
	for (const FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		if (ThrowerState.M_SquadUnit.IsValid() && ThrowerState.M_GrenadesRemaining > 0)
		{
			return true;
		}
	}
	return false;
}

void UGrenadeComponent::ResetThrowSequenceState()
{
	for (FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		ResetThrowerState(ThrowerState);
	}

	M_ThrowerStates.Reset();
	M_ThrowSequenceState = FGrenadeThrowSequenceState();
}

void UGrenadeComponent::OnThrowFinished()
{
	if (M_AbilityState != EGrenadeAbilityState::Throwing)
	{
		ReportIllegalStateTransition(UEnum::GetValueAsString(M_AbilityState), TEXT("ResupplyingGrenades"));
	}

	ResetThrowSequenceState();
	M_AbilityState = EGrenadeAbilityState::ResupplyingGrenades;
	if (GetIsValidSquadController())
	{
		M_SquadController->DoneExecutingCommand(EAbilityID::IdThrowGrenade);
	}
}

void UGrenadeComponent::StartCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_CooldownTimerHandle,
			this,
			&UGrenadeComponent::ResetCooldown,
			M_Settings.Cooldown,
			false);
	}
}

void UGrenadeComponent::ResetCooldown()
{
	if (M_AbilityState == EGrenadeAbilityState::MovingToThrowPosition)
	{
		ReportIllegalStateTransition(TEXT("MovingToThrowPosition"), TEXT("NotActive"));
	}
	if (M_AbilityState == EGrenadeAbilityState::Throwing)
	{
		ReportIllegalStateTransition(TEXT("Throwing"), TEXT("NotActive"));
	}

	M_GrenadesRemaining = M_Settings.GrenadesPerSquad;
	M_AbilityState = EGrenadeAbilityState::NotActive;
	SetAbilityToThrowGrenade();
}

bool UGrenadeComponent::CanUseGrenades() const
{
	if (M_AbilityState == EGrenadeAbilityState::Throwing || M_AbilityState == EGrenadeAbilityState::ResupplyingGrenades)
	{
		return false;
	}
	return M_GrenadesRemaining > 0;
}

AGrenadeActor* UGrenadeComponent::AcquireGrenade()
{
	bool bIsAnyGrenadeStillBurning = false;
	for (TObjectPtr<AGrenadeActor>& Grenade : M_GrenadePool)
	{
		if (not IsValid(Grenade))
		{
			continue;
		}
		if (Grenade->GetIsAvailableForThrow())
		{
			return Grenade.Get();
		}
		bIsAnyGrenadeStillBurning |= Grenade->GetGrenadeState() == EGrenadeActorState::Burning ||
			Grenade->GetGrenadeState() == EGrenadeActorState::Fusing ||
			Grenade->GetGrenadeState() == EGrenadeActorState::Flying;
	}

	// Grenades still flying, on their fuse or burning are busy on purpose; the caller spawns a fallback grenade.
	if (not bIsAnyGrenadeStillBurning)
	{
		RTSFunctionLibrary::ReportError(
			"Grenade pool exhausted or invalid when attempting to acquire a grenade at function AquireGrenade");
	}
	return nullptr;
}

AGrenadeActor* UGrenadeComponent::SpawnFallbackGrenade()
{
	if (UWorld* World = GetWorld())
	{
		AGrenadeActor* FallbackGrenade = World->SpawnActor<AGrenadeActor>();
		if (IsValid(FallbackGrenade))
		{
			M_GrenadePool.Add(FallbackGrenade);
			return FallbackGrenade;
		}
	}

	RTSFunctionLibrary::ReportError("Failed to spawn fallback grenade actor. SpawnFallBackGrenade");
	return nullptr;
}

void UGrenadeComponent::DestroyGrenadePool()
{
	for (TObjectPtr<AGrenadeActor>& Grenade : M_GrenadePool)
	{
		if (IsValid(Grenade))
		{
			// Thrown grenades still explode; their thrower's squad dying does not defuse them.
			Grenade->ReleaseFromPool();
		}
	}
	M_GrenadePool.Empty();
}

FUnitAbilityEntry UGrenadeComponent::CreateGrenadeAbilityEntry(const EAbilityID AbilityId) const
{
	FUnitAbilityEntry AbilityEntry;
	AbilityEntry.AbilityId = AbilityId;
	AbilityEntry.CustomType = static_cast<int32>(M_Settings.GrenadeAbility);
	if (AbilityId == EAbilityID::IdThrowGrenade)
	{
		AbilityEntry.CooldownDuration = M_Settings.Cooldown;
	}
	return AbilityEntry;
}

void UGrenadeComponent::SetAbilityToResupplying()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const FUnitAbilityEntry GrenadeAbilityEntry = CreateGrenadeAbilityEntry(EAbilityID::IdGrenadesResupplying);
	M_SquadController->SwapAbility(EAbilityID::IdCancelThrowGrenade, GrenadeAbilityEntry);
}

void UGrenadeComponent::SetAbilityToThrowGrenade()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const FUnitAbilityEntry GrenadeAbilityEntry = CreateGrenadeAbilityEntry(EAbilityID::IdThrowGrenade);

	if (M_SquadController->HasAbility(EAbilityID::IdGrenadesResupplying))
	{
		M_SquadController->SwapAbility(EAbilityID::IdGrenadesResupplying, GrenadeAbilityEntry);
		return;
	}

	if (not M_SquadController->HasAbility(EAbilityID::IdThrowGrenade))
	{
		M_SquadController->AddAbility(GrenadeAbilityEntry, M_Settings.PreferredIndex);
	}
	M_SquadController->SwapAbility(EAbilityID::IdCancelThrowGrenade, GrenadeAbilityEntry);
}

void UGrenadeComponent::SetAbilityToCancel()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const FUnitAbilityEntry GrenadeAbilityEntry = CreateGrenadeAbilityEntry(EAbilityID::IdCancelThrowGrenade);
	M_SquadController->SwapAbility(EAbilityID::IdThrowGrenade, GrenadeAbilityEntry);
}

void UGrenadeComponent::ReportIllegalStateTransition(const FString& FromState, const FString& ToState) const
{
	const FString Message = FString::Printf(TEXT("Illegal grenade state transition: %s to %s"), *FromState, *ToState);
	RTSFunctionLibrary::ReportError(Message);
}
