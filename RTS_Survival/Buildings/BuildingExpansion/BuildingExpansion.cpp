// Copyright (C) Bas Blokzijl - All rights reserved.


#include "BuildingExpansion.h"

#include "BuildingExpansionEnums.h"
#include "NiagaraFunctionLibrary.h"
#include "BXPConstructionRules/BXPConstructionRules.h"
#include "Interface/BuildingExpansionOwner.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Buildings/EnergyComponent/BuildingExpansionEnergyComponent.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "RTS_Survival/Collapse/VerticalCollapse/FRTS_VerticalCollapse.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/RTSComponents/TimeProgressBarWidget.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

ABuildingExpansion::ABuildingExpansion(FObjectInitializer const& ObjectInitializer):
	Super(ObjectInitializer),
	BuildingMeshComponent(nullptr),
	M_ConstructionMesh(nullptr),
	M_BuildingMesh(nullptr),
	M_ProgressBarWidget(nullptr),
	M_BuildingTime(10),
	M_AmountSmokes(0),
	M_SmokeRadius(100.f),
	M_ConstructionAnimationMaterial(nullptr),
	M_Owner(nullptr),
	M_BuildingExpansionType(EBuildingExpansionType::BXT_Invalid),
	M_StatusBuildingExpansion(EBuildingExpansionStatus::BXS_NotUnlocked),
	M_MaterialIndex(0),
	M_TimeElapsedWhenConstructionCancelled(0), M_TargetActor(nullptr),
	bM_IsStandAlone(false)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	BuildingMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMeshComponent"));
	if (BuildingMeshComponent)
	{
		// set as root component
		RootComponent = BuildingMeshComponent;
	}
}

void ABuildingExpansion::GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const
{
	OutLocalOffsets = AimOffsetPoints;
}

void ABuildingExpansion::OnBuildingExpansionCreatedByOwner(const TScriptInterface<IBuildingExpansionOwner>& NewOwner,
                                                           const EBuildingExpansionStatus NewStatus)
{
	if (not NewOwner)
	{
		RTSFunctionLibrary::ReportError("Attempted to set null owner for BuildingExpansion: " + GetName()
			+ "\n At function ABuildingExpansion::OnBuildingExpansionCreatedByOwner in BuildingExpansion.cpp");
		return;
	}
	M_Owner = NewOwner;
	// Updates the Main game UI too.
	SetStatusAndPropagateToOwner(NewStatus);
}


void ABuildingExpansion::StartExpansionConstructionAtLocation(
	const FVector& Location,
	const FRotator& Rotation,
	const FName& AttachedToSocketName)
{
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}


	if (M_StatusBuildingExpansion == EBuildingExpansionStatus::BXS_LookingForPlacement)
	{
		OnNewConstructionLocationPropagateStatus(EBuildingExpansionStatus::BXS_BeingBuild, Rotation,
		                                         AttachedToSocketName);
	}
	else
	{
		// The building expansion was packed up and is now being unpacked at a new location.
		OnNewConstructionLocationPropagateStatus(EBuildingExpansionStatus::BXS_BeingUnpacked, Rotation,
		                                         AttachedToSocketName);
	}

	(void)SetActorLocation(Location);
	(void)SetActorRotation(Rotation);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Set the construction mesh on the building expansion.
	BuildingMeshComponent->SetStaticMesh(M_ConstructionMesh);

	// Caches the original materials and applies the construction materials to all slots.
	// Starts the progress bar and timer to reapply the original materials.
	StartConstructionAnimation(M_BuildingTime);
	BP_OnStartBxpConstruction();
}

void ABuildingExpansion::StartPackUpBuildingExpansion(const float TotalTime)
{
	DestroyBuildingAttachments();
	SetAllTurretsDisabled();
	SetMeshToConstructionMesh();
	if (M_StatusBuildingExpansion != EBuildingExpansionStatus::BXS_Built &&
		GetIsValidProgressBarWidget())
	{
		// Get elapesed time from the progress bar to restart the construction if packing is cancelled.
		M_TimeElapsedWhenConstructionCancelled = M_ProgressBarWidget->GetTimeElapsed();
		// The progressbar is still running from construction, stop it.
		M_ProgressBarWidget->StopProgressBar();
	}
	else
	{
		// The building expansion is already built, so we can cache all materials on the building mesh.
		CacheOriginalMaterials();
	}
	SetStatusAndPropagateToOwner(EBuildingExpansionStatus::BXS_BeingPackedUp);

	const int32 CachedMaterialCount = M_CachedOriginalMaterials.Num();
	if (CachedMaterialCount == 0)
	{
		StartFinishMaterialsTimer(TotalTime);
		OnBxpPackingUp.Broadcast();
		BP_OnStartPackUpBxp();
		return;
	}
	const float Interval = TotalTime / CachedMaterialCount;
	// Procedurally apply construction materials to material slots.
	StartAnimationTimer(Interval, false);
	OnBxpPackingUp.Broadcast();
	BP_OnStartPackUpBxp();
}

