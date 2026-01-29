// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "InfantryWeaponMaster.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadAnimationEnums/SquadAnimationEnums.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateMultiHitLaser.h"
#include "RTS_Survival/Weapons/WeaponAIState/WeaponAIState.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"

// Sets default values for this component's properties
AInfantryWeaponMaster::AInfantryWeaponMaster()
	: WeaponMesh(nullptr)
	  , M_WorldSpawnedIn(nullptr)
	  , M_OwningSquadUnit(nullptr)
	  , M_AimOffsetType(ESquadWeaponAimOffset::Rifle)
	  , M_SquadUnitAnimInstance(nullptr)
	  , M_OwningPlayer(-1)
	  , bM_IsPendingTarget(false)
	  , M_WeaponAIState(EWeaponAIState::None)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryActorTick.bCanEverTick = false;
}

void AInfantryWeaponMaster::SetOwningPlayer(const int32 PlayerIndex)
{
	M_OwningPlayer = PlayerIndex;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);
}

void AInfantryWeaponMaster::SetAutoEngageTargets(const bool bUseLastTarget)
{
	if (IsValid(WeaponMesh) && not WeaponMesh->IsVisible())
	{
		WeaponMesh->SetVisibility(true);
	}
	if (not bUseLastTarget)
	{
		ResetTarget();
	}
	if (not GetIsValidWeaponState())
	{
		return;
	}
	WeaponState->StopFire(false, true);
	InitiateAutoEngageTimers();
}

void AInfantryWeaponMaster::InitiateAutoEngageTimers()
{
	M_WeaponAIState = EWeaponAIState::AutoEngage;
	if (M_WorldSpawnedIn)
	{
		M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
		M_WeaponSearchDel.BindUObject(this, &AInfantryWeaponMaster::AutoEngage);
		M_WorldSpawnedIn->GetTimerManager().SetTimer(
			M_WeaponSearchHandle,
			M_WeaponSearchDel,
			AInfantryWeaponMaster::M_WeaponSearchTimeInterval,
			true);
	}
}

void AInfantryWeaponMaster::InitiateSpecificEngageTimers()
{
	M_WeaponAIState = EWeaponAIState::SpecificEngage;
	if (M_WorldSpawnedIn)
	{
		M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
		M_WeaponSearchDel.BindUObject(this, &AInfantryWeaponMaster::SpecificEngage);
		M_WorldSpawnedIn->GetTimerManager().SetTimer(
			M_WeaponSearchHandle,
			M_WeaponSearchDel,
			AInfantryWeaponMaster::M_WeaponSearchTimeInterval,
			true);
	}
}

void AInfantryWeaponMaster::BeginPlay_SetupCallbackToProjectileManager()
{
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not GameState)
	{
		return;
	}
	TWeakObjectPtr<AInfantryWeaponMaster> WeakThis(this);
	auto Callback = [WeakThis](const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnProjectileManagerLoaded(ProjectileManager);
	};
	GameState->RegisterCallbackForSmallArmsProjectileMgr(Callback, this);
}

void AInfantryWeaponMaster::BeginPlay_SetupGameUnitManager()
{
	if (UGameUnitManager* GUM = FRTS_Statics::GetGameUnitManager(this))
	{
		M_GameUnitManager = GUM;
	}
}

bool AInfantryWeaponMaster::GetIsValidGameUnitManager() const
{
	if (M_GameUnitManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("GameUnitManager not initialized for InfantryWeapon: " + GetName());
	return false;
}

bool AInfantryWeaponMaster::GetIsValidWeaponState() const
{
	if (IsValid(WeaponState))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"WeaponState",
		"GetIsValidWeaponState",
		this
	);

	return false;
}

void AInfantryWeaponMaster::AutoEngage()
{
	if (not GetIsSquadUnitOwnerValid())
	{
		RTSFunctionLibrary::ReportError("Owning squad unit not valid for weapon: " + GetName() +
			"\n at function: AInfantryWeaponMaster::AutoEngage");
		return;
	}

	if (GetIsTargetValidVisibleAndInRange())
	{
		// Let aim selection possibly rotate to a new aim point before we read the location.
		M_TargetingData.TickAimSelection();

		// Save TargetPitch for weapon.
		M_FireDirection = CalculateTargetDirection(GetActorLocation());

		// Target is allowed to be reset when the unit is walking and the angle is too large.
		if (GetIsTargetWithinAngleLimitsAndSetAO(true))
		{
			WeaponState->Fire(M_TargetingData.GetActiveTargetLocation());
			return;
		}
		WeaponState->StopFire(false, true);
		return;
	}

	ResetTarget();
	GetClosestTarget();
}

