// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ArmorCalculation.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

using namespace VehicleModuleBalance;

FVehicleModuleSaveData UArmorCalculation::ExportModuleState() const
{
	FVehicleModuleSaveData SaveData;
	SaveData.RuleVersion = RuleVersion;
	SaveData.Profile = M_ModuleProfile;
	SaveData.RunningGear = M_RunningGear;
	SaveData.DefaultImpactSerial = static_cast<int32>(M_DefaultImpactSerial);
	SaveData.Modules.Reserve(M_InstalledModuleCount);

	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		const FVehicleModule& Module = M_Modules[SlotIndex];
		if (not Module.bInstalled)
		{
			continue;
		}
		FVehicleModuleSavedInstance& SavedInstance = SaveData.Modules.AddDefaulted_GetRef();
		SavedInstance.ModuleId = Module.ModuleId;
		SavedInstance.Type = Module.Type;
		SavedInstance.HealthFraction = Module.MaxHp > 0.f ? FMath::Clamp(Module.CurrentHp / Module.MaxHp, 0.f, 1.f) : 1.f;
		AppendSavedCoverageForSlot(SlotIndex, SaveData);
	}
	return SaveData;
}

void UArmorCalculation::AppendSavedCoverageForSlot(const int32 SlotIndex, FVehicleModuleSaveData& OutSaveData) const
{
	// Coverage is saved by stable mesh registration slot and plate type, never by component pointer.
	const int32 ModuleId = M_Modules[SlotIndex].ModuleId;
	const uint64 CoverageMask = M_ModuleBindings[SlotIndex].CoveredPlateMask;
	for (int32 PlateRegistrationId = 0; PlateRegistrationId < MaxPlateBindings; ++PlateRegistrationId)
	{
		const int32 ArmorMeshSlot = PlateRegistrationId / MaxArmorPlatesPerRegisteredMesh;
		const FArmorSettings* ArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
		if ((CoverageMask & (uint64(1) << PlateRegistrationId)) == 0 || ArmorSettings == nullptr)
		{
			continue;
		}
		const EArmorPlate PlateType = ArmorSettings[PlateRegistrationId % MaxArmorPlatesPerRegisteredMesh].ArmorType;
		const bool bAlreadySaved = OutSaveData.Coverage.ContainsByPredicate(
			[ModuleId, ArmorMeshSlot, PlateType](const FVehicleModuleSavedCoverage& Existing)
			{
				return Existing.ModuleId == ModuleId && Existing.ArmorMeshSlot == ArmorMeshSlot
					&& Existing.PlateType == PlateType;
			});
		if (bAlreadySaved)
		{
			continue;
		}
		FVehicleModuleSavedCoverage& SavedCoverage = OutSaveData.Coverage.AddDefaulted_GetRef();
		SavedCoverage.ModuleId = ModuleId;
		SavedCoverage.ArmorMeshSlot = ArmorMeshSlot;
		SavedCoverage.PlateType = PlateType;
	}
}

bool UArmorCalculation::ImportModuleState(const FVehicleModuleSaveData& SaveData)
{
	if (not GetIsValidSaveData(SaveData))
	{
		return false;
	}

	(void)TryApplySavedCoverage(SaveData);
	M_DefaultImpactSerial = static_cast<uint32>(FMath::Max(0, SaveData.DefaultImpactSerial));
	ApplyImportedModuleHealth(SaveData);
	return true;
}

bool UArmorCalculation::GetIsValidSaveData(const FVehicleModuleSaveData& SaveData) const
{
	const FString OwnerName = GetNameSafe(GetOwner());
	if (not bM_AreModulesFinalized || bM_IsMutatingModules || bM_IsDispatchingModuleChanges)
	{
		RTSFunctionLibrary::ReportError(TEXT("ImportModuleState: modules must be finalized and idle before load on ")
			+ OwnerName);
		return false;
	}
	const bool bSupportedRuleVersion = SaveData.RuleVersion == RuleVersion
		|| SaveData.RuleVersion == DesignerAssignedModuleIdRuleVersion;
	if (not bSupportedRuleVersion || SaveData.Profile != M_ModuleProfile
		|| SaveData.RunningGear != M_RunningGear || SaveData.Modules.Num() != M_InstalledModuleCount)
	{
		RTSFunctionLibrary::ReportError(TEXT("ImportModuleState: saved rules, profile, running gear or module count")
			TEXT(" do not match the installed setup on ") + OwnerName);
		return false;
	}

	uint32 SeenSlotMask = 0;
	TSet<int32> SeenModuleIds;
	for (const FVehicleModuleSavedInstance& SavedInstance : SaveData.Modules)
	{
		const int32 SlotIndex = GetSlotForSavedModuleId(SaveData, SavedInstance.ModuleId);
		const bool bValidFraction = FMath::IsFinite(SavedInstance.HealthFraction)
			&& SavedInstance.HealthFraction >= 0.f && SavedInstance.HealthFraction <= 1.f;
		if (SavedInstance.ModuleId < 0 || SlotIndex == INDEX_NONE
			|| M_Modules[SlotIndex].Type != SavedInstance.Type || not bValidFraction
			|| SeenModuleIds.Contains(SavedInstance.ModuleId)
			|| (SeenSlotMask & (1u << SlotIndex)) != 0)
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("ImportModuleState: saved module %d does not match an installed module on %s."),
				SavedInstance.ModuleId, *OwnerName));
			return false;
		}
		SeenModuleIds.Add(SavedInstance.ModuleId);
		SeenSlotMask |= 1u << SlotIndex;
	}
	return true;
}