void ABuildingExpansion::CancelPackUpBuildingExpansion()
{
	const UWorld* World = GetWorld();
	if (not EnsureWorldIsValid(World, "ABuildingExpansion::CancelPackUpBuildingExpansion"))
	{
		return;
	}
	SpawnBuildingAttachments();
	SetMeshToBuildingMesh();
	World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
	if (M_TimeElapsedWhenConstructionCancelled != 0.f)
	{
		// This bxp was packed while being built, restart the construction.
		SetStatusAndPropagateToOwner(EBuildingExpansionStatus::BXS_BeingBuild);
		// We restart the construction animation, note that the materials are already cached!!
		StartConstructionAnimation(M_TimeElapsedWhenConstructionCancelled, false);
		OnBxpCancelPackingUp.Broadcast();
		BP_OnCancelledPackUpBxp();
	}
	else
	{
		if (GetIsValidBuildingMeshComponent())
		{
			for (int32 i = 0; i < M_CachedOriginalMaterials.Num(); ++i)
			{
				BuildingMeshComponent->SetMaterial(i, M_CachedOriginalMaterials[i]);
			}
		}
		ResetCachedMaterials();
		SetStatusAndPropagateToOwner(EBuildingExpansionStatus::BXS_Built);
		OnBxpCancelPackingUp.Broadcast();
		BP_OnCancelledPackUpBxp();
	}
	SetTurretsToAutoEngage();
}

void ABuildingExpansion::FinishPackUpExpansion()
{
	SetStatusAndPropagateToOwner(EBuildingExpansionStatus::BXS_PackedUp);
	BP_OnFinishedPackupBxp();
	Destroy();
}


TArray<UWeaponState*> ABuildingExpansion::GetAllWeapons() const
{
	TArray<UWeaponState*> ValidWeapons = {};
	for (auto EachTurret : M_TTurrets)
	{
		if (not EachTurret.IsValid())
		{
			continue;
		}
		for (auto EachWeapon : EachTurret->GetWeapons())
		{
			if (not IsValid(EachWeapon))
			{
				continue;
			}
			ValidWeapons.Add(EachWeapon);
		}
	}
	return ValidWeapons;
}

TArray<ACPPTurretsMaster*> ABuildingExpansion::GetTurrets() const
{
	TArray<ACPPTurretsMaster*> Turrets;
	Turrets.Reserve(M_TTurrets.Num());

	for (const TWeakObjectPtr<ACPPTurretsMaster>& EachTurret : M_TTurrets)
	{
		if (not EachTurret.IsValid())
		{
			continue;
		}

		Turrets.Add(EachTurret.Get());
	}

	return Turrets;
}

bool ABuildingExpansion::GetIsValidBehaviourComponent() const
{
	if (not IsValid(BehaviourComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "BehaviourComponent",
		                                             "ABuildingExpansion::GetIsValidBehaviourComponent");
		return false;
	}
	return true;
}

void ABuildingExpansion::VerticalDestruction(const FRTSVerticalCollapseSettings& CollapseSettings,
                                             const FCollapseFX& CollapseFX)
{
	TWeakObjectPtr<ABuildingExpansion> WeakThis(this);
	auto OnFinished = [WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnVerticalDestructionComplete();
	};
	FRTS_VerticalCollapse::StartVerticalCollapse(
		this,
		CollapseSettings,
		CollapseFX,
		OnFinished);
}

void ABuildingExpansion::SetupComponentCollisions(TArray<UMeshComponent*> MeshComponents,
                                                  TArray<UGeometryCollectionComponent*> GeometryComponents,
                                                  const bool bStaticMeshesAffectNavMesh) const
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	if (MeshComponents.Contains(BuildingMeshComponent))
	{
		RTSFunctionLibrary::ReportWarning("BuildingMeshComponent is already in the MeshComponents array!"
			"\n At function: ABuildingExpansion::SetupComponentCollisions"
			"\n For building expansion: " + GetName());
	}
	MeshComponents.Remove(BuildingMeshComponent);
	const int32 OwningPlayer = RTSComponent->GetOwningPlayer();
	FRTS_CollisionSetup::SetupBuildingExpansionCollision(BuildingMeshComponent, OwningPlayer,
	                                                     bStaticMeshesAffectNavMesh);
	for (const auto EachGeometryComp : GeometryComponents)
	{
		if (not IsValid(EachGeometryComp))
		{
			continue;
		}
		EachGeometryComp->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		EachGeometryComp->SetSimulatePhysics(false);
	}
}

void ABuildingExpansion::BeginDestroy()
{
	DestroyBuildingAttachments();
	(void)SetActorLocation(GetActorLocation() + (0.f, 0.f, 100));
	const UWorld* World = GetWorld();
	if (not EnsureWorldIsValid(World, "ABuildingExpansion:BeginDestroy"))
	{
		Super::BeginDestroy();
		return;
	}
	World->GetTimerManager().ClearTimer(M_BuildingTimerHandle);
	Super::BeginDestroy();
}

void ABuildingExpansion::BeginPlay()
{
	Super::BeginPlay();
}