void AInfantryWeaponMaster::SpecificEngage()
{
	if (not GetIsSquadUnitOwnerValid())
	{
		RTSFunctionLibrary::ReportError("Owning squad unit not valid for weapon: " + GetName() +
			"\n at function: AInfantryWeaponMaster::SpecificEngage");
		return;
	}

	// Valid & visible per struct (Ground always valid).
	if (M_TargetingData.GetIsTargetValid())
	{
		M_TargetingData.TickAimSelection();

		const FVector TargetLocation = M_TargetingData.GetActiveTargetLocation();
		const bool bInRange = (FVector::DistSquared(TargetLocation, M_OwningSquadUnit->GetActorLocation())
			<= M_WeaponRangeData.M_MaxWeaponRangeSquared);

		if (bInRange)
		{
			// Target is in range.
			Debug_Weapons("Specific Valid target in range");

			// Save TargetPitch for weapon.
			M_FireDirection = CalculateTargetDirection(GetActorLocation());

			// Notify owner in range, if the owner was moving closer that movement will be stopped.
			M_OwningSquadUnit->OnSpecificTargetInRange();

			// Do not allow the target to be reset when the unit is walking and the angle is too large.
			if (GetIsTargetWithinAngleLimitsAndSetAO(false))
			{
				WeaponState->Fire(M_TargetingData.GetActiveTargetLocation());
				return;
			}

			WeaponState->StopFire(false, true);
			return;
		}

		// Target is out of range.
		Debug_Weapons("Specific target out of range");
		M_OwningSquadUnit->OnSpecificTargetOutOfRange(TargetLocation);
		return;
	}

	ResetTarget();
	M_OwningSquadUnit->OnSpecificTargetDestroyedOrInvisible();
	SetAutoEngageTargets(false);
}

bool AInfantryWeaponMaster::IsSquadUnitAnimInstanceValid()
{
	if (IsValid(M_SquadUnitAnimInstance))
	{
		return true;
	}
	if (GetIsSquadUnitOwnerValid())
	{
		UAnimInstance* AnimInstance = M_OwningSquadUnit->GetMesh()->GetAnimInstance();
		if (IsValid(AnimInstance))
		{
			M_SquadUnitAnimInstance = Cast<USquadUnitAnimInstance>(AnimInstance);
			if (not IsValid(M_SquadUnitAnimInstance))
			{
				RTSFunctionLibrary::ReportFailedCastError("AnimInstance", "USquadUnitAnimInstance",
				                                          "AInfantryWeaponMaster::IsSquadUnitAnimInstanceValid");
			}
			return IsValid(M_SquadUnitAnimInstance);
		}
		RTSFunctionLibrary::ReportNullErrorComponent(this, "AnimInstance",
		                                             "AInfantryWeaponMaster::IsSquadUnitAnimInstanceValid");
		return false;
	}
	return false;
}

void AInfantryWeaponMaster::CheckIfMeshIsValid()
{
	if (IsValid(WeaponMesh))
	{
		return;
	}
	RTSFunctionLibrary::ReportError("The infantry weapon mesh is not valid, attempting to repair the reference");
	WeaponMesh = FindComponentByClass<UStaticMeshComponent>();
}

void AInfantryWeaponMaster::GetClosestTarget()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GetClosestTarget);

	if (not GetIsSquadUnitOwnerValid() || not GetIsValidGameUnitManager())
	{
		return;
	}
	if (bM_IsPendingTarget)
	{
		return;
	}

	// Prefer the squad unit’s RTS owning player; also ensures the struct is seeded.
	if (IsValid(M_OwningSquadUnit) && IsValid(M_OwningSquadUnit->GetRTSComponent()))
	{
		SetOwningPlayer(M_OwningSquadUnit->GetRTSComponent()->GetOwningPlayer());
	}

	// Create a weak pointer to this actor
	TWeakObjectPtr<AInfantryWeaponMaster> WeakThis(this);

	const FVector SearchLocation = GetActorLocation();
	const float SearchRadius = M_WeaponRangeData.M_WeaponSearchRadius;
	const int32 NumTargets = 3;
	bM_IsPendingTarget = true;

	// Request targets from the game unit manager
	M_GameUnitManager->RequestClosestTargets(
		SearchLocation,
		SearchRadius,
		NumTargets,
		M_OwningPlayer,
		TargetPreference,
		[WeakThis](const TArray<AActor*>& Targets)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnTargetsFound(Targets);
			}
		});
}

bool AInfantryWeaponMaster::GetIsTargetValidVisibleAndInRange()
{
	if (not GetIsSquadUnitOwnerValid())
	{
		return false;
	}
	if (not M_TargetingData.GetIsTargetValid())
	{
		return false;
	}
	const FVector TargetLoc = M_TargetingData.GetActiveTargetLocation();
	const FVector MyLoc = M_OwningSquadUnit->GetActorLocation();
	return (FVector::DistSquared(TargetLoc, MyLoc) <= M_WeaponRangeData.M_MaxWeaponRangeSquared);
}

