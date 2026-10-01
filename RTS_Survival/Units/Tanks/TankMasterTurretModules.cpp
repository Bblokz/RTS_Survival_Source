// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankMaster.h"

#include "Components/ChildActorComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace TankMasterTurretModuleHelpers
{
	// The child actor component name is fixed by the tank Blueprint, unlike spawn or timer order.
	FName GetTurretMountSortName(const ACPPTurretsMaster& Turret)
	{
		const UChildActorComponent* MountComponent = Turret.GetParentComponent();
		return IsValid(MountComponent) ? MountComponent->GetFName() : Turret.GetFName();
	}

	bool GetIsCancelledOrStaleTurret(const TWeakObjectPtr<ACPPTurretsMaster>& Entry,
	                                 const ACPPTurretsMaster* CancelledTurret)
	{
		return not Entry.IsValid() || Entry.Get() == CancelledTurret;
	}
}

// ----------------------------------------------------------------------------------------------------
// Turret notifications
// ----------------------------------------------------------------------------------------------------

void ATankMaster::AddPendingTurretModuleRegistration(ACPPTurretsMaster* Turret)
{
	if (not IsValid(Turret))
	{
		return;
	}
	const TWeakObjectPtr<ACPPTurretsMaster> TurretEntry(Turret);
	if (M_TurretModuleRegistration.ReadyTurrets.Contains(TurretEntry))
	{
		return;
	}
	M_TurretModuleRegistration.PendingTurrets.AddUnique(Turret);
}

void ATankMaster::OnTurretReadyForModuleRegistration(ACPPTurretsMaster* Turret)
{
	if (not IsValid(Turret))
	{
		return;
	}
	M_TurretModuleRegistration.PendingTurrets.Remove(Turret);
	M_TurretModuleRegistration.ReadyTurrets.AddUnique(Turret);
	ProcessReadyTurretModuleRegistrations();
}

void ATankMaster::CancelTurretModuleRegistration(const ACPPTurretsMaster* Turret)
{
	const auto IsCancelledOrStale = [Turret](const TWeakObjectPtr<ACPPTurretsMaster>& Entry)
	{
		return TankMasterTurretModuleHelpers::GetIsCancelledOrStaleTurret(Entry, Turret);
	};
	M_TurretModuleRegistration.PendingTurrets.RemoveAll(IsCancelledOrStale);
	M_TurretModuleRegistration.ReadyTurrets.RemoveAll(IsCancelledOrStale);
	const int32 RemovedRegisteredCount = M_TurretModuleRegistration.RegisteredTurrets.RemoveAll(IsCancelledOrStale);
	if (RemovedRegisteredCount > 0 && GetCanInstallTurretModules())
	{
		// A swapped or destroyed turret hands the tank's Weapon module to the largest remaining gun.
		UpdateAutoWeaponModule();
	}
	// The cancelled turret may have been the last one a ready batch or deferred load waited for.
	ProcessReadyTurretModuleRegistrations();
}

// ----------------------------------------------------------------------------------------------------
// Batching
// ----------------------------------------------------------------------------------------------------

bool ATankMaster::GetHasUnprocessedTurretModuleRegistrations()
{
	const auto IsStale = [](const TWeakObjectPtr<ACPPTurretsMaster>& Entry)
	{
		return not Entry.IsValid();
	};
	M_TurretModuleRegistration.PendingTurrets.RemoveAll(IsStale);
	M_TurretModuleRegistration.ReadyTurrets.RemoveAll(IsStale);
	return M_TurretModuleRegistration.PendingTurrets.Num() > 0 || M_TurretModuleRegistration.ReadyTurrets.Num() > 0;
}

bool ATankMaster::GetCanInstallTurretModules()
{
	if (not M_TurretModuleRegistration.bBlueprintModuleSetupComplete || not IsUnitAlive()
		|| IsActorBeingDestroyed())
	{
		return false;
	}
	return GetIsValidModuleArmor();
}

void ATankMaster::ScheduleInitialTurretDiscovery()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || IsActorBeingDestroyed())
	{
		return;
	}
	M_TurretModuleRegistration.InitialTurretDiscoveryTimer = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ATankMaster::DiscoverInitialTurretsAndTryFinalize));
}