void ABuildingExpansion::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	PostInit_GetCargoComponent();

	UBehaviourComp* BehaviourComp = FindComponentByClass<UBehaviourComp>();
	BehaviourComponent = BehaviourComp;
	(void)GetIsValidBehaviourComponent();
}

void ABuildingExpansion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (M_StatusBuildingExpansion == EBuildingExpansionStatus::BXS_PackedUp)
	{
		// packed up expansions already have their status set. They are expected to be destroyed at some point but we
		// do not need to update the bxp owner with that (packed status already set in bxp entry).
		return;
	}
	// only notify for “normal destruction,” not level unload, etc.
	if (EndPlayReason == EEndPlayReason::Destroyed && M_Owner && !bM_IsStandAlone)
	{
		// Never provide packed as true here as that would mean we allow to unpack a destroyed bxp with no costs.
		M_Owner->DestroyBuildingExpansion(this, false);
	}
}

void ABuildingExpansion::UnitDies(const ERTSDeathType DeathType)
{
	// do not call super as it will instantly destroy the actor.
	if (not IsUnitAlive())
	{
		return;
	}
	DisableCargoComponent();

	SetUnitDying();
	// Before bp event as the bp event may destroy the actor immediately.
	OnUnitDies.Broadcast();
	BP_OnUnitDies();
}

void ABuildingExpansion::DestroyAndSpawnActors(const FDestroySpawnActorsParameters& SpawnParams, FCollapseFX CollapseFX)
{
	Super::DestroyAndSpawnActors(SpawnParams, CollapseFX);
}

void ABuildingExpansion::SetTurretsToAutoEngage()
{
	for (auto EachTurret : M_TTurrets)
	{
		if (EachTurret.IsValid())
		{
			EachTurret->SetAutoEngageTargets(false);
		}
	}
	for (const auto EachHullWeapon : HullWeapons)
	{
		if (GetIsValidHullWeapon(EachHullWeapon))
		{
			EachHullWeapon->SetAutoEngageTargets(true);
		}
	}
}


bool ABuildingExpansion::GetIsValidHullWeapon(const UHullWeaponComponent* HullWeapon) const
{
	if (IsValid(HullWeapon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Hull weapon is not valid"
		"\n At function: ABuildingexpansion::GetIsValidHullWeapon"
		"\n For tank: " + GetName());
	return false;
}

void ABuildingExpansion::SetupTurret(ACPPTurretsMaster* NewTurret)
{
	M_TTurrets.Add(NewTurret);
}

void ABuildingExpansion::OnTurretInRange(ACPPTurretsMaster* CallingTurret)
{
}

void ABuildingExpansion::OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret)
{
	if (not IsValid(CallingTurret))
	{
		return;
	}
	CallingTurret->SetAutoEngageTargets(false);
}

void ABuildingExpansion::OnMountedWeaponTargetDestroyed(ACPPTurretsMaster* CallingTurret,
                                                        UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor,
                                                        const bool bWasDestroyedByOwnWeapons)
{
	if (not GetIsValidCommandData())
	{
		return;
	}
	if (CallingTurret)
	{
		OnTurretTargetDestroyed(CallingTurret, DestroyedActor);
		return;
	}
	if (CallingHullWeapon)
	{
		OnHullWeaponKilledActor(CallingHullWeapon, DestroyedActor);
	}
}

void ABuildingExpansion::OnFireWeapon(ACPPTurretsMaster* CallingTurret)
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	RTSComponent->SetUnitInCombat(true);
}

void ABuildingExpansion::OnProjectileHit(const bool bBounced)
{
}

int32 ABuildingExpansion::GetOwningPlayer()
{
	if (IsValid(RTSComponent))
	{
		return RTSComponent->GetOwningPlayer();
	}
	RTSFunctionLibrary::ReportNullErrorComponent(
		this,
		"RTSComponent",
		"ABuildingExpansion::GetOwningPlayer");
	return 0;
}

FRotator ABuildingExpansion::GetOwnerRotation() const
{
	return BuildingMeshComponent->GetComponentRotation();
}

void ABuildingExpansion::ExecuteAttackCommand(AActor* TargetActor)
{
	M_TargetActor = TargetActor;
	for (const auto EachTurret : M_TTurrets)
	{
		if (EachTurret.IsValid())
		{
			EachTurret->SetEngageSpecificTarget(TargetActor);
		}
	}
}

void ABuildingExpansion::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
	if (GroundLocation.IsNearlyZero())
	{
		DoneExecutingCommand(EAbilityID::IdAttackGround);
		return;
	}

	M_TargetActor = nullptr;

	for (const auto EachTurret : M_TTurrets)
	{
		if (EachTurret.IsValid())
		{
			EachTurret->SetEngageGroundLocation(GroundLocation);
		}
	}

	for (const auto EachHullWeapon : HullWeapons)
	{
		if (GetIsValidHullWeapon(EachHullWeapon))
		{
			EachHullWeapon->SetEngageGroundLocation(GroundLocation);
		}
	}
}