void AInfantryWeaponMaster::OnTargetsFound(const TArray<AActor*>& Targets)
{
	bM_IsPendingTarget = false;
	for (AActor* NewTarget : Targets)
	{
		if (RTSFunctionLibrary::RTSIsVisibleTarget(NewTarget, M_OwningPlayer))
		{
			M_TargetingData.SetTargetActor(NewTarget);

			if constexpr (DeveloperSettings::Debugging::GAsyncTargetFinding_Compile_DebugSymbols)
			{
				FString TargetType = "None ";

				if (ATankMaster* Tank = Cast<ATankMaster>(NewTarget))
				{
					TargetType = "Tank";
				}
				else if (AHpPawnMaster* TestHpPawn = Cast<AHpPawnMaster>(NewTarget))
				{
					TargetType = Global_GetTargetPreferenceAsString(TestHpPawn->GetTargetPreference());
				}
				else if (ASquadUnit* SquadUnit = Cast<ASquadUnit>(NewTarget))
				{
					TargetType = "SquadUnit";
				}

				const FColor DebugColor = M_OwningPlayer == 1 ? FColor::Green : FColor::Red;
				const FString DebugString = M_OwningPlayer == 1 ? "Targeted by P1" : "Targeted By Enemy";
				const FVector DebugLocation = NewTarget->GetActorLocation() + FVector(0, 0, 300);

				DrawDebugString(GetWorld(), DebugLocation, TargetType + DebugString, nullptr, DebugColor, 2.0f, false);
			}
			return;
		}
	}
}

void AInfantryWeaponMaster::Debug_Weapons(const FString& DebugMessage) const
{
	if constexpr (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(DebugMessage);
	}
}

void AInfantryWeaponMaster::OnProjectileManagerLoaded(
	const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager) const
{
	if (not IsValid(ProjectileManager))
	{
		RTSFunctionLibrary::ReportError("OnProjectileManagerLoaded: ProjectileManager is not valid"
			"\n For weapon: " + GetName());
	}
	FRTSWeaponHelpers::SetupProjectileManagerForWeapon(WeaponState, ProjectileManager);
}

void AInfantryWeaponMaster::SetEngageSpecificTarget(AActor* Target)
{
	// If we’re already in specific mode and the same actor is targeted, ignore.
	if (M_WeaponAIState == EWeaponAIState::SpecificEngage && M_TargetingData.GetTargetActor() == Target)
	{
		return;
	}

	M_TargetingData.SetTargetActor(Target);

	if (IsValid(WeaponMesh) && not WeaponMesh->IsVisible())
	{
		WeaponMesh->SetVisibility(true);
	}
	if (not GetIsValidWeaponState())
	{
		return;
	}
	WeaponState->StopFire(false, true);
	InitiateSpecificEngageTimers();
}

void AInfantryWeaponMaster::SetEngageGroundLocation(const FVector& GroundLocation)
{
	if (IsValid(WeaponMesh) && !WeaponMesh->IsVisible())
	{
		WeaponMesh->SetVisibility(true);
	}

	if (not GetIsValidWeaponState())
	{
		return;
	}

	WeaponState->StopFire(/*bStopReload*/false, /*bStopCoolDown*/true);

	// Push the ground target into the target struct and run the specific engage loop.
	M_TargetingData.SetTargetGround(GroundLocation);
	InitiateSpecificEngageTimers();
}


void AInfantryWeaponMaster::CheckTargetKilled(AActor* KilledActor)
{
	if (not M_TargetingData.WasKilledActorCurrentTarget(KilledActor))
	{
		return;
	}
	// Notify owner & fall back to auto engage.
	if (IsValid(M_OwningSquadUnit))
	{
		// Also called when target out of sight to move closer.
		M_OwningSquadUnit->OnSpecificTargetDestroyedOrInvisible();
		// Called when a weapon kills the target only.
		M_OwningSquadUnit->OnWeaponKilledTarget();
	}
	SetAutoEngageTargets(false);
}

TArray<UWeaponState*> AInfantryWeaponMaster::GetWeapons()
{
	TArray<UWeaponState*> WeaponStates;
	if (GetIsValidWeaponState())
	{
		WeaponStates.Add(WeaponState);
	}
	return WeaponStates;
}

void AInfantryWeaponMaster::DisableWeaponSearch(const bool bStopReload, const bool bMakeWeaponInvisible)
{
	if (GetIsValidWeaponState())
	{
		WeaponState->StopFire(bStopReload, true);
	}
	if (bMakeWeaponInvisible && IsValid(WeaponMesh))
	{
		WeaponMesh->SetVisibility(false);
	}
	if (M_WorldSpawnedIn)
	{
		M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	}
}