void ATankMaster::DiscoverInitialTurretsAndTryFinalize()
{
	M_TurretModuleRegistration.InitialTurretDiscoveryTimer.Invalidate();
	if (not M_TurretModuleRegistration.bBlueprintModuleSetupComplete || IsActorBeingDestroyed())
	{
		return;
	}
	TArray<UChildActorComponent*> MountComponents;
	GetComponents<UChildActorComponent>(MountComponents);
	bool bIsWaitingForChildActor = false;
	for (UChildActorComponent* MountComponent : MountComponents)
	{
		if (not IsValid(MountComponent))
		{
			continue;
		}
		UClass* ChildActorClass = MountComponent->GetChildActorClass();
		if (not IsValid(ChildActorClass) || not ChildActorClass->IsChildOf(ACPPTurretsMaster::StaticClass()))
		{
			continue;
		}
		ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(MountComponent->GetChildActor());
		if (not IsValid(Turret))
		{
			const ACPPTurretsMaster* TurretDefaults = ChildActorClass->GetDefaultObject<ACPPTurretsMaster>();
			bIsWaitingForChildActor |= IsValid(TurretDefaults) && TurretDefaults->GetHasOwnVehicleModuleMount();
			continue;
		}
		if (Turret->GetHasOwnVehicleModuleMount())
		{
			AddPendingTurretModuleRegistration(Turret);
		}
	}
	if (bIsWaitingForChildActor)
	{
		ScheduleInitialTurretDiscovery();
		return;
	}
	M_TurretModuleRegistration.bInitialTurretsDiscovered = true;
	ProcessReadyTurretModuleRegistrations();
}

void ATankMaster::ProcessReadyTurretModuleRegistrations()
{
	if (not M_TurretModuleRegistration.bInitialTurretsDiscovered || not GetCanInstallTurretModules())
	{
		return;
	}
	const auto IsStale = [](const TWeakObjectPtr<ACPPTurretsMaster>& Entry)
	{
		return not Entry.IsValid();
	};
	M_TurretModuleRegistration.PendingTurrets.RemoveAll(IsStale);
	// Turrets of one spawn become ready in the same tick but in timer order; install them together.
	if (M_TurretModuleRegistration.PendingTurrets.Num() > 0)
	{
		return;
	}

	const TArray<ACPPTurretsMaster*> TurretsInMountOrder = TakeReadyTurretsInMountOrder();
	if (TurretsInMountOrder.Num() > 0)
	{
		RegisterTurretModulesInMountOrder(TurretsInMountOrder);
	}
	if (not bM_AreVehicleModulesInitialized)
	{
		FinalizeVehicleModulesAfterTurretRegistration();
	}
	if (bM_AreVehicleModulesInitialized)
	{
		ApplyDeferredVehicleModuleSaveData();
	}
}

TArray<ACPPTurretsMaster*> ATankMaster::TakeReadyTurretsInMountOrder()
{
	TArray<ACPPTurretsMaster*> TurretsInMountOrder;
	for (const TWeakObjectPtr<ACPPTurretsMaster>& ReadyTurret : M_TurretModuleRegistration.ReadyTurrets)
	{
		if (ReadyTurret.IsValid())
		{
			TurretsInMountOrder.Add(ReadyTurret.Get());
		}
	}
	M_TurretModuleRegistration.ReadyTurrets.Reset();

	// Module slot IDs follow installation order, so it must not depend on BeginPlay or timer order.
	TurretsInMountOrder.Sort([](const ACPPTurretsMaster& TurretA, const ACPPTurretsMaster& TurretB)
	{
		return TankMasterTurretModuleHelpers::GetTurretMountSortName(TurretA).Compare(
			TankMasterTurretModuleHelpers::GetTurretMountSortName(TurretB)) < 0;
	});
	return TurretsInMountOrder;
}

void ATankMaster::RegisterTurretModulesInMountOrder(const TArray<ACPPTurretsMaster*>& TurretsInMountOrder)
{
	for (ACPPTurretsMaster* Turret : TurretsInMountOrder)
	{
		if (RegisterModulesForTurret(Turret))
		{
			M_TurretModuleRegistration.RegisteredTurrets.AddUnique(Turret);
		}
	}
	UpdateAutoWeaponModule();

	// Installation after finalization does not dispatch batches; publish the new slots explicitly.
	if (not bM_AreVehicleModulesInitialized)
	{
		return;
	}
	RefreshMountedModuleBehaviours();
	RefreshModuleRepairCounts();
	if (GetIsValidHealthComponent())
	{
		HealthComponent->SynchronizeModuleIconSnapshot(M_ModuleArmor->GetModuleIconStates());
	}
}