void ABuildingExpansion::TerminateAttackCommand()
{
	SetTurretsToAutoEngage();
}

void ABuildingExpansion::TerminateAttackGroundCommand()
{
	SetTurretsToAutoEngage();
}

void ABuildingExpansion::ExecuteAttachedWeaponAbilityCommand(
	const FVector TargetLocation, const EAttachWeaponAbilitySubType AttachedWeaponAbilityType)
{
	UAttachedWeaponAbilityComponent* AbilityComponent = FAbilityHelpers::GetAttachedWeaponAbilityComponent(
		AttachedWeaponAbilityType, this);
	if (not IsValid(AbilityComponent))
	{
		DoneExecutingCommand(EAbilityID::IdAttachedWeapon);
		return;
	}

	AbilityComponent->ExecuteAttachedWeaponAbility(TargetLocation);
	DoneExecutingCommand(EAbilityID::IdAttachedWeapon);
}

void ABuildingExpansion::TerminateAttachedWeaponAbilityCommand(
	const EAttachWeaponAbilitySubType AttachedWeaponAbilityType)
{
}

void ABuildingExpansion::OnVerticalDestructionComplete()
{
	BP_OnVerticalDestructionComplete();
}

FBxpData ABuildingExpansion::GetBxpData(const EBuildingExpansionType BxpSubType) const
{
	FBxpData BxpData;
	if (not GetIsValidRTSComponent())
	{
		return BxpData;
	}
	if (ACPPGameState* RTSGameState = FRTS_Statics::GetGameState(this))
	{
		const bool bOwnedByPlayer = RTSComponent->GetOwningPlayer() == 1;
		BxpData = bOwnedByPlayer
			          ? RTSGameState->GetPlayerBxpData(BxpSubType)
			          : RTSGameState->GetEnemyBxpData(BxpSubType);
	}
	return BxpData;
}

bool ABuildingExpansion::GetHasValidEnergyComponent() const
{
	if (IsValid(M_BuildingExpansionEnergyComponent))
	{
		return true;
	}
	return false;
}

void ABuildingExpansion::OnInit_FindEnergyComponent(const int32 MyEnergy)
{
	UBuildingExpansionEnergyComponent* EnergyComp = FindComponentByClass<UBuildingExpansionEnergyComponent>();
	if (not IsValid(EnergyComp))
	{
		RTSFunctionLibrary::ReportError("The building expansion: " + GetName() +
			"\n Could not find a building expansion energy component eventhough its energy amount per bxp data is not zero!"
			"\n Energy amount: " + FString::FromInt(MyEnergy));
		return;
	}
	M_BuildingExpansionEnergyComponent = EnergyComp;
	M_BuildingExpansionEnergyComponent->InitEnergyComponent(MyEnergy);
	M_BuildingExpansionEnergyComponent->SetEnabled(true);
}

void ABuildingExpansion::PostInit_GetCargoComponent()
{
	CargoComponent = FindComponentByClass<UCargo>();
}

bool ABuildingExpansion::GetHasValidCargoComponent() const
{
	if (not IsValid(CargoComponent))
	{
		return false;
	}
	return true;
}

void ABuildingExpansion::DisableCargoComponent() const
{
	if (GetHasValidCargoComponent())
	{
		CargoComponent->SetIsEnabled(false);
	}
}

void ABuildingExpansion::DisableAllWeapons()
{
	for (auto eachTurret : M_TTurrets)
	{
		if (eachTurret.IsValid())
		{
			eachTurret->DisableTurret();
		}
	}
	for (auto EachHullWeapon : HullWeapons)
	{
		if (GetIsValidHullWeapon(EachHullWeapon))
		{
			EachHullWeapon->DisableHullWeapon();
		}
	}
}

void ABuildingExpansion::CollapseMesh(UGeometryCollectionComponent* GeoCollapseComp,
                                      TSoftObjectPtr<UGeometryCollection> GeoCollection, UMeshComponent* MeshToCollapse,
                                      FCollapseDuration CollapseDuration, FCollapseForce CollapseForce,
                                      FCollapseFX CollapseFX)
{
	FRTS_Collapse::CollapseMesh(this, GeoCollapseComp, GeoCollection, MeshToCollapse, CollapseDuration, CollapseForce,
	                            CollapseFX);
}