void AInfantryWeaponMaster::DisableAllWeapons()
{
	if (GetIsValidWeaponState())
	{
		WeaponState->DisableWeapon();
	}
	if (M_WorldSpawnedIn)
	{
		M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	}
}

// Called when the game starts
void AInfantryWeaponMaster::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
		if (not IsValid(PlayerController))
		{
			RTSFunctionLibrary::ReportNullErrorInitialisation(this, "PlayerController",
			                                                  "AInfantryWeaponMaster::BeginPlay");
		}
		M_WorldSpawnedIn = World;
	}

	BeginPlay_SetupCallbackToProjectileManager();
	BeginPlay_SetupGameUnitManager();

	CheckIfMeshIsValid();
}

void AInfantryWeaponMaster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (M_WorldSpawnedIn)
	{
		M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	}
}

void AInfantryWeaponMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	WeaponMesh = FindComponentByClass<UStaticMeshComponent>();
}

void AInfantryWeaponMaster::SetWeaponAimOffsetType(const ESquadWeaponAimOffset AimOffsetType)
{
	M_AimOffsetType = AimOffsetType;
}

int32 AInfantryWeaponMaster::GetOwningPLayerForWeaponInit()
{
	if (AActor* OwnerActor = GetParentActor())
	{
		if (const ASquadUnit* SquadUnit = Cast<ASquadUnit>(OwnerActor))
		{
			return SquadUnit->GetRTSComponent()->GetOwningPlayer();
		}
		RTSFunctionLibrary::ReportError("No ASquadunit found for weapon: " + GetName() +
			"\n at function: AInfantryWeaponMaster::GetOwningPlayerForWeaponInit");
	}
	RTSFunctionLibrary::ReportError("No Parent actor found for weapon: " + GetName()
		+ "\n at function: AInfantryWeaponMaster::GetOwningPlayerForWeaponInit");
	return int32();
}

void AInfantryWeaponMaster::RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister)
{
	if (not RTSFunctionLibrary::RTSIsValid(ActorToIgnore))
	{
		return;
	}
	if (GetIsValidWeaponState())
	{
		WeaponState->RegisterActorToIgnore(ActorToIgnore, bRegister);	
	}
}

void AInfantryWeaponMaster::OnWeaponAdded(const int32 /*WeaponIndex*/, UWeaponState* /*Weapon*/)
{
}

void AInfantryWeaponMaster::OnWeaponBehaviourChangesRange(const UWeaponState* WeaponState, const float NewRange)
{
	SetupRange();
}

FVector& AInfantryWeaponMaster::GetFireDirection(const int32 /*WeaponIndex*/)
{
	return M_FireDirection;
}

FVector& AInfantryWeaponMaster::GetTargetLocation(const int32 WeaponIndex)
{
	return M_TargetingData.GetActiveTargetLocation();
}

bool AInfantryWeaponMaster::AllowWeaponToReload(const int32 /*WeaponIndex*/) const
{
	return true;
}

void AInfantryWeaponMaster::OnWeaponKilledActor(const int32 /*WeaponIndex*/, AActor* KilledActor)
{
	// Common helper that checks if the killed actor was the active target.
	CheckTargetKilled(KilledActor);
}

void AInfantryWeaponMaster::PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode)
{
	if(IsValid(M_OwningSquadUnit))
	{
		M_OwningSquadUnit->OnWeaponFire();
	}
	if (IsSquadUnitAnimInstanceValid())
	{
		switch (FireMode)
		{
		// one fire effect
		case EWeaponFireMode::Single:
			M_SquadUnitAnimInstance->PlaySingleFireAnim();
			break;
		// one fire effect
		case EWeaponFireMode::SingleBurst:
			M_SquadUnitAnimInstance->PlayBurstAnim();
			break;
		// fire effect per bullet but animation once!
		case EWeaponFireMode::RandomBurst:
			M_SquadUnitAnimInstance->PlayBurstAnim();
			break;
		}
	}
}

void AInfantryWeaponMaster::OnProjectileHit(const bool bBounced)
{
	if(IsValid(M_OwningSquadUnit))
	{
		M_OwningSquadUnit->OnProjectileHit(bBounced);
	}
}

void AInfantryWeaponMaster::OnReloadStart(const int32 /*WeaponIndex*/, const float ReloadTime)
{
	if (IsSquadUnitAnimInstanceValid())
	{
		M_SquadUnitAnimInstance->PlayReloadAnim(ReloadTime);
	}
}

void AInfantryWeaponMaster::OnReloadFinished(const int32 /*WeaponIndex*/)
{
}