// ----------------------------------------------------------------------------------------------------
// Per-turret installation
// ----------------------------------------------------------------------------------------------------

bool ATankMaster::RegisterModulesForTurret(ACPPTurretsMaster* Turret)
{
	if (not IsValid(Turret))
	{
		return false;
	}
	UMeshComponent* TurretMesh = Turret->GetModuleBindingMesh();
	// Module damage is routed through armor plates; a mount without registered armor stays module-less.
	if (not M_ModuleArmor->GetIsMeshRegisteredForArmor(TurretMesh))
	{
		return false;
	}
	EnsureTurretModuleForMesh(TurretMesh);
	AdoptDesignerWeaponModule(Turret, TurretMesh);
	return true;
}

void ATankMaster::EnsureTurretModuleForMesh(UMeshComponent* TurretMesh)
{
	// Installed earlier or by the tank Blueprint; never install a second module for one mount.
	if (M_ModuleArmor->FindModuleIdBoundToMesh(EVehicleModuleTypes::Turret, TurretMesh) != INDEX_NONE)
	{
		return;
	}
	const int32 OrphanedModuleId = M_ModuleArmor->FindOrphanedModuleIdOfType(EVehicleModuleTypes::Turret);
	if (OrphanedModuleId != INDEX_NONE)
	{
		// A swapped-in turret takes over the mount's module together with its damage state.
		(void)M_ModuleArmor->RebindModuleMesh(OrphanedModuleId, TurretMesh);
		return;
	}

	FVehicleModuleSetup TurretSetup;
	TurretSetup.Type = EVehicleModuleTypes::Turret;
	TurretSetup.BoundMesh = TurretMesh;
	int32 NewModuleId = INDEX_NONE;
	// Capacity and duplicate-mount failures are reported by SetupModule.
	(void)M_ModuleArmor->SetupModule(TurretSetup, NewModuleId);
}

void ATankMaster::AdoptDesignerWeaponModule(const ACPPTurretsMaster* Turret, UMeshComponent* TurretMesh)
{
	const int32 WeaponModuleId = M_ModuleArmor->FindFirstInstalledModuleIdOfType(EVehicleModuleTypes::Weapon);
	const bool bIsDesignerWeaponModule = WeaponModuleId != INDEX_NONE
		&& WeaponModuleId != M_TurretModuleRegistration.AutoWeaponModule.ModuleId;
	if (not bIsDesignerWeaponModule)
	{
		return;
	}
	const int32 SlotIndex = M_ModuleArmor->GetModuleSlotById(WeaponModuleId);
	const bool bIsUnidentifiedGunOnThisMount = M_ModuleArmor->GetBoundMeshForSlot(SlotIndex) == TurretMesh
		&& M_ModuleArmor->GetBoundWeaponForSlot(SlotIndex).IsExplicitlyNull();
	if (not bIsUnidentifiedGunOnThisMount)
	{
		return;
	}
	// The designer chose the mount; the largest gun on it replaces mesh matching, which fails on multi-gun turrets.
	UWeaponState* LargestWeapon = Turret->GetLargestCalibreWeapon();
	if (IsValid(LargestWeapon))
	{
		(void)M_ModuleArmor->RebindWeaponModule(WeaponModuleId, TurretMesh, LargestWeapon);
	}
}

// ----------------------------------------------------------------------------------------------------
// The tank's single Weapon module
// ----------------------------------------------------------------------------------------------------