void ABuildingExpansion::InitBuildingExpansion(
	UStaticMesh* NewConstructionMesh,
	UStaticMesh* NewBuildingMesh,
	UTimeProgressBarWidget* NewProgressBar,
	TArray<UNiagaraSystem*> SmokeSystems,
	const int NewAmountSmokes,
	const float NewSmokeRadius,
	UMaterialInstance* NewConstructionAnimationMaterial,
	EBuildingExpansionType NewBuildingExpansionType,
	TArray<FBuildingAttachment> NewBuildingAttachments, const bool bIsStandAlone,
	const bool bLetBuildingMeshAffectNavMesh)
{
	const FBxpData MyData = GetBxpData(NewBuildingExpansionType);
	if (GetIsValidFowComponent())
	{
		FowComponent->SetVisionRadius(MyData.VisionRadius);
	}
	if (MyData.EnergySupply != 0)
	{
		OnInit_FindEnergyComponent(MyData.EnergySupply);
	}
	SetUnitAbilitiesRunTime(MyData.Abilities);
	OnInitBuildingExpansion_SetupCollision(bLetBuildingMeshAffectNavMesh);
	if (GetIsValidHealthComponent())
	{
		HealthComponent->InitHealthAndResistance(MyData.ResistancesAndDamageMlt, MyData.Health);
	}
	if (IsValid(NewConstructionMesh))

	{
		M_ConstructionMesh = NewConstructionMesh;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "NewConstructionMesh",
		                                             "ABuildingExpansion::InitBuildingExpansion");
	}
	if (IsValid(NewBuildingMesh))
	{
		M_BuildingMesh = NewBuildingMesh;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "NewBuildingMesh",
		                                             "ABuildingExpansion::InitBuildingExpansion");
	}
	if (IsValid(NewProgressBar))
	{
		M_ProgressBarWidget = NewProgressBar;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "NewProgressBar",
		                                             "ABuildingExpansion::InitBuildingExpansion");
	}
	if (MyData.ConstructionTime > 0.f)
	{
		M_BuildingTime = MyData.ConstructionTime;
	}
	else
	{
		M_BuildingTime = 10.f;
	}
	M_SmokeSystems = SmokeSystems;
	M_AmountSmokes = NewAmountSmokes;
	if (NewSmokeRadius > 0.f)
	{
		M_SmokeRadius = NewSmokeRadius;
	}
	else
	{
		M_SmokeRadius = 100.f;
	}
	if (IsValid(NewConstructionAnimationMaterial))
	{
		M_ConstructionAnimationMaterial = NewConstructionAnimationMaterial;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "NewConstructionAnimationMaterial",
		                                             "ABuildingExpansion::InitBuildingExpansion");
	}

	M_BuildingExpansionType = NewBuildingExpansionType;
	M_BuildingAttachments = NewBuildingAttachments;
	bM_IsStandAlone = bIsStandAlone;
	if (bM_IsStandAlone)
	{
		OnBxpStandAlone();
	}
}

void ABuildingExpansion::OnFinishedExpansionConstruction()
{
	if (not GetIsValidProgressBarWidget())
	{
		return;
	}
	SetStatusAndPropagateToOwner(EBuildingExpansionStatus::BXS_Built);

	M_ProgressBarWidget->StopProgressBar();
	// No need to reconstruct when cancelling packing; this bxp is completely built.
	M_TimeElapsedWhenConstructionCancelled = 0.f;
	SpawnBuildingAttachments();
	SetTurretsToAutoEngage();
	SetMeshToBuildingMesh();
	OnBxpConstructed.Broadcast();
	BP_OnFinishedExpansionConstruction();
}


void ABuildingExpansion::SpawnBuildingAttachments()
{
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (not EnsureWorldIsValid(World, "ABuildingExpansion::SpawnBuildingAttachments"))
	{
		return;
	}
	for (const auto& [ActorToSpawn, SocketName, Scale] : M_BuildingAttachments)
	{
		if (not ActorToSpawn)
		{
			continue;
		}
		// Attempt to get the transform of the specified socket
		FTransform SocketTransform;
		if (not BuildingMeshComponent->DoesSocketExist(SocketName) ||
			not(SocketTransform = BuildingMeshComponent->GetSocketTransform(SocketName,
			                                                                ERelativeTransformSpace::RTS_World)).
			IsValid())
		{
			continue;
		}

		// Spawn the actor at the socket's location and orientation
		AActor* SpawnedActor = World->SpawnActor<AActor>(ActorToSpawn,
		                                                 SocketTransform.GetLocation(),
		                                                 SocketTransform.GetRotation().Rotator());
		if (not SpawnedActor)
		{
			continue;
		}
		// Attach it to the BuildingMeshComponent at the specified socket
		SpawnedActor->AttachToComponent(BuildingMeshComponent,
		                                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		                                SocketName);
		SpawnedActor->SetActorScale3D(Scale);
		M_SpawnedAttachments.Add(SpawnedActor);
	}
}

void ABuildingExpansion::DestroyBuildingAttachments()
{
	// Iterate over all the spawned attachment actors and destroy them
	for (auto SpawnedActor : M_SpawnedAttachments)
	{
		if (SpawnedActor.IsValid())
		{
			SpawnedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			SpawnedActor->Destroy();
		}
	}
	M_SpawnedAttachments.Empty();
}


void ABuildingExpansion::SetStatusAndPropagateToOwner(const EBuildingExpansionStatus NewStatus)
{
	M_StatusBuildingExpansion = NewStatus;
	// Propagate status change to the owner and to the UI if needed.
	if (M_Owner)
	{
		M_Owner->UpdateExpansionData(this);
	}
}