void AInfantryWeaponMaster::ForceSetAllWeaponsFullyReloaded()
{
	if (not GetIsValidWeaponState())
	{
		return;
	}
	WeaponState->ForceInstantReload();
}

void AInfantryWeaponMaster::SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters)
{
	SetOwningPlayer(DirectHitWeaponParameters.OwningPlayer);

	constexpr int32 WeaponIndex = 0;
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for SquadUnitWeapon: " + GetName());
		return;
	}
	UWeaponStateDirectHit* DirectHit = NewObject<UWeaponStateDirectHit>(this);
	DirectHit->InitDirectHitWeapon(
		DirectHitWeaponParameters.OwningPlayer,
		WeaponIndex,
		DirectHitWeaponParameters.WeaponName,
		DirectHitWeaponParameters.WeaponBurstMode,
		DirectHitWeaponParameters.WeaponOwner,
		DirectHitWeaponParameters.MeshComponent,
		DirectHitWeaponParameters.FireSocketName,
		World,
		DirectHitWeaponParameters.WeaponVFX,
		DirectHitWeaponParameters.WeaponShellCase,
		DirectHitWeaponParameters.BurstCooldown,
		DirectHitWeaponParameters.SingleBurstAmountMaxBurstAmount,
		DirectHitWeaponParameters.MinBurstAmount,
		DirectHitWeaponParameters.CreateShellCasingOnEveryRandomBurst);

	WeaponState = DirectHit;
	SetupRange();
}

void AInfantryWeaponMaster::SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters)
{
	SetOwningPlayer(TraceWeaponParameters.OwningPlayer);

	constexpr int32 WeaponIndex = 0;
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for SquadUnitWeapon: " + GetName());
		return;
	}
	UWeaponStateTrace* Trace = NewObject<UWeaponStateTrace>(this);
	Trace->InitTraceWeapon(
		TraceWeaponParameters.OwningPlayer,
		WeaponIndex,
		TraceWeaponParameters.WeaponName,
		TraceWeaponParameters.WeaponBurstMode,
		TraceWeaponParameters.WeaponOwner,
		TraceWeaponParameters.MeshComponent,
		TraceWeaponParameters.FireSocketName,
		World,
		TraceWeaponParameters.WeaponVFX,
		TraceWeaponParameters.WeaponShellCase,
		TraceWeaponParameters.BurstCooldown,
		TraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		TraceWeaponParameters.MinBurstAmount,
		TraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		TraceWeaponParameters.TraceProjectileType);
	WeaponState = Trace;
	SetupRange();

	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(WeaponState, M_ProjectileManager.Get());
	}
}

void AInfantryWeaponMaster::SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters)
{
	SetOwningPlayer(ProjectileWeaponParameters.OwningPlayer);

	constexpr int32 WeaponIndex = 0;
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for SquadUnitWeapon: " + GetName());
		return;
	}
	UWeaponStateProjectile* Projectile = NewObject<UWeaponStateProjectile>(this);
	Projectile->InitProjectileWeapon(
		ProjectileWeaponParameters.OwningPlayer,
		WeaponIndex,
		ProjectileWeaponParameters.WeaponName,
		ProjectileWeaponParameters.WeaponBurstMode,
		ProjectileWeaponParameters.WeaponOwner,
		ProjectileWeaponParameters.MeshComponent,
		ProjectileWeaponParameters.FireSocketName,
		World,
		ProjectileWeaponParameters.ProjectileSystem,
		ProjectileWeaponParameters.WeaponVFX,
		ProjectileWeaponParameters.WeaponShellCase,
		ProjectileWeaponParameters.BurstCooldown,
		ProjectileWeaponParameters.SingleBurstAmountMaxBurstAmount,
		ProjectileWeaponParameters.MinBurstAmount,
		ProjectileWeaponParameters.CreateShellCasingOnEveryRandomBurst);
	WeaponState = Projectile;
	SetupRange();

	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(WeaponState, M_ProjectileManager.Get());
	}
}

void AInfantryWeaponMaster::SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters)
{
	SetOwningPlayer(RocketProjectileParameters.OwningPlayer);

	constexpr int32 WeaponIndex = 0;
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for SquadUnitWeapon: " + GetName());
		return;
	}
	UWeaponStateRocketProjectile* RocketProjectile = NewObject<UWeaponStateRocketProjectile>(this);
	RocketProjectile->InitRocketProjectileWeapon(
		RocketProjectileParameters.OwningPlayer,
		WeaponIndex,
		RocketProjectileParameters.WeaponName,
		RocketProjectileParameters.WeaponBurstMode,
		RocketProjectileParameters.WeaponOwner,
		RocketProjectileParameters.MeshComponent,
		RocketProjectileParameters.FireSocketName,
		World,
		RocketProjectileParameters.ProjectileSystem,
		RocketProjectileParameters.WeaponVFX,
		RocketProjectileParameters.WeaponShellCase,
		RocketProjectileParameters.RocketSettings,
		RocketProjectileParameters.BurstCooldown,
		RocketProjectileParameters.SingleBurstAmountMaxBurstAmount,
		RocketProjectileParameters.MinBurstAmount,
		RocketProjectileParameters.CreateShellCasingOnEveryRandomBurst);
	WeaponState = RocketProjectile;
	SetupRange();

	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(WeaponState, M_ProjectileManager.Get());
	}
}