void ATankMaster::UpdateAutoWeaponModule()
{
	FTankAutoWeaponModuleBinding& AutoWeaponModule = M_TurretModuleRegistration.AutoWeaponModule;
	const int32 WeaponModuleId = M_ModuleArmor->FindFirstInstalledModuleIdOfType(EVehicleModuleTypes::Weapon);
	if (WeaponModuleId != INDEX_NONE && WeaponModuleId != AutoWeaponModule.ModuleId)
	{
		// Designer-installed; only AdoptDesignerWeaponModule may refine it.
		return;
	}

	FTankAutoWeaponModuleBinding Candidate;
	if (not FindLargestRegisteredTurretWeapon(Candidate))
	{
		return;
	}
	// Equal calibres keep the current gun so twin mounts never trade the module back and forth.
	if (GetIsAutoWeaponModuleBindingCurrent() && Candidate.WeaponCalibre <= AutoWeaponModule.WeaponCalibre)
	{
		return;
	}

	UMeshComponent* CandidateMesh = Candidate.Turret->GetModuleBindingMesh();
	int32 TargetModuleId = WeaponModuleId;
	if (TargetModuleId == INDEX_NONE)
	{
		FVehicleModuleSetup WeaponSetup;
		WeaponSetup.Type = EVehicleModuleTypes::Weapon;
		WeaponSetup.BoundMesh = CandidateMesh;
		if (not M_ModuleArmor->SetupModule(WeaponSetup, TargetModuleId))
		{
			return;
		}
	}
	// Moving the module preserves its HP and state; it models the tank's main gun, not one weapon object.
	if (not M_ModuleArmor->RebindWeaponModule(TargetModuleId, CandidateMesh, Candidate.Weapon.Get()))
	{
		return;
	}
	Candidate.ModuleId = TargetModuleId;
	AutoWeaponModule = Candidate;
}

bool ATankMaster::FindLargestRegisteredTurretWeapon(FTankAutoWeaponModuleBinding& OutCandidate) const
{
	bool bFoundCandidate = false;
	for (const TWeakObjectPtr<ACPPTurretsMaster>& RegisteredTurret : M_TurretModuleRegistration.RegisteredTurrets)
	{
		const ACPPTurretsMaster* Turret = RegisteredTurret.Get();
		if (not IsValid(Turret) || not M_ModuleArmor->GetIsMeshRegisteredForArmor(Turret->GetModuleBindingMesh()))
		{
			continue;
		}
		UWeaponState* LargestWeapon = Turret->GetLargestCalibreWeapon();
		if (not IsValid(LargestWeapon))
		{
			continue;
		}
		const float WeaponCalibre = LargestWeapon->GetRawWeaponData().WeaponCalibre;
		// Strictly larger only: ties keep the earlier mount.
		if (bFoundCandidate && WeaponCalibre <= OutCandidate.WeaponCalibre)
		{
			continue;
		}
		OutCandidate.Turret = RegisteredTurret;
		OutCandidate.Weapon = LargestWeapon;
		OutCandidate.WeaponCalibre = WeaponCalibre;
		bFoundCandidate = true;
	}
	return bFoundCandidate;
}

bool ATankMaster::GetIsAutoWeaponModuleBindingCurrent() const
{
	const FTankAutoWeaponModuleBinding& AutoWeaponModule = M_TurretModuleRegistration.AutoWeaponModule;
	if (AutoWeaponModule.ModuleId == INDEX_NONE || not AutoWeaponModule.Weapon.IsValid())
	{
		return false;
	}
	const ACPPTurretsMaster* BoundTurret = AutoWeaponModule.Turret.Get();
	// A turret leaving the tank is still valid during its EndPlay, so registration membership decides.
	return IsValid(BoundTurret)
		&& M_TurretModuleRegistration.RegisteredTurrets.Contains(AutoWeaponModule.Turret)
		&& M_ModuleArmor->GetIsMeshRegisteredForArmor(BoundTurret->GetModuleBindingMesh());
}

// ----------------------------------------------------------------------------------------------------
// Deferred load
// ----------------------------------------------------------------------------------------------------

void ATankMaster::ApplyDeferredVehicleModuleSaveData()
{
	if (not M_TurretModuleRegistration.DeferredSaveData.IsSet())
	{
		return;
	}
	const FVehicleModuleSaveData SaveData = M_TurretModuleRegistration.DeferredSaveData.GetValue();
	M_TurretModuleRegistration.DeferredSaveData.Reset();
	if (not ImportVehicleModuleSaveData(SaveData) && IsUnitAlive())
	{
		RTSFunctionLibrary::ReportError(TEXT("Deferred vehicle module load failed after the turrets installed")
			TEXT(" their modules; the saved module setup does not match. Tank: ") + GetName());
	}
}