void ABuildingExpansion::OnNewConstructionLocationPropagateStatus(const EBuildingExpansionStatus NewStatus,
                                                                  const FRotator& NewRotation,
                                                                  const FName& NewSocketName)
{
	M_StatusBuildingExpansion = NewStatus;
	// Propagate status change to the owner and to the UI if needed.
	if (M_Owner)
	{
		M_Owner->OnBxpConstructionStartUpdateExpansionData(
			this,
			NewRotation,
			NewSocketName);
	}
}

void ABuildingExpansion::CacheOriginalMaterials()
{
	ResetCachedMaterials();
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	for (int32 i = 0; i < BuildingMeshComponent->GetNumMaterials(); ++i)
	{
		M_CachedOriginalMaterials.Add(BuildingMeshComponent->GetMaterial(i));
	}
}

void ABuildingExpansion::ApplyConstructionMaterial() const
{
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	if (not GetIsValidConstructionAnimationMaterial())
	{
		return;
	}
	for (int32 i = 0; i < BuildingMeshComponent->GetNumMaterials(); ++i)
	{
		BuildingMeshComponent->SetMaterial(i, M_ConstructionAnimationMaterial);
	}
}

void ABuildingExpansion::ApplyCachedMaterials()
{
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	if (M_StatusBuildingExpansion == EBuildingExpansionStatus::BXS_BeingPackedUp)
	{
		if (M_MaterialIndex < 0)
		{
			FinishReapplyingMaterials();
			return;
		}
		if (not GetIsValidConstructionAnimationMaterial())
		{
			FinishReapplyingMaterials();
			return;
		}
		BuildingMeshComponent->SetMaterial(M_MaterialIndex, M_ConstructionAnimationMaterial);
		CreateSmokeSystems(M_AmountSmokes + FMath::RandRange(-1, 2));
		M_MaterialIndex--;
	}
	else if (M_MaterialIndex < M_CachedOriginalMaterials.Num())
	{
		if (BuildingMeshComponent)
		{
			BuildingMeshComponent->SetMaterial(M_MaterialIndex, M_CachedOriginalMaterials[M_MaterialIndex]);
			CreateSmokeSystems(M_AmountSmokes + FMath::RandRange(-1, 2));
			M_MaterialIndex++;
		}
	}
	else
	{
		FinishReapplyingMaterials();
	}
}

void ABuildingExpansion::FinishReapplyingMaterials()
{
	const UWorld* World = GetWorld();
	if (not EnsureWorldIsValid(World, "ABuildingExpansion::FinishReapplyingMaterials"))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
	// Empty the cached materials and reset the index.
	ResetCachedMaterials();
	if (M_StatusBuildingExpansion == EBuildingExpansionStatus::BXS_BeingBuild ||
		M_StatusBuildingExpansion == EBuildingExpansionStatus::BXS_BeingUnpacked)
	{
		// We played a construction animation, finish the building.
		OnFinishedExpansionConstruction();
	}
}

void ABuildingExpansion::ResetCachedMaterials()
{
	M_CachedOriginalMaterials.Empty();
	M_MaterialIndex = 0;
}

void ABuildingExpansion::CreateSmokeSystems(const int AmountSystemsToSpawn) const
{
	const FVector BaseLocation = GetActorLocation();
	const FVector BaseScale = FVector(1.f, 1.f, 1.f);
	for (int i = 0; i < AmountSystemsToSpawn; ++i)
	{
		// Randomly select a location within the radius of the building expansion
		FVector RandomLocationRadius = BaseLocation + FVector(FMath::RandRange(-M_SmokeRadius, M_SmokeRadius),
		                                                      FMath::RandRange(-M_SmokeRadius, M_SmokeRadius),
		                                                      0);
		// With 20% variability in scale.
		FVector RandomScale = BaseScale + FVector(FMath::RandRange(-0.2f, 0.2f),
		                                          FMath::RandRange(-0.2f, 0.2f),
		                                          FMath::RandRange(-0.2f, 0.2f));
		// Rotate away from the center of the building expansion.
		CreateRandomSmokeSystemAtTransform(FTransform(
			FQuat(UKismetMathLibrary::FindLookAtRotation(BaseLocation, RandomLocationRadius)),
			RandomLocationRadius, RandomScale));
	}
}

void ABuildingExpansion::CreateRandomSmokeSystemAtTransform(const FTransform Transform) const
{
	UWorld* World = GetWorld();
	if (M_SmokeSystems.Num() == 0 || not World)
	{
		// No smoke systems are available
		return;
	}

	// Randomly select a smoke system from the array
	UNiagaraSystem* SelectedSystem = M_SmokeSystems[FMath::RandRange(0, M_SmokeSystems.Num() - 1)];
	if (not SelectedSystem)
	{
		RTSFunctionLibrary::ReportError("Selected an invalid smoke system! \n Actor: " + GetName());
		return;
	}
	// Spawn the system at the transform's location, using its rotation
	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, SelectedSystem, Transform.GetLocation(), Transform.GetRotation().Rotator(),
		Transform.GetScale3D(), true, true, ENCPoolMethod::AutoRelease, true);
}