void AInfantryWeaponMaster::SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState)
{
	SetOwningPlayer(MultiProjectileState.OwningPlayer);

	constexpr int32 WeaponIndex = 0;
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UWeaponStateMultiProjectile* MultiProjectile = NewObject<UWeaponStateMultiProjectile>(this);
	MultiProjectile->InitMultiProjectileWeapon(
		M_OwningPlayer,
		WeaponIndex,
		MultiProjectileState.WeaponName,
		MultiProjectileState.WeaponBurstMode,
		MultiProjectileState.WeaponOwner,
		MultiProjectileState.MeshComponent,
		World,
		MultiProjectileState.FireSocketNames,
		MultiProjectileState.ProjectileSystem,
		MultiProjectileState.WeaponVFX,
		MultiProjectileState.WeaponShellCase,
		MultiProjectileState.BurstCooldown,
		MultiProjectileState.SingleBurstAmountMaxBurstAmount,
		MultiProjectileState.MinBurstAmount,
		MultiProjectileState.CreateShellCasingOnEveryRandomBurst);
	WeaponState = MultiProjectile;
	SetupRange();

	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(WeaponState, M_ProjectileManager.Get());
	}
}

void AInfantryWeaponMaster::SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
	SetOwningPlayer(ArchProjParameters.OwningPlayer);

	constexpr int32 WeaponIndex = 0;
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for SquadUnitWeapon: " + GetName());
		return;
	}
        UWeaponStateArchProjectile* Projectile = NewObject<UWeaponStateArchProjectile>(this);
        Projectile->InitArchProjectileWeapon(
                ArchProjParameters.OwningPlayer,
                WeaponIndex,
                ArchProjParameters.WeaponName,
                ArchProjParameters.WeaponBurstMode,
                ArchProjParameters.WeaponOwner,
                ArchProjParameters.MeshComponent,
                ArchProjParameters.FireSocketName,
                World,
                ArchProjParameters.ProjectileSystem,
                ArchProjParameters.WeaponVFX,
                ArchProjParameters.WeaponShellCase,
                ArchProjParameters.BurstCooldown,
                ArchProjParameters.SingleBurstAmountMaxBurstAmount,
                ArchProjParameters.MinBurstAmount,
                ArchProjParameters.CreateShellCasingOnEveryRandomBurst,
                ArchProjParameters.ArchSettings);
        WeaponState = Projectile;
        SetupRange();

        if (M_ProjectileManager.IsValid())
        {
                FRTSWeaponHelpers::SetupProjectileManagerForWeapon(WeaponState, M_ProjectileManager.Get());
        }
}

void AInfantryWeaponMaster::SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
        SetupArchProjectileWeapon(ArchProjParameters);
}

void AInfantryWeaponMaster::SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters)
{
	SetOwningPlayer(MultiTraceWeaponParameters.OwningPlayer);

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for SquadUnitWeapon: " + GetName());
		return;
	}

	constexpr int32 WeaponIndex = 0;

	UWeaponStateMultiTrace* MultiTrace = NewObject<UWeaponStateMultiTrace>(this);
	MultiTrace->InitMultiTraceWeapon(
		MultiTraceWeaponParameters.OwningPlayer,
		WeaponIndex,
		MultiTraceWeaponParameters.WeaponName,
		MultiTraceWeaponParameters.WeaponBurstMode,
		MultiTraceWeaponParameters.WeaponOwner,
		MultiTraceWeaponParameters.MeshComponent,
		M_WorldSpawnedIn,
		MultiTraceWeaponParameters.FireSocketNames,
		MultiTraceWeaponParameters.bUseOwnerFireDirection,
		MultiTraceWeaponParameters.WeaponVFX,
		MultiTraceWeaponParameters.WeaponShellCase,
		MultiTraceWeaponParameters.BurstCooldown,
		MultiTraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		MultiTraceWeaponParameters.MinBurstAmount,
		MultiTraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		MultiTraceWeaponParameters.TraceProjectileType);

	WeaponState = MultiTrace;
	SetupRange();

	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(WeaponState, M_ProjectileManager.Get());
	}
}