int32 UArmorCalculation::GetSlotForSavedModuleId(const FVehicleModuleSaveData& SaveData,
	                                               const int32 SavedModuleId) const
{
	if (SaveData.RuleVersion == RuleVersion)
	{
		return GetModuleSlotById(SavedModuleId);
	}

	// The legacy export walked installed slots in order, so its array position identifies each old ID.
	const int32 SavedModuleIndex = SaveData.Modules.IndexOfByPredicate(
		[SavedModuleId](const FVehicleModuleSavedInstance& SavedInstance)
		{
			return SavedInstance.ModuleId == SavedModuleId;
		});
	if (SavedModuleIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 InstalledModuleIndex = 0;
	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		if (not M_Modules[SlotIndex].bInstalled)
		{
			continue;
		}
		if (InstalledModuleIndex == SavedModuleIndex)
		{
			return SlotIndex;
		}
		++InstalledModuleIndex;
	}
	return INDEX_NONE;
}

bool UArmorCalculation::TryApplySavedCoverage(const FVehicleModuleSaveData& SaveData)
{
	uint64 NewCoverageBySlot[MaxModuleInstances] = {};
	bool bHasUnresolvedBySlot[MaxModuleInstances] = {};
	bool bAllResolved = true;
	for (const FVehicleModuleSavedCoverage& SavedCoverage : SaveData.Coverage)
	{
		const int32 SlotIndex = GetSlotForSavedModuleId(SaveData, SavedCoverage.ModuleId);
		if (SlotIndex == INDEX_NONE || M_Modules[SlotIndex].Type != EVehicleModuleTypes::AddOnArmor)
		{
			bAllResolved = false;
			continue;
		}
		if (GetArmorSettingsForSlot(SavedCoverage.ArmorMeshSlot) == nullptr)
		{
			// Unresolved coverage stays disabled and is reported until the mesh registers.
			bHasUnresolvedBySlot[SlotIndex] = true;
			bAllResolved = false;
			continue;
		}
		NewCoverageBySlot[SlotIndex] |= GetCoverageMaskForPlateType(SavedCoverage.ArmorMeshSlot,
		                                                            SavedCoverage.PlateType);
	}

	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		if (not M_Modules[SlotIndex].bInstalled || M_Modules[SlotIndex].Type != EVehicleModuleTypes::AddOnArmor)
		{
			continue;
		}
		M_ModuleBindings[SlotIndex].CoveredPlateMask = NewCoverageBySlot[SlotIndex];
		M_ModuleBindings[SlotIndex].bHasUnresolvedCoverage = bHasUnresolvedBySlot[SlotIndex];
	}
	RebuildPlateModuleBindings();
	RefreshArmorContributionsAndRearCache();
	if (not bAllResolved)
	{
		(void)ValidateModuleSetup();
	}
	return bAllResolved;
}

void UArmorCalculation::ApplyImportedModuleHealth(const FVehicleModuleSaveData& SaveData)
{
	{
		TGuardValue<bool> MutationGuard(bM_IsMutatingModules, true);
		for (const FVehicleModuleSavedInstance& SavedInstance : SaveData.Modules)
		{
			const int32 SlotIndex = GetSlotForSavedModuleId(SaveData, SavedInstance.ModuleId);
			// HP is derived from the current tank MaxHealth; the fraction is the saved quantity.
			SetModuleHealth(SlotIndex, M_Modules[SlotIndex].MaxHp * SavedInstance.HealthFraction,
			                EModuleChangeCause::Load, EWeaponShellType::Shell_None);
		}
	}
	// Load reconciliation publishes behaviours/icons/card but never replays Blueprint transition events.
	DispatchModuleChangesAfterMutation(TakePendingModuleChanges());
}