void ABuildingExpansion::StartConstructionAnimation(
	const float BuildingTime,
	const bool bCacheOriginalMaterials)
{
	if (bCacheOriginalMaterials)
	{
		// Cache the original materials of the building mesh.
		CacheOriginalMaterials();
	}
	// Applies construction materials to all slots.
	ApplyConstructionMaterial();

	if (GetIsValidProgressBarWidget())
	{
		// Start the progress bar.
		M_ProgressBarWidget->StartProgressBar(BuildingTime);
	}
	// Start timer to reapply original materials.
	const int32 CachedMaterialCount = M_CachedOriginalMaterials.Num();
	if (CachedMaterialCount == 0)
	{
		StartFinishMaterialsTimer(BuildingTime);
		return;
	}
	const float Interval = BuildingTime / CachedMaterialCount;
	StartAnimationTimer(Interval, true);
}

void ABuildingExpansion::StartAnimationTimer(const float Interval, const bool bStartAtMaterialZero)
{
	M_MaterialIndex = bStartAtMaterialZero ? 0 : M_CachedOriginalMaterials.Num() - 1;
	CreateSmokeSystems(2);
	const UWorld* World = GetWorld();
	if (not EnsureWorldIsValid(World, "ABuildingExpansion::StartAnimationTimer"))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
	World->GetTimerManager().SetTimer(MaterialReapplyTimerHandle, this,
	                                  &ABuildingExpansion::ApplyCachedMaterials,
	                                  Interval, true);
}

void ABuildingExpansion::OnBxpStandAlone()
{
	for (const auto Turret : M_TTurrets)
	{
		if (Turret.IsValid())
		{
			Turret->SetAutoEngageTargets(false);
		}
	}
}

void ABuildingExpansion::SetAllTurretsDisabled()
{
	for (auto EachTurret : M_TTurrets)
	{
		if (EachTurret.IsValid())
		{
			EachTurret->DisableTurret();
		}
	}
}

void ABuildingExpansion::SetMeshToBuildingMesh()
{
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	if (not GetIsValidBuildingMesh())
	{
		return;
	}
	// Set the new static mesh
	BuildingMeshComponent->SetStaticMesh(M_BuildingMesh);

	// Now ensure the materials of the new building mesh are applied correctly
	int32 NumMaterials = M_BuildingMesh->GetStaticMaterials().Num();
	// Get the number of materials in the building mesh
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		if (UMaterialInterface* BuildingMaterial = M_BuildingMesh->GetMaterial(i))
		{
			// Apply each material of the building mesh to the corresponding material slot
			BuildingMeshComponent->SetMaterial(i, BuildingMaterial);
		}
	}
}

void ABuildingExpansion::SetMeshToConstructionMesh()
{
	// Set the construction mesh back on the building expansion.
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	if (not GetIsValidConstructionMesh())
	{
		return;
	}
	BuildingMeshComponent->SetStaticMesh(M_ConstructionMesh);

	// Ensure that the construction materials are applied correctly
	int32 NumMaterials = M_ConstructionMesh->GetStaticMaterials().Num();
	// Get the number of materials in the construction mesh
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		UMaterialInterface* ConstructionMaterial = M_ConstructionMesh->GetMaterial(i);
		if (ConstructionMaterial)
		{
			// Apply each material of the construction mesh to the corresponding material slot
			BuildingMeshComponent->SetMaterial(i, ConstructionMaterial);
		}
	}
}

bool ABuildingExpansion::EnsureWorldIsValid(const UWorld* World, const FString& FunctionName) const
{
	if (not IsValid(World))
	{
		return false;
	}
	return true;
}

bool ABuildingExpansion::DidKillTargetActorOrTargetNoLongerValid(AActor* TargetActor, AActor* KilledActor)
{
	if (TargetActor != nullptr && TargetActor == KilledActor)
	{
		// The target was destroyed.
		return true;
	}
	return not RTSFunctionLibrary::RTSIsValid(TargetActor);
}