void AInfantryWeaponMaster::SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters)
{
	SetOwningPlayer(LaserWeaponParameters.OwningPlayer);

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for inf weapon: " + GetName());
		return;
	}
	constexpr int32 WeaponIndex = 0;
	UWeaponStateLaser* LaserWeapon = NewObject<UWeaponStateLaser>(this);
	LaserWeapon->InitLaserWeapon(
		LaserWeaponParameters.OwningPlayer,
		WeaponIndex,
		LaserWeaponParameters.WeaponName,
		LaserWeaponParameters.WeaponOwner,
		LaserWeaponParameters.MeshComponent,
		LaserWeaponParameters.FireSocketName,
		GetWorld(),
		LaserWeaponParameters.LaserWeaponSettings);

	WeaponState = LaserWeapon;
	SetupRange();
}

void AInfantryWeaponMaster::SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters)
{
	SetOwningPlayer(LaserWeaponParameters.OwningPlayer);

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for inf weapon: " + GetName());
		return;
	}

	constexpr int32 WeaponIndex = 0;
	UWeaponStateMultiHitLaser* LaserWeapon = NewObject<UWeaponStateMultiHitLaser>(this);
	LaserWeapon->InitMultiHitLaserWeapon(
		LaserWeaponParameters.OwningPlayer,
		WeaponIndex,
		LaserWeaponParameters.WeaponName,
		LaserWeaponParameters.WeaponOwner,
		LaserWeaponParameters.MeshComponent,
		LaserWeaponParameters.FireSocketName,
		GetWorld(),
		LaserWeaponParameters.LaserWeaponSettings);

	WeaponState = LaserWeapon;
	SetupRange();
}

void AInfantryWeaponMaster::SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters)
{
	SetOwningPlayer(FlameWeaponParameters.OwningPlayer);

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for inf weapon: " + GetName());
		return;
	}

	constexpr int32 WeaponIndex = 0;

	UWeaponStateFlameThrower* FlameWeapon = NewObject<UWeaponStateFlameThrower>(this);
	FlameWeapon->InitFlameThrowerWeapon(
		FlameWeaponParameters.OwningPlayer,
		WeaponIndex,
		FlameWeaponParameters.WeaponName,
		FlameWeaponParameters.WeaponOwner,
		FlameWeaponParameters.MeshComponent,
		FlameWeaponParameters.FireSocketName,
		GetWorld(),
		FlameWeaponParameters.FlameSettings);

	WeaponState = FlameWeapon;
	SetupRange();
}

void AInfantryWeaponMaster::SetupOwner(AActor* InActor)
{
	if (not InActor || not InActor->IsA(ASquadUnit::StaticClass()))
	{
		RTSFunctionLibrary::ReportFailedCastError("InActor", "ASquadUnit", "AInfantryWeaponMaster::SetupOwner");
		return;
	}

	// Cast the incoming actor to ASquadUnit
	M_OwningSquadUnit = Cast<ASquadUnit>(InActor);
	if (not IsValid(M_OwningSquadUnit))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "M_OwningSquadUnit",
		                                                  "AInfantryWeaponMaster::SetupOwner");
		return;
	}

	// Retrieve the animation instance from the skeletal mesh component of the squad unit
	UAnimInstance* AnimInstance = M_OwningSquadUnit->GetMesh()->GetAnimInstance();
	if (not IsValid(AnimInstance))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "AnimInstance", "AInfantryWeaponMaster::SetupOwner");
		return;
	}
	M_SquadUnitAnimInstance = Cast<USquadUnitAnimInstance>(AnimInstance);
	if (not IsValid(M_SquadUnitAnimInstance))
	{
		RTSFunctionLibrary::ReportFailedCastError("AnimInstance", "USquadUnitAnimInstance",
		                                          "AInfantryWeaponMaster::SetupOwner");
		return;
	}
	M_SquadUnitAnimInstance->SetWeaponAimOffset(M_AimOffsetType);
}

bool AInfantryWeaponMaster::GetIsSquadUnitOwnerValid()
{
	if (IsValid(M_OwningSquadUnit))
	{
		return true;
	}
	return false;
}

void AInfantryWeaponMaster::ResetTarget()
{
	if (IsValid(M_SquadUnitAnimInstance))
	{
		M_SquadUnitAnimInstance->StopAiming();
	}
	M_TargetingData.ResetTarget();
}

void AInfantryWeaponMaster::SetupRange()
{
	if (not GetIsValidWeaponState())
	{
		return;
	}
	M_WeaponRangeData.RecalculateRangeFromWeapons(GetWeapons());
}

