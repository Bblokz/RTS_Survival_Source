// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "EnergyComp.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/LowEnergyBehaviour.h"
#include "RTS_Survival/Buildings/EnergyComponent/Settings/RTSEnergyGameSettings.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values for this component's properties
UEnergyComp::UEnergyComp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnergyComp::SetEnabled(const bool bEnabled)
{
	bM_IsEnabled = bEnabled;

	UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (PlayerResourceManager)
	{
		if (bEnabled)
		{
			PlayerResourceManager->RegisterEnergyComponent(this);
			return;
		}
		PlayerResourceManager->DeregisterEnergyComponent(this);
		return;
	}
	RTSFunctionLibrary::ReportError("Failed to get PlayerResourceManager in EnergyComp.cpp"
		"\n at function SetEnabled in EnergyComp.cpp."
		"\n for component: " + GetName());
}


void UEnergyComp::UpgradeEnergy(const int32 Amount)
{
	UpdateEnergySupplied(M_Energy + Amount);
}

void UEnergyComp::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CacheLowEnergySettings();
	if (bStartEnabled)
	{
		// Register with the player resource manager.
		SetEnabled(true);
	}
	BeginPlay_InitializeLowEnergyStrategy();
	BeginPlay_CheckInitialLowEnergyState();
}

void UEnergyComp::BeginDestroy()
{
	bM_IsShuttingDown = true;
	if (bM_IsEnabled)
	{
		UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
		if (PlayerResourceManager)
		{
			PlayerResourceManager->DeregisterEnergyComponent(this);
		}
	}
	Super::BeginDestroy();
}

void UEnergyComp::InitEnergyComponent(const int32 Energy)
{
	UpdateEnergySupplied(Energy);
}

void UEnergyComp::CacheOwnerMaterials()
{
	CacheMeshesFromActor(GetOwner());
}

void UEnergyComp::UpdateEnergySupplied(const int32 NewEnergySupplied)
{
	const int32 OldEnergy = M_Energy;
	M_Energy = NewEnergySupplied;
	// Only when the component is enabled, it is registered.
	if (bM_IsEnabled)
	{
		UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
		if (PlayerResourceManager)
		{
			// Provide the old energy supply too so we know what amount to subtract before we add the new amount.
			PlayerResourceManager->UpdateEnergySupplyForUpgradedComponent(this, OldEnergy);
			return;
		}
		RTSFunctionLibrary::ReportError("Failed to get PlayerResourceManager in EnergyComp.cpp"
			"\n at function UpdateEnergySupplied in EnergyComp.cpp."
			"\n for component: " + GetName());
	}
}

void UEnergyComp::OnBaseEnergyChange(const bool bIsLowPower)
{
	if (GetIsShuttingDown())
	{
		return;
	}

	if (bIsLowPower == bM_IsLowEnergy)
	{
		ReportDuplicateEnergyState(bIsLowPower);
		return;
	}

	bM_IsLowEnergy = bIsLowPower;

	if (GetDoesStrategyUseMaterials() && GetShouldApplyMaterialChanges())
	{
		ApplyMaterialsForCurrentEnergyState();
	}

	if (GetDoesStrategyUseBehaviour())
	{
		if (bIsLowPower)
		{
			ApplyLowEnergyBehaviour();
			return;
		}
		RemoveLowEnergyBehaviour();
	}
}

void UEnergyComp::BeginPlay_CacheLowEnergySettings()
{
	const URTSEnergyGameSettings* Settings = GetDefault<URTSEnergyGameSettings>();
	if (Settings == nullptr)
	{
		return;
	}

	M_LowEnergyMaterial = Settings->GetLowEnergyMaterial();
	M_LowEnergyBehaviourClass = Settings->GetLowEnergyBehaviourClass();
}

void UEnergyComp::BeginPlay_InitializeLowEnergyStrategy()
{
	DetermineLowEnergyStrategy();

	if (GetDoesStrategyUseMaterials())
	{
		CacheOwnerMaterials();
	}

	if (GetDoesStrategyUseBehaviour())
	{
		CacheOwnerBehaviourComponent();
	}
}

void UEnergyComp::BeginPlay_CheckInitialLowEnergyState()
{
	UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (PlayerResourceManager == nullptr)
	{
		return;
	}

	const bool bIsLowPower = PlayerResourceManager->GetPlayerEnergySupply() < PlayerResourceManager->GetPlayerEnergyDemand();
	if (bIsLowPower)
	{
		OnBaseEnergyChange(true);
	}
}