bool ABuildingExpansion::GetIsValidBuildingMeshComponent() const
{
	if (IsValid(BuildingMeshComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"BuildingMeshComponent",
		"GetIsValidBuildingMeshComponent",
		this);
	return false;
}

bool ABuildingExpansion::GetIsValidProgressBarWidget() const
{
	if (M_ProgressBarWidget.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_ProgressBarWidget",
		"GetIsValidProgressBarWidget",
		this);
	return false;
}

bool ABuildingExpansion::GetIsValidConstructionMesh() const
{
	if (IsValid(M_ConstructionMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_ConstructionMesh",
		"GetIsValidConstructionMesh",
		this);
	return false;
}

bool ABuildingExpansion::GetIsValidBuildingMesh() const
{
	if (IsValid(M_BuildingMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_BuildingMesh",
		"GetIsValidBuildingMesh",
		this);
	return false;
}

bool ABuildingExpansion::GetIsValidConstructionAnimationMaterial() const
{
	if (IsValid(M_ConstructionAnimationMaterial))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_ConstructionAnimationMaterial",
		"GetIsValidConstructionAnimationMaterial",
		this);
	return false;
}

void ABuildingExpansion::StartFinishMaterialsTimer(const float TotalTime)
{
	const UWorld* World = GetWorld();
	if (not EnsureWorldIsValid(World, "ABuildingExpansion::StartFinishMaterialsTimer"))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
	if (TotalTime <= 0.f)
	{
		FinishReapplyingMaterials();
		return;
	}
	World->GetTimerManager().SetTimer(MaterialReapplyTimerHandle, this,
	                                  &ABuildingExpansion::FinishReapplyingMaterials,
	                                  TotalTime, false);
}

void ABuildingExpansion::OnTurretTargetDestroyed(ACPPTurretsMaster* CallingTurret, AActor* DestroyedActor)
{
	if (not IsValid(CallingTurret))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "CallingTurret",
		                                             "ABuildingExpansion::OnTurretTargetDestroyed");
		return;
	}
	if (UnitCommandData->GetCurrentActiveCommand() == EAbilityID::IdAttack)
	{
		if (DidKillTargetActorOrTargetNoLongerValid(M_TargetActor, DestroyedActor))
		{
			// The target was destroyed, so we can stop the attack command; will auto engage all turrets.
			DoneExecutingCommand(EAbilityID::IdAttack);
			return;
		}
		// The turret killed an actor that was not the main target.
		CallingTurret->SetEngageSpecificTarget(M_TargetActor);
		return;
	}
	// No attack command active, so turret can auto engage.
	CallingTurret->SetAutoEngageTargets(true);
}


void ABuildingExpansion::OnHullWeaponKilledActor(UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor)
{
	if (not GetIsValidHullWeapon(CallingHullWeapon))
	{
		return;
	}
	if (UnitCommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack)
	{
		if (M_TargetActor == DestroyedActor)
		{
			// The target was destroyed, so we can stop the attack command.
			DoneExecutingCommand(EAbilityID::IdAttack);
			return;
		}
		if (RTSFunctionLibrary::RTSIsValid(M_TargetActor))
		{
			// The turret killed an actor that was not the main target.
			CallingHullWeapon->SetEngageSpecificTarget(M_TargetActor);
		}
		else
		{
			// The target was destroyed, so we can stop the attack command; will auto engage all turrets.
			DoneExecutingCommand(EAbilityID::IdAttack);
		}
	}
	else
	{
		// No attack command active, so turret can auto engage.
		CallingHullWeapon->SetAutoEngageTargets(true);
	}
}

void ABuildingExpansion::OnInitBuildingExpansion_SetupCollision(const bool bLetBuildingComponentAffectNavmesh) const
{
	if (not GetIsValidBuildingMeshComponent())
	{
		return;
	}
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	int32 OwningPlayer = RTSComponent->GetOwningPlayer();
	FRTS_CollisionSetup::SetupBuildingExpansionCollision(BuildingMeshComponent, OwningPlayer,
	                                                     bLetBuildingComponentAffectNavmesh);
}

void ABuildingExpansion::BeginPlay_NextFrameInitAbilities()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	TWeakObjectPtr<ABuildingExpansion> WeakThis(this);
	auto SetAbilitiesLambda = [WeakThis]()-> void
	{
		if (not WeakThis.IsValid() || not WeakThis.Get()->GetIsValidRTSComponent())
		{
			return;
		}
		URTSComponent* RTSComp = WeakThis.Get()->GetRTSComponent();
		if (RTSComp->GetUnitType() != EAllUnitType::UNType_BuildingExpansion)
		{
			RTSFunctionLibrary::ReportError(
				"Cannot init abililties for building expansion: " + WeakThis.Get()->GetName() +
				"\n Building expansion is not of type BuildingExpansion. \n Ensure the building expansion is spawned correctly.");
			return;
		}
		bool bIsValidData = false;
		const EBuildingExpansionType MyBxpType = RTSComp->GetSubtypeAsBxpSubtype();
		if (RTSComp->GetOwningPlayer() == 1)
		{
			const FBxpData BxpData = FRTS_Statics::BP_GetPlayerBxpData(MyBxpType,
			                                                           WeakThis.Get(), bIsValidData);
			WeakThis.Get()->InitAbilityArray(BxpData.Abilities);
		}
		else
		{
			const FBxpData BxpData = FRTS_Statics::BP_GetEnemyBxpData(MyBxpType,
			                                                          WeakThis.Get(), bIsValidData);
			WeakThis.Get()->InitAbilityArray(BxpData.Abilities);
		}
		if (not bIsValidData)
		{
			RTSFunctionLibrary::ReportError("Could not get valid data for buiding expansion when setting the abilities"
				"\n in function ABuildingExpansion::BeginPlay_NextFrameInitAbilities"
				"\n for expansion: " + WeakThis.Get()->GetName() +
				"\n Ensure the data is set correctly in the GameState");
		}
	};
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindLambda(SetAbilitiesLambda);
	// Fire next frame.
	World->GetTimerManager().SetTimerForNextTick(TimerDelegate);
}

void ABuildingExpansion::DebugDisplayMessage(const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString(Message));
	}
}