bool AInfantryWeaponMaster::GetIsTargetWithinAngleLimitsAndSetAO(const bool bInvalidateTargetOutOfReach)
{
	if (not IsSquadUnitAnimInstanceValid())
	{
		return false;
	}

	const FRotator OwnerRotation = M_OwningSquadUnit->GetActorRotation();
	const FVector OwnerForwardVector = OwnerRotation.Vector();

	// Direction from squad unit to the active target location (actor aim point or ground point).
	const FVector EnemyLocation = M_TargetingData.GetActiveTargetLocation();
	const FVector DirectionToTarget = (EnemyLocation - M_OwningSquadUnit->GetActorLocation()).GetSafeNormal();

	// Calculate the right vector of the owner to determine the sign of the angle
	const FVector OwnerRightVector = FRotationMatrix(OwnerRotation).GetUnitAxis(EAxis::Y);

	// Calculate the dot product to determine the cosine of the angle between the forward vector and the target direction
	const float DotProduct = FVector::DotProduct(OwnerForwardVector, DirectionToTarget);

	// Use atan2 to calculate the signed angle
	const float AngleToTarget = FMath::Atan2(FVector::DotProduct(OwnerRightVector, DirectionToTarget), DotProduct) *
		(180.0f / PI);

	// Get the current movement state from the animation instance
	const ESquadMovementAnimState CurrentMovementState = M_SquadUnitAnimInstance->GetMovementState();

	Debug_Weapons("This is the angle to the target: " + FString::SanitizeFloat(AngleToTarget));
	Debug_Weapons("This is the current movement state: " + UEnum::GetValueAsString(CurrentMovementState));

	switch (CurrentMovementState)
	{
	case ESquadMovementAnimState::Idle:
		return SetAO_Idle(AngleToTarget);

	case ESquadMovementAnimState::Walking:
		if (SetAO_Walking(AngleToTarget))
		{
			return true;
		}
		if (bInvalidateTargetOutOfReach)
		{
			// Target is outside the valid range, initiate a search for a new target
			ResetTarget();
			Debug_Weapons("Walking: target angle not accessible; resetting target");
		}
		return false;

	case ESquadMovementAnimState::Running:
		// Running: No engagement allowed
		M_SquadUnitAnimInstance->StopAiming();
		Debug_Weapons("Running: no engagement allowed");
		return false;

	default:
		return false;
	}
}

bool AInfantryWeaponMaster::SetAO_Idle(const float AngleToTarget) const
{
	// Idle: Full 360-degree aim offset allowed, also sets aiming to active
	if (FMath::Abs(AngleToTarget) > 90.0f)
	{
		// Use the signed angle to determine the new rotation
		const FRotator NewRotation = FRotator(0, AngleToTarget, 0) + M_OwningSquadUnit->GetActorRotation();
		M_OwningSquadUnit->SetActorRotation(NewRotation);

		// Set the aim offset to 0 as we are directly looking at the target now
		M_SquadUnitAnimInstance->SetAimOffsetAngle(0.0f);
		return true;
	}

	// If within 90 degrees, just set the aim offset angle
	M_SquadUnitAnimInstance->SetAimOffsetAngle(AngleToTarget);
	return true;
}

bool AInfantryWeaponMaster::SetAO_Walking(const float AngleToTarget) const
{
	// Walking: Limit to [-90, 90] degrees
	M_SquadUnitAnimInstance->SetAimOffsetAngle(AngleToTarget);
	if (AngleToTarget >= -90.0f && AngleToTarget <= 90.0f)
	{
		return true;
	}
	return false;
}

FVector AInfantryWeaponMaster::CalculateTargetDirection(const FVector& WeaponLocation)
{
	// Use the struct's active aim location (already includes any aim offset logic).
	const FVector AimpointWS = M_TargetingData.GetActiveTargetLocation();

	// Calculate the look-at direction vector
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(WeaponLocation, AimpointWS);
	const FVector LookAtDirection = LookAtRotation.Vector();

	if constexpr (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
	{
		// Calculate the end point for the debug line
		const FVector EndPoint = WeaponLocation + LookAtDirection * 1000;

		DrawDebugLine(
			GetWorld(),
			WeaponLocation,
			EndPoint,
			FColor::Red,
			false,
			1.f,
			0,
			10.0f
		);

		DrawDebugString(
			GetWorld(),
			WeaponLocation + FVector(0, 0, 50),
			FString::Printf(TEXT("Pitch: %.2f, Yaw: %.2f"), LookAtRotation.Pitch, LookAtRotation.Yaw),
			nullptr,
			FColor::White,
			1.0f,
			false
		);

		DrawDebugSphere(
			GetWorld(),
			AimpointWS,
			100.f,
			10,
			FColor::Blue,
			false,
			2.f,
			0,
			1);
	}

	return LookAtDirection;
}