void UEnergyComp::DetermineLowEnergyStrategy()
{
	ELowEnergyStrategy DeterminedStrategy = ELowEnergyStrategy::LES_None;

	if (Cast<ANomadicVehicle>(GetOwner()) != nullptr)
	{
		DeterminedStrategy = ELowEnergyStrategy::LES_ApplyBehaviourOnly;
	}
	else if (Cast<ABuildingExpansion>(GetOwner()) != nullptr)
	{
		DeterminedStrategy = ELowEnergyStrategy::LES_ApplyBehaviourAndChangeMaterials;
	}
	else if (Cast<ATankMaster>(GetOwner()) != nullptr)
	{
		DeterminedStrategy = ELowEnergyStrategy::LES_ApplyBehaviourAndChangeMaterials;
	}

	M_LowEnergyStrategy = OverwriteLowEnergyStrategy != ELowEnergyStrategy::LES_Invalid
		? OverwriteLowEnergyStrategy
		: DeterminedStrategy;
}

void UEnergyComp::CacheOwnerBehaviourComponent()
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	M_OwnerBehaviourComponent = OwnerActor->FindComponentByClass<UBehaviourComp>();
}

void UEnergyComp::ApplyLowEnergyBehaviour()
{
	if (bM_HasAppliedLowEnergyBehaviour)
	{
		return;
	}

	if (not GetIsValidLowEnergyBehaviourClass())
	{
		return;
	}

	if (not GetIsOwnerBehaviourComponentValid())
	{
		return;
	}

	M_OwnerBehaviourComponent->AddBehaviour(M_LowEnergyBehaviourClass);
	bM_HasAppliedLowEnergyBehaviour = true;
}

void UEnergyComp::RemoveLowEnergyBehaviour()
{
	if (not bM_HasAppliedLowEnergyBehaviour)
	{
		return;
	}

	if (not GetIsOwnerBehaviourComponentValid())
	{
		return;
	}

	M_OwnerBehaviourComponent->RemoveBehaviour(M_LowEnergyBehaviourClass);
	bM_HasAppliedLowEnergyBehaviour = false;
}

void UEnergyComp::CacheMeshComponentMaterials(UMeshComponent* MeshComponent)
{
	if (MeshComponent == nullptr)
	{
		return;
	}

	FEnergyMeshMaterialCache MaterialCache;
	MaterialCache.MeshComponent = MeshComponent;

	const int32 MaterialCount = MeshComponent->GetNumMaterials();
	MaterialCache.OriginalMaterials.Reserve(MaterialCount);

	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		MaterialCache.OriginalMaterials.Add(MeshComponent->GetMaterial(Index));
	}

	M_CachedMeshMaterials.Add(MoveTemp(MaterialCache));
}

void UEnergyComp::CacheMeshesFromActor(AActor* ActorToCache)
{
	if (ActorToCache == nullptr)
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	ActorToCache->GetComponents(MeshComponents);

	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		CacheMeshComponentMaterials(MeshComponent);
	}
}

void UEnergyComp::ApplyLowEnergyMaterials()
{
	if (not GetIsValidLowEnergyMaterial())
	{
		return;
	}

	if (M_CachedMeshMaterials.Num() == 0)
	{
		ReportNoCachedMeshes(TEXT("ApplyLowEnergyMaterials"));
		return;
	}

	for (const FEnergyMeshMaterialCache& MeshCache : M_CachedMeshMaterials)
	{
		if (not MeshCache.MeshComponent.IsValid())
		{
			continue;
		}

		UMeshComponent* MeshComponent = MeshCache.MeshComponent.Get();
		const int32 CurrentMaterialCount = MeshComponent->GetNumMaterials();
		const int32 CachedMaterialCount = MeshCache.OriginalMaterials.Num();

		if (CurrentMaterialCount > CachedMaterialCount)
		{
			ReportInvalidMaterialCache(MeshComponent, CachedMaterialCount, CurrentMaterialCount,
			                           TEXT("ApplyLowEnergyMaterials"));
		}

		for (int32 Index = 0; Index < CurrentMaterialCount; ++Index)
		{
			MeshComponent->SetMaterial(Index, M_LowEnergyMaterial);
		}
	}
}

void UEnergyComp::RestoreOriginalMaterials()
{
	if (M_CachedMeshMaterials.Num() == 0)
	{
		ReportNoCachedMeshes(TEXT("RestoreOriginalMaterials"));
		return;
	}

	for (const FEnergyMeshMaterialCache& MeshCache : M_CachedMeshMaterials)
	{
		if (not MeshCache.MeshComponent.IsValid())
		{
			continue;
		}

		UMeshComponent* MeshComponent = MeshCache.MeshComponent.Get();
		const int32 CurrentMaterialCount = MeshComponent->GetNumMaterials();
		const int32 CachedMaterialCount = MeshCache.OriginalMaterials.Num();

		if (CurrentMaterialCount < CachedMaterialCount)
		{
			ReportInvalidMaterialCache(MeshComponent, CachedMaterialCount, CurrentMaterialCount,
			                           TEXT("RestoreOriginalMaterials"));
		}

		const int32 MaterialsToApply = FMath::Min(CurrentMaterialCount, CachedMaterialCount);
		for (int32 Index = 0; Index < MaterialsToApply; ++Index)
		{
			MeshComponent->SetMaterial(Index, MeshCache.OriginalMaterials[Index]);
		}
	}
}

void UEnergyComp::ApplyMaterialsForCurrentEnergyState()
{
	if (bM_IsLowEnergy)
	{
		ApplyLowEnergyMaterials();
		return;
	}

	RestoreOriginalMaterials();
}

bool UEnergyComp::GetIsOwnerBehaviourComponentValid() const
{
	return M_OwnerBehaviourComponent.IsValid();
}

bool UEnergyComp::GetIsValidLowEnergyMaterial() const
{
	if (not IsValid(M_LowEnergyMaterial))
	{
		if (GetIsShuttingDown())
		{
			return false;
		}

		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_LowEnergyMaterial"),
			TEXT("GetIsValidLowEnergyMaterial"),
			this
		);

		return false;
	}

	return true;
}

bool UEnergyComp::GetIsValidLowEnergyBehaviourClass() const
{
	if (M_LowEnergyBehaviourClass != nullptr)
	{
		return true;
	}

	if (GetIsShuttingDown())
	{
		return false;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_LowEnergyBehaviourClass"),
		TEXT("GetIsValidLowEnergyBehaviourClass"),
		this
	);

	return false;
}

bool UEnergyComp::GetShouldApplyMaterialChanges() const
{
	return true;
}

bool UEnergyComp::GetIsShuttingDown() const
{
	if (bM_IsShuttingDown)
	{
		return true;
	}

	const AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return true;
	}

	return OwnerActor->IsActorBeingDestroyed();
}

bool UEnergyComp::GetDoesStrategyUseMaterials() const
{
	return M_LowEnergyStrategy == ELowEnergyStrategy::LES_ChangeMaterialsOnly
		|| M_LowEnergyStrategy == ELowEnergyStrategy::LES_ApplyBehaviourAndChangeMaterials;
}

bool UEnergyComp::GetDoesStrategyUseBehaviour() const
{
	return M_LowEnergyStrategy == ELowEnergyStrategy::LES_ApplyBehaviourOnly
		|| M_LowEnergyStrategy == ELowEnergyStrategy::LES_ApplyBehaviourAndChangeMaterials;
}

void UEnergyComp::ReportInvalidMaterialCache(const UMeshComponent* MeshComponent, const int32 CachedMaterialCount,
                                             const int32 CurrentMaterialCount, const FString& Context) const
{
	if (GetIsShuttingDown())
	{
		return;
	}

	const FString MeshName = MeshComponent != nullptr ? MeshComponent->GetName() : TEXT("InvalidMesh");
	RTSFunctionLibrary::ReportError(
		"Energy component material cache mismatch in " + Context +
		"\n Mesh: " + MeshName +
		"\n Cached Materials: " + FString::FromInt(CachedMaterialCount) +
		"\n Current Materials: " + FString::FromInt(CurrentMaterialCount) +
		"\n Component: " + GetName());
}

void UEnergyComp::ReportNoCachedMeshes(const FString& Context) const
{
	if (GetIsShuttingDown())
	{
		return;
	}

	RTSFunctionLibrary::ReportError(
		"Energy component found no cached meshes in " + Context +
		"\n Component: " + GetName());
}

void UEnergyComp::ReportDuplicateEnergyState(const bool bAttemptedLowEnergy) const
{
	if (GetIsShuttingDown())
	{
		return;
	}

	const FString DesiredState = bAttemptedLowEnergy ? TEXT("LowEnergy") : TEXT("EnergyRestored");
	RTSFunctionLibrary::ReportError(
		"Energy component received duplicate state change: " + DesiredState +
		"\n Component: " + GetName());
}
