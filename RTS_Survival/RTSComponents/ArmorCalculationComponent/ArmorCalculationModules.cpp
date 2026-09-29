// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ArmorCalculation.h"

#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

using namespace VehicleModuleBalance;

namespace ArmorCalculationModuleHelpers
{
	bool GetIsLeftHullSidePlate(const EArmorPlate Plate)
	{
		return Plate == EArmorPlate::Plate_SideLeft || Plate == EArmorPlate::Plate_SideLowerLeft;
	}

	bool GetIsRightHullSidePlate(const EArmorPlate Plate)
	{
		return Plate == EArmorPlate::Plate_SideRight || Plate == EArmorPlate::Plate_SideLowerRight;
	}

	uint64 MakePlateBit(const int32 PlateRegistrationId)
	{
		return uint64(1) << PlateRegistrationId;
	}
}

namespace ArmorCalculationModuleDebug
{
	constexpr float PrintDurationSeconds = 4.f;
	constexpr float FirstPrintHeight = 300.f;
	constexpr float PrintHeightStep = 100.f;
	constexpr float PrintTextScale = 1.f;
	const FColor DarkGreen(0, 100, 0);

	void PrintModuleEvent(ATankMaster* Tank, const FString& Message, const FColor Color)
	{
		if (not IsValid(Tank))
		{
			return;
		}
		UWorld* World = Tank->GetWorld();
		if (not IsValid(World))
		{
			return;
		}

		static TMap<TWeakObjectPtr<ATankMaster>, TArray<double>> PrintExpiryByTank;
		for (auto Iterator = PrintExpiryByTank.CreateIterator(); Iterator; ++Iterator)
		{
			if (not Iterator.Key().IsValid())
			{
				Iterator.RemoveCurrent();
			}
		}

		const TWeakObjectPtr<ATankMaster> TankKey(Tank);
		TArray<double>& LineExpiryTimes = PrintExpiryByTank.FindOrAdd(TankKey);
		const double NowSeconds = World->GetTimeSeconds();
		int32 LineIndex = INDEX_NONE;
		for (int32 CandidateIndex = 0; CandidateIndex < LineExpiryTimes.Num(); ++CandidateIndex)
		{
			if (LineExpiryTimes[CandidateIndex] <= NowSeconds)
			{
				LineIndex = CandidateIndex;
				break;
			}
		}
		if (LineIndex == INDEX_NONE)
		{
			LineIndex = LineExpiryTimes.Add(NowSeconds + PrintDurationSeconds);
		}
		else
		{
			LineExpiryTimes[LineIndex] = NowSeconds + PrintDurationSeconds;
		}
		const float Height = FirstPrintHeight + PrintHeightStep * LineIndex;
		DrawDebugString(World, Tank->GetActorLocation() + FVector(0.f, 0.f, Height), Message,
		                nullptr, Color, PrintDurationSeconds, false, PrintTextScale);
	}

	FString GetModuleLabel(const FVehicleModule& Module)
	{
		return FString::Printf(TEXT("%s [%d]"), *UEnum::GetValueAsString(Module.Type), Module.ModuleId);
	}

	void PrintModuleHit(ATankMaster* Tank, const FVehicleModule& Module, const float AppliedDamage)
	{
		PrintModuleEvent(Tank,
		                 FString::Printf(TEXT("%s hit: %.1f module damage"), *GetModuleLabel(Module), AppliedDamage),
		                 FColor::Orange);
	}

	void PrintModuleStateChange(ATankMaster* Tank, const FVehicleModule& Module,
	                            const EVehicleModuleState PreviousState, const EModuleChangeCause Cause)
	{
		if (Cause == EModuleChangeCause::Load)
		{
			return;
		}
		const FString ModuleLabel = GetModuleLabel(Module);
		if (Cause == EModuleChangeCause::Damage)
		{
			if (Module.State == EVehicleModuleState::Destroyed)
			{
				PrintModuleEvent(Tank, ModuleLabel + TEXT(" destroyed"), FColor::Red);
			}
			else if (Module.State == EVehicleModuleState::Damaged)
			{
				PrintModuleEvent(Tank, ModuleLabel + TEXT(" yellow"), FColor::Yellow);
			}
			return;
		}
		if (PreviousState == EVehicleModuleState::Destroyed && Module.State == EVehicleModuleState::Damaged)
		{
			PrintModuleEvent(Tank, ModuleLabel + TEXT(" repaired: red to yellow"), FColor::Green);
		}
		else if (Module.State == EVehicleModuleState::Healthy)
		{
			PrintModuleEvent(Tank, ModuleLabel + TEXT(" fully repaired"), DarkGreen);
		}
	}
}

// ----------------------------------------------------------------------------------------------------
// Validators
// ----------------------------------------------------------------------------------------------------

bool UArmorCalculation::GetIsValidModuleTank() const
{
	if (M_ModuleTank.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ModuleTank",
		"GetIsValidModuleTank",
		this
	);
	return false;
}

bool UArmorCalculation::GetCanChangeModuleConfiguration(const TCHAR* FunctionName) const
{
	if (not bM_HasModuleInstallationStarted)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(FString(FunctionName)
		+ TEXT(": vehicle module profile and running gear must be selected before installing modules.")
		+ TEXT("\n Owner: ") + GetNameSafe(GetOwner()));
	return false;
}

// ----------------------------------------------------------------------------------------------------
// Profile, running gear and installation
// ----------------------------------------------------------------------------------------------------

bool UArmorCalculation::SetVehicleModuleProfile(const EVehicleModuleProfile Profile)
{
	if (not GetCanChangeModuleConfiguration(TEXT("SetVehicleModuleProfile")))
	{
		return false;
	}

	const int32 ProfileIndex = GetProfileIndex(Profile);
	if (ProfileIndex < 0 || ProfileIndex >= VehicleProfileCount)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetVehicleModuleProfile: invalid profile on ") + GetNameSafe(GetOwner()));
		return false;
	}

	M_ModuleProfile = Profile;
	if (not bM_HasExplicitRunningGear)
	{
		M_RunningGear = ModuleProfileRules[ProfileIndex].DefaultRunningGear;
	}
	return true;
}

bool UArmorCalculation::SetRunningGearType(const EVehicleRunningGear RunningGear)
{
	if (not GetCanChangeModuleConfiguration(TEXT("SetRunningGearType")))
	{
		return false;
	}

	const int32 RunningGearIndex = GetRunningGearIndex(RunningGear);
	if (RunningGearIndex < 0 || RunningGearIndex >= RunningGearTypeCount)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetRunningGearType: invalid running gear on ") + GetNameSafe(GetOwner()));
		return false;
	}

	M_RunningGear = RunningGear;
	bM_HasExplicitRunningGear = true;
	return true;
}

bool UArmorCalculation::SetupModule(const FVehicleModuleSetup& Setup, int32& OutModuleId)
{
	OutModuleId = INDEX_NONE;
	if (not GetIsValidModuleSetupInput(Setup))
	{
		return false;
	}

	const int32 SlotIndex = FindFreeSlotForSetup(Setup);
	if (SlotIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("SetupModule: no free slot for type %s on %s (duplicate side, mount or capacity)."),
			*UEnum::GetValueAsString(Setup.Type), *GetNameSafe(GetOwner())));
		return false;
	}

	InitializeModuleSlot(SlotIndex, Setup, GetArmorMeshSlot(Setup.BoundMesh));
	bM_HasModuleInstallationStarted = true;
	++M_InstalledModuleCount;
	RebuildPlateModuleBindings();
	OutModuleId = SlotIndex;
	return true;
}

bool UArmorCalculation::GetIsValidModuleSetupInput(const FVehicleModuleSetup& Setup) const
{
	const FString OwnerName = GetNameSafe(GetOwner());
	if (not GetIsInstallableModuleType(Setup.Type))
	{
		RTSFunctionLibrary::ReportError(TEXT("SetupModule: the type cannot be None on ") + OwnerName);
		return false;
	}
	if (bM_IsMutatingModules || bM_IsDispatchingModuleChanges)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetupModule: cannot install modules during a module batch on ") + OwnerName);
		return false;
	}
	if (GetIsRunningGearType(Setup.Type))
	{
		const EVehicleModuleTypes SelectedGearType = M_RunningGear == EVehicleRunningGear::Wheels
			                                             ? EVehicleModuleTypes::Wheels
			                                             : EVehicleModuleTypes::Tracks;
		if (Setup.Type != SelectedGearType)
		{
			RTSFunctionLibrary::ReportError(TEXT("SetupModule: running gear type does not match the tank's selected")
				TEXT(" running gear (tracks and wheels cannot be mixed) on ") + OwnerName);
			return false;
		}
		return true;
	}
	const bool bRequiresMeshBinding = Setup.Type == EVehicleModuleTypes::Turret
		|| Setup.Type == EVehicleModuleTypes::Weapon;
	if (bRequiresMeshBinding && GetArmorMeshSlot(Setup.BoundMesh) == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetupModule: turret and weapon modules need a BoundMesh registered with")
			TEXT(" InitArmorCalculation first. Owner: ") + OwnerName);
		return false;
	}
	return true;
}

int32 UArmorCalculation::FindFreeSlotForSetup(const FVehicleModuleSetup& Setup) const
{
	if (GetIsRunningGearType(Setup.Type))
	{
		const int32 SideSlot = Setup.bRightSide ? RightRunningGearSlot : LeftRunningGearSlot;
		return M_Modules[SideSlot].bInstalled ? INDEX_NONE : SideSlot;
	}

	const int32 FirstSlot = GetFirstSlotForType(Setup.Type);
	const int32 EndSlot = FirstSlot + GetSlotCountForType(Setup.Type);
	const int32 BoundMeshSlot = GetArmorMeshSlot(Setup.BoundMesh);
	int32 FreeSlot = INDEX_NONE;
	for (int32 SlotIndex = FirstSlot; SlotIndex < EndSlot; ++SlotIndex)
	{
		if (not M_Modules[SlotIndex].bInstalled)
		{
			FreeSlot = FreeSlot == INDEX_NONE ? SlotIndex : FreeSlot;
			continue;
		}
		// Two turrets cannot share one mount.
		if (Setup.Type == EVehicleModuleTypes::Turret && M_ModuleBindings[SlotIndex].ArmorMeshSlot == BoundMeshSlot)
		{
			return INDEX_NONE;
		}
	}
	return FreeSlot;
}

void UArmorCalculation::InitializeModuleSlot(const int32 SlotIndex, const FVehicleModuleSetup& Setup,
                                             const int32 ArmorMeshSlot)
{
	FVehicleModule& Module = M_Modules[SlotIndex];
	Module = FVehicleModule();
	Module.ModuleId = SlotIndex;
	Module.Type = Setup.Type;
	Module.bInstalled = true;
	// Modules installed after finalization start at full health; earlier ones receive HP on finalization.
	Module.MaxHp = bM_AreModulesFinalized ? GetModuleMaxHpForType(Setup.Type, M_TankMaxHealth) : 0.f;
	Module.CurrentHp = Module.MaxHp;
	Module.State = EVehicleModuleState::Healthy;

	FVehicleModuleBinding& Binding = M_ModuleBindings[SlotIndex];
	Binding = FVehicleModuleBinding();
	Binding.bRightSide = Setup.bRightSide;
	const bool bUsesMeshBinding = Setup.Type == EVehicleModuleTypes::Turret || Setup.Type == EVehicleModuleTypes::Weapon;
	if (bUsesMeshBinding)
	{
		Binding.ArmorMeshSlot = ArmorMeshSlot;
		Binding.BoundMesh = Setup.BoundMesh;
	}
	M_AddOnContributionBySlot[SlotIndex] = 1.f;
}

float UArmorCalculation::GetModuleMaxHpForType(const EVehicleModuleTypes Type, const float TankMaxHealth) const
{
	return TankMaxHealth * GetModuleHealthMultiplier(M_ModuleProfile, Type);
}

bool UArmorCalculation::ValidateModuleSetup() const
{
	bool bIsValid = true;
	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		const FVehicleModule& Module = M_Modules[SlotIndex];
		if (not Module.bInstalled)
		{
			continue;
		}
		const bool bUsesMeshBinding = Module.Type == EVehicleModuleTypes::Turret
			|| Module.Type == EVehicleModuleTypes::Weapon;
		const FVehicleModuleBinding& Binding = M_ModuleBindings[SlotIndex];
		if (bUsesMeshBinding && GetArmorMeshForSlot(Binding.ArmorMeshSlot) != Binding.BoundMesh.Get())
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Vehicle module %d on %s is bound to a mesh that is no longer registered for armor."),
				Module.ModuleId, *GetNameSafe(GetOwner())));
			bIsValid = false;
		}
		if (Binding.bHasUnresolvedCoverage)
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Add-on armor module %d on %s has saved coverage for an unregistered mesh; coverage disabled."),
				Module.ModuleId, *GetNameSafe(GetOwner())));
			bIsValid = false;
		}
	}
	return bIsValid;
}

bool UArmorCalculation::FinalizeVehicleModuleSetup(ATankMaster* Tank, const float TankMaxHealth)
{
	if (not IsValid(Tank) || not FMath::IsFinite(TankMaxHealth) || TankMaxHealth <= 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("FinalizeVehicleModuleSetup: requires a valid tank and positive MaxHealth.")
			TEXT(" Module setup stays disabled on ") + GetNameSafe(GetOwner()));
		return false;
	}
	if (bM_AreModulesFinalized)
	{
		RecalculateModuleMaxHealth(TankMaxHealth);
		return true;
	}

	M_ModuleTank = Tank;
	M_TankMaxHealth = TankMaxHealth;
	for (FVehicleModule& Module : M_Modules)
	{
		if (not Module.bInstalled)
		{
			continue;
		}
		// New spawn: every module starts at full health.
		Module.MaxHp = GetModuleMaxHpForType(Module.Type, TankMaxHealth);
		Module.CurrentHp = Module.MaxHp;
		Module.State = EVehicleModuleState::Healthy;
	}
	(void)ValidateModuleSetup();
	bM_AreModulesFinalized = true;
	RebuildPlateModuleBindings();
	return true;
}

void UArmorCalculation::RecalculateModuleMaxHealth(const float TankMaxHealth)
{
	if (not FMath::IsFinite(TankMaxHealth) || TankMaxHealth <= 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("RecalculateModuleMaxHealth: invalid tank MaxHealth; module HP unchanged on ")
			+ GetNameSafe(GetOwner()));
		return;
	}
	if (not bM_AreModulesFinalized)
	{
		return;
	}

	M_TankMaxHealth = TankMaxHealth;
	for (FVehicleModule& Module : M_Modules)
	{
		if (not Module.bInstalled)
		{
			continue;
		}
		const float Health01 = Module.MaxHp > 0.f ? FMath::Clamp(Module.CurrentHp / Module.MaxHp, 0.f, 1.f) : 1.f;
		Module.MaxHp = GetModuleMaxHpForType(Module.Type, TankMaxHealth);
		Module.CurrentHp = Module.MaxHp * Health01;
		// The state is kept as-is so floating point rescaling cannot move a module across its exact threshold.
		++Module.Revision;
	}
}

// ----------------------------------------------------------------------------------------------------
// Add-on armor coverage and mesh rebinding
// ----------------------------------------------------------------------------------------------------

bool UArmorCalculation::SetAddOnArmorPlateCoverage(const int32 ModuleId,
                                                   const TArray<FAddOnArmorPlateBinding>& CoveredPlates)
{
	if (bM_IsMutatingModules || bM_IsDispatchingModuleChanges)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetAddOnArmorPlateCoverage: rejected during a module batch; apply")
			TEXT(" coverage between impacts. Owner: ") + GetNameSafe(GetOwner()));
		return false;
	}

	const int32 SlotIndex = GetModuleSlotById(ModuleId);
	if (SlotIndex == INDEX_NONE || M_Modules[SlotIndex].Type != EVehicleModuleTypes::AddOnArmor)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("SetAddOnArmorPlateCoverage: module %d is not an installed AddOnArmor module on %s."),
			ModuleId, *GetNameSafe(GetOwner())));
		return false;
	}

	uint64 NewCoverageMask = 0;
	if (not TryResolveCoverageMask(CoveredPlates, NewCoverageMask))
	{
		return false;
	}
	// Resolved into fixed plate IDs; the Blueprint array is not retained.
	if (GetIsCoverageOwnedByOtherModule(SlotIndex, NewCoverageMask))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("SetAddOnArmorPlateCoverage: a plate is already covered by another add-on zone (module %d, %s)."),
			ModuleId, *GetNameSafe(GetOwner())));
		return false;
	}

	// Replacement never resets HP or state and never implies a damage effect.
	FVehicleModuleBinding& Binding = M_ModuleBindings[SlotIndex];
	Binding.CoveredPlateMask = NewCoverageMask;
	Binding.bHasUnresolvedCoverage = false;
	RebuildPlateModuleBindings();
	RefreshArmorContributionsAndRearCache();
	return true;
}

bool UArmorCalculation::TryResolveCoverageMask(const TArray<FAddOnArmorPlateBinding>& CoveredPlates,
                                               uint64& OutCoverageMask) const
{
	OutCoverageMask = 0;
	for (const FAddOnArmorPlateBinding& PlateBinding : CoveredPlates)
	{
		const int32 ArmorMeshSlot = GetArmorMeshSlot(PlateBinding.MeshWithArmor);
		if (GetArmorSettingsForSlot(ArmorMeshSlot) == nullptr)
		{
			RTSFunctionLibrary::ReportError(TEXT("SetAddOnArmorPlateCoverage: mesh is not registered for armor on ")
				+ GetNameSafe(GetOwner()));
			return false;
		}

		const uint64 PlateTypeMask = GetCoverageMaskForPlateType(ArmorMeshSlot, PlateBinding.PlateType);
		if (PlateTypeMask == 0)
		{
			RTSFunctionLibrary::ReportError(TEXT("SetAddOnArmorPlateCoverage: plate type ")
				+ UEnum::GetValueAsString(PlateBinding.PlateType)
				+ TEXT(" is not registered on the provided mesh of ") + GetNameSafe(GetOwner()));
			return false;
		}
		OutCoverageMask |= PlateTypeMask;
	}
	return true;
}

uint64 UArmorCalculation::GetCoverageMaskForPlateType(const int32 ArmorMeshSlot, const EArmorPlate PlateType) const
{
	// A mesh/type pair covers every matching registered armor box on that mesh.
	const FArmorSettings* ArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
	const int32 NumPlates = GetRegisteredPlateCountForSlot(ArmorMeshSlot);
	uint64 CoverageMask = 0;
	for (int32 PlateIndex = 0; PlateIndex < NumPlates; ++PlateIndex)
	{
		if (ArmorSettings[PlateIndex].ArmorType == PlateType)
		{
			CoverageMask |= ArmorCalculationModuleHelpers::MakePlateBit(MakePlateRegistrationId(ArmorMeshSlot, PlateIndex));
		}
	}
	return CoverageMask;
}

bool UArmorCalculation::GetIsCoverageOwnedByOtherModule(const int32 SlotIndex, const uint64 CoverageMask) const
{
	const int32 FirstAddOnSlot = GetFirstSlotForType(EVehicleModuleTypes::AddOnArmor);
	const int32 EndSlot = FirstAddOnSlot + GetSlotCountForType(EVehicleModuleTypes::AddOnArmor);
	for (int32 OtherSlot = FirstAddOnSlot; OtherSlot < EndSlot; ++OtherSlot)
	{
		if (OtherSlot == SlotIndex || not M_Modules[OtherSlot].bInstalled)
		{
			continue;
		}
		if ((M_ModuleBindings[OtherSlot].CoveredPlateMask & CoverageMask) != 0)
		{
			return true;
		}
	}
	return false;
}

bool UArmorCalculation::RebindModuleMesh(const int32 ModuleId, UMeshComponent* NewMesh)
{
	const int32 SlotIndex = GetModuleSlotById(ModuleId);
	const int32 NewArmorMeshSlot = GetArmorMeshSlot(NewMesh);
	if (SlotIndex == INDEX_NONE || NewArmorMeshSlot == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("RebindModuleMesh: module %d must be installed and the mesh registered for armor on %s."),
			ModuleId, *GetNameSafe(GetOwner())));
		return false;
	}
	const EVehicleModuleTypes Type = M_Modules[SlotIndex].Type;
	if (Type != EVehicleModuleTypes::Turret && Type != EVehicleModuleTypes::Weapon)
	{
		RTSFunctionLibrary::ReportError(TEXT("RebindModuleMesh: only turret and weapon modules bind meshes."));
		return false;
	}
	if (Type == EVehicleModuleTypes::Turret)
	{
		const int32 ExistingSlot = FindBoundSlotOfType(EVehicleModuleTypes::Turret, NewArmorMeshSlot);
		if (ExistingSlot != INDEX_NONE && ExistingSlot != SlotIndex)
		{
			RTSFunctionLibrary::ReportError(TEXT("RebindModuleMesh: another turret module already uses this mount."));
			return false;
		}
	}

	// Rebinding preserves HP and state; no healing or transition events.
	FVehicleModuleBinding& Binding = M_ModuleBindings[SlotIndex];
	Binding.ArmorMeshSlot = NewArmorMeshSlot;
	Binding.BoundMesh = NewMesh;
	RebuildPlateModuleBindings();
	if (M_ModuleTank.IsValid())
	{
		M_ModuleTank->OnModuleBindingChanged(SlotIndex);
	}
	return true;
}

UMeshComponent* UArmorCalculation::GetBoundMeshForSlot(const int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= MaxModuleInstances)
	{
		return nullptr;
	}
	return M_ModuleBindings[SlotIndex].BoundMesh.Get();
}

void UArmorCalculation::SetAddOnArmorContributionMultiplier(const int32 ModuleId, const float ContributionMultiplier)
{
	const int32 SlotIndex = GetModuleSlotById(ModuleId);
	if (SlotIndex == INDEX_NONE || M_Modules[SlotIndex].Type != EVehicleModuleTypes::AddOnArmor)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("SetAddOnArmorContributionMultiplier: module %d is not an installed add-on armor module on %s."),
			ModuleId, *GetNameSafe(GetOwner())));
		return;
	}
	if (not FMath::IsFinite(ContributionMultiplier) || ContributionMultiplier < 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetAddOnArmorContributionMultiplier: multiplier must be finite and")
			TEXT(" nonnegative."));
		return;
	}
	if (M_AddOnContributionBySlot[SlotIndex] == ContributionMultiplier)
	{
		return;
	}
	// Hits already in flight used pre-impact armor; the change affects subsequent hits.
	M_AddOnContributionBySlot[SlotIndex] = ContributionMultiplier;
	RefreshArmorContributionsAndRearCache();
}

void UArmorCalculation::RefreshArmorContributionsAndRearCache()
{
	for (float& PlateContribution : M_PlateAddOnContribution)
	{
		PlateContribution = 1.f;
	}

	const int32 FirstAddOnSlot = GetFirstSlotForType(EVehicleModuleTypes::AddOnArmor);
	const int32 EndSlot = FirstAddOnSlot + GetSlotCountForType(EVehicleModuleTypes::AddOnArmor);
	for (int32 SlotIndex = FirstAddOnSlot; SlotIndex < EndSlot; ++SlotIndex)
	{
		if (M_Modules[SlotIndex].bInstalled)
		{
			ApplyAddOnContributionForSlot(SlotIndex);
		}
	}
	RefreshRearArmorCache();
}

void UArmorCalculation::ApplyAddOnContributionForSlot(const int32 SlotIndex)
{
	const uint64 CoverageMask = M_ModuleBindings[SlotIndex].CoveredPlateMask;
	for (int32 PlateRegistrationId = 0; PlateRegistrationId < MaxPlateBindings; ++PlateRegistrationId)
	{
		if ((CoverageMask & ArmorCalculationModuleHelpers::MakePlateBit(PlateRegistrationId)) != 0)
		{
			M_PlateAddOnContribution[PlateRegistrationId] = M_AddOnContributionBySlot[SlotIndex];
		}
	}
}

// ----------------------------------------------------------------------------------------------------
// Plate -> module routes
// ----------------------------------------------------------------------------------------------------

void UArmorCalculation::RebuildPlateModuleBindings()
{
	M_SelectedProfilePlateRules = GetProfilePlateRules(M_ModuleProfile, M_RunningGear);
	RefreshBoundMeshSlots();
	for (int32 PlateRegistrationId = 0; PlateRegistrationId < MaxPlateBindings; ++PlateRegistrationId)
	{
		M_PlateRuleRows[PlateRegistrationId] = INDEX_NONE;
		for (FModulePlateRoute& Route : M_PlateRoutes[PlateRegistrationId])
		{
			Route = FModulePlateRoute();
		}
	}

	for (int32 ArmorMeshSlot = 0; ArmorMeshSlot < MaxRegisteredArmorMeshes; ++ArmorMeshSlot)
	{
		const FArmorSettings* ArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
		const int32 NumPlates = GetRegisteredPlateCountForSlot(ArmorMeshSlot);
		for (int32 PlateIndex = 0; PlateIndex < NumPlates; ++PlateIndex)
		{
			const int32 RuleRow = TryGetPlateRuleIndex(ArmorSettings[PlateIndex].ArmorType);
			if (RuleRow == INDEX_NONE)
			{
				continue;
			}
			const int32 PlateRegistrationId = MakePlateRegistrationId(ArmorMeshSlot, PlateIndex);
			M_PlateRuleRows[PlateRegistrationId] = static_cast<int8>(RuleRow);
			RebuildRoutesForPlate(PlateRegistrationId, RuleRow);
		}
	}
}

void UArmorCalculation::DisableAddOnCoverageForClearedRegistration()
{
	// Plate registration IDs are rebuilt by the next registrations; old coverage bits would misroute hits.
	bool bDisabledCoverage = false;
	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		FVehicleModuleBinding& Binding = M_ModuleBindings[SlotIndex];
		if (not M_Modules[SlotIndex].bInstalled || Binding.CoveredPlateMask == 0)
		{
			continue;
		}
		Binding.CoveredPlateMask = 0;
		Binding.bHasUnresolvedCoverage = true;
		bDisabledCoverage = true;
	}
	if (bDisabledCoverage)
	{
		RTSFunctionLibrary::ReportWarning(TEXT("ClearArmorSetup disabled add-on armor coverage; call")
			TEXT(" SetAddOnArmorPlateCoverage again after registering the new meshes. Owner: ") + GetNameSafe(GetOwner()));
	}
}

void UArmorCalculation::RefreshBoundMeshSlots()
{
	// Registration slots can change when meshes are cleared and registered again; bindings follow the mesh itself.
	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		const FVehicleModule& Module = M_Modules[SlotIndex];
		const bool bUsesMeshBinding = Module.Type == EVehicleModuleTypes::Turret
			|| Module.Type == EVehicleModuleTypes::Weapon;
		if (Module.bInstalled && bUsesMeshBinding)
		{
			FVehicleModuleBinding& Binding = M_ModuleBindings[SlotIndex];
			Binding.ArmorMeshSlot = GetArmorMeshSlot(Binding.BoundMesh.Get());
		}
	}
}

void UArmorCalculation::RebuildRoutesForPlate(const int32 PlateRegistrationId, const int32 RuleRow)
{
	for (int32 CandidateIndex = 0; CandidateIndex < MaxModulesPerPlate; ++CandidateIndex)
	{
		M_PlateRoutes[PlateRegistrationId][CandidateIndex] =
			MakeRouteForCandidate(PlateRegistrationId, RuleRow, CandidateIndex);
	}
}

FModulePlateRoute UArmorCalculation::MakeRouteForCandidate(const int32 PlateRegistrationId, const int32 RuleRow,
                                                           const int32 CandidateIndex) const
{
	FModulePlateRoute Route;
	const FPlateModuleDamage& Rule = M_SelectedProfilePlateRules[RuleRow].Entries[CandidateIndex];
	const int32 ArmorMeshSlot = PlateRegistrationId / MaxArmorPlatesPerRegisteredMesh;
	const EArmorPlate Plate = static_cast<EArmorPlate>(RuleRow);
	switch (Rule.TargetSelector)
	{
	case EModuleTargetSelector::Singleton:
		Route.PrimarySlot = static_cast<int8>(FindSingletonSlotOfType(Rule.TypeToDamage));
		break;
	case EModuleTargetSelector::BoundTurret:
	case EModuleTargetSelector::BoundWeapon:
		Route.PrimarySlot = static_cast<int8>(FindBoundSlotOfType(Rule.TypeToDamage, ArmorMeshSlot));
		break;
	case EModuleTargetSelector::CoveringArmorZone:
		Route.PrimarySlot = static_cast<int8>(FindCoveringAddOnSlot(PlateRegistrationId));
		break;
	case EModuleTargetSelector::RunningGearSide:
		Route = MakeRunningGearRoute(Plate, Rule.TypeToDamage);
		break;
	default:
		break;
	}
	return Route;
}

FModulePlateRoute UArmorCalculation::MakeRunningGearRoute(const EArmorPlate Plate,
                                                          const EVehicleModuleTypes RunningGearType) const
{
	const bool bLeftInstalled = M_Modules[LeftRunningGearSlot].bInstalled
		&& M_Modules[LeftRunningGearSlot].Type == RunningGearType;
	const bool bRightInstalled = M_Modules[RightRunningGearSlot].bInstalled
		&& M_Modules[RightRunningGearSlot].Type == RunningGearType;
	const int8 LeftSlot = static_cast<int8>(bLeftInstalled ? LeftRunningGearSlot : INDEX_NONE);
	const int8 RightSlot = static_cast<int8>(bRightInstalled ? RightRunningGearSlot : INDEX_NONE);

	FModulePlateRoute Route;
	if (ArmorCalculationModuleHelpers::GetIsLeftHullSidePlate(Plate))
	{
		Route.PrimarySlot = LeftSlot;
		return Route;
	}
	if (ArmorCalculationModuleHelpers::GetIsRightHullSidePlate(Plate))
	{
		Route.PrimarySlot = RightSlot;
		return Route;
	}
	// Front and rear plates: the hull-local lateral hit position selects the side at impact time.
	Route.PrimarySlot = LeftSlot;
	Route.AlternateSlot = RightSlot;
	Route.bUsesHitSide = true;
	return Route;
}

int32 UArmorCalculation::FindBoundSlotOfType(const EVehicleModuleTypes Type, const int32 ArmorMeshSlot) const
{
	const int32 FirstSlot = GetFirstSlotForType(Type);
	if (FirstSlot == INDEX_NONE || ArmorMeshSlot == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	const int32 EndSlot = FirstSlot + GetSlotCountForType(Type);
	for (int32 SlotIndex = FirstSlot; SlotIndex < EndSlot; ++SlotIndex)
	{
		if (M_Modules[SlotIndex].bInstalled && M_Modules[SlotIndex].Type == Type
			&& M_ModuleBindings[SlotIndex].ArmorMeshSlot == ArmorMeshSlot)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

int32 UArmorCalculation::FindSingletonSlotOfType(const EVehicleModuleTypes Type) const
{
	const int32 FirstSlot = GetFirstSlotForType(Type);
	if (FirstSlot == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	const int32 EndSlot = FirstSlot + GetSlotCountForType(Type);
	for (int32 SlotIndex = FirstSlot; SlotIndex < EndSlot; ++SlotIndex)
	{
		if (M_Modules[SlotIndex].bInstalled && M_Modules[SlotIndex].Type == Type)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

int32 UArmorCalculation::FindCoveringAddOnSlot(const int32 PlateRegistrationId) const
{
	const uint64 PlateBit = ArmorCalculationModuleHelpers::MakePlateBit(PlateRegistrationId);
	const int32 FirstAddOnSlot = GetFirstSlotForType(EVehicleModuleTypes::AddOnArmor);
	const int32 EndSlot = FirstAddOnSlot + GetSlotCountForType(EVehicleModuleTypes::AddOnArmor);
	for (int32 SlotIndex = FirstAddOnSlot; SlotIndex < EndSlot; ++SlotIndex)
	{
		if (M_Modules[SlotIndex].bInstalled && (M_ModuleBindings[SlotIndex].CoveredPlateMask & PlateBit) != 0)
		{
			return SlotIndex;
		}
	}
	// An unconfigured add-on candidate is skipped.
	return INDEX_NONE;
}

// ----------------------------------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------------------------------

int32 UArmorCalculation::GetModuleSlotById(const int32 ModuleId) const
{
	if (ModuleId < 0)
	{
		return INDEX_NONE;
	}
	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		if (M_Modules[SlotIndex].bInstalled && M_Modules[SlotIndex].ModuleId == ModuleId)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

FVehicleModuleSnapshot UArmorCalculation::GetModuleSnapshot(const int32 ModuleId) const
{
	return GetModuleSnapshotForSlot(GetModuleSlotById(ModuleId));
}

FVehicleModuleSnapshot UArmorCalculation::GetModuleSnapshotForSlot(const int32 SlotIndex) const
{
	FVehicleModuleSnapshot Snapshot;
	if (SlotIndex < 0 || SlotIndex >= MaxModuleInstances || not M_Modules[SlotIndex].bInstalled)
	{
		return Snapshot;
	}
	const FVehicleModule& Module = M_Modules[SlotIndex];
	Snapshot.ModuleId = Module.ModuleId;
	Snapshot.Type = Module.Type;
	Snapshot.State = Module.State;
	Snapshot.CurrentHp = Module.CurrentHp;
	Snapshot.MaxHp = Module.MaxHp;
	Snapshot.bInstalled = true;
	return Snapshot;
}

EVehicleModuleState UArmorCalculation::GetAggregateModuleState(const EVehicleModuleTypes Type) const
{
	if (not GetIsInstallableModuleType(Type))
	{
		return EVehicleModuleState::Healthy;
	}
	const int32 TypeIndex = GetModuleTypeIndex(Type);
	if (M_DestroyedCountByType[TypeIndex] > 0)
	{
		return EVehicleModuleState::Destroyed;
	}
	if (M_DamagedCountByType[TypeIndex] > 0)
	{
		return EVehicleModuleState::Damaged;
	}
	return EVehicleModuleState::Healthy;
}

FModuleIconStates UArmorCalculation::GetModuleIconStates() const
{
	FModuleIconStates IconStates;
	for (int32 TypeIndex = 1; TypeIndex < ModuleTypeCount; ++TypeIndex)
	{
		IconStates.States[TypeIndex] = GetAggregateModuleState(static_cast<EVehicleModuleTypes>(TypeIndex));
	}
	return IconStates;
}

int32 UArmorCalculation::SelectNextRedModuleForCrewRepair() const
{
	// Constexpr priority first, then stable slot order.
	for (const EVehicleModuleTypes Type : CrewRepairPriority)
	{
		const int32 RedModuleId = FindFirstRedModuleOfType(Type);
		if (RedModuleId != INDEX_NONE)
		{
			return RedModuleId;
		}
	}
	return INDEX_NONE;
}

int32 UArmorCalculation::FindFirstRedModuleOfType(const EVehicleModuleTypes Type) const
{
	const int32 FirstSlot = GetFirstSlotForType(Type);
	const int32 EndSlot = FirstSlot + GetSlotCountForType(Type);
	for (int32 SlotIndex = FirstSlot; SlotIndex < EndSlot; ++SlotIndex)
	{
		const FVehicleModule& Module = M_Modules[SlotIndex];
		if (Module.bInstalled && Module.Type == Type && Module.State == EVehicleModuleState::Destroyed)
		{
			return Module.ModuleId;
		}
	}
	return INDEX_NONE;
}

// ----------------------------------------------------------------------------------------------------
// State mutation, repair milestones and dispatch
// ----------------------------------------------------------------------------------------------------

void UArmorCalculation::SetModuleHealth(const int32 SlotIndex, const float NewHp, const EModuleChangeCause Cause)
{
	FVehicleModule& Module = M_Modules[SlotIndex];
	const float ClampedHp = FMath::Clamp(NewHp, 0.f, Module.MaxHp);
	const EVehicleModuleState PreviousState = Module.State;
	if constexpr (DeveloperSettings::Debugging::GTankModuleSystem_Compile_DebugSymbols)
	{
		if (Cause == EModuleChangeCause::Damage && ClampedHp < Module.CurrentHp && GetIsValidModuleTank())
		{
			ArmorCalculationModuleDebug::PrintModuleHit(M_ModuleTank.Get(), Module, Module.CurrentHp - ClampedHp);
		}
	}
	Module.CurrentHp = ClampedHp;
	++Module.Revision;
	const float Health01 = Module.MaxHp > 0.f ? FMath::Clamp(ClampedHp / Module.MaxHp, 0.f, 1.f) : CompletedHealth01;
	Module.State = GetModuleStateForHealth01(Module.Type, Health01);
	if constexpr (DeveloperSettings::Debugging::GTankModuleSystem_Compile_DebugSymbols)
	{
		if (PreviousState != Module.State && GetIsValidModuleTank())
		{
			ArmorCalculationModuleDebug::PrintModuleStateChange(M_ModuleTank.Get(), Module, PreviousState, Cause);
		}
	}
	if (PreviousState == Module.State)
	{
		return;
	}

	UpdateModuleStateCounts(Module.Type, PreviousState, Module.State);
	FModuleStateChange Change;
	Change.ModuleId = Module.ModuleId;
	Change.SlotIndex = SlotIndex;
	Change.Type = Module.Type;
	Change.PreviousState = PreviousState;
	Change.NewState = Module.State;
	Change.CurrentHp = Module.CurrentHp;
	Change.MaxHp = Module.MaxHp;
	Change.Cause = Cause;
	M_PendingModuleChanges.AddOrMerge(Change);
}

void UArmorCalculation::UpdateModuleStateCounts(const EVehicleModuleTypes Type,
                                                const EVehicleModuleState PreviousState,
                                                const EVehicleModuleState NewState)
{
	const int32 TypeIndex = GetModuleTypeIndex(Type);
	auto AdjustCounts = [this, TypeIndex](const EVehicleModuleState State, const int32 Delta)
	{
		if (State == EVehicleModuleState::Destroyed)
		{
			M_DestroyedModuleCount += Delta;
			M_DestroyedCountByType[TypeIndex] += Delta;
		}
		if (State == EVehicleModuleState::Damaged)
		{
			M_DamagedCountByType[TypeIndex] += Delta;
		}
		if (State != EVehicleModuleState::Healthy)
		{
			M_NonHealthyModuleCount += Delta;
		}
	};
	AdjustCounts(PreviousState, -1);
	AdjustCounts(NewState, 1);
}

FModuleChangeBatch UArmorCalculation::TakePendingModuleChanges()
{
	FModuleChangeBatch Changes = M_PendingModuleChanges;
	M_PendingModuleChanges.Reset();
	return Changes;
}

void UArmorCalculation::DispatchModuleChangesAfterMutation(const FModuleChangeBatch& Changes)
{
	if (Changes.IsEmpty())
	{
		return;
	}
	if (bM_IsDispatchingModuleChanges)
	{
		// Re-entrant mutation from a callback: the running dispatch loop publishes it after the current batch.
		for (const FModuleStateChange& Change : Changes.GetChanges())
		{
			M_PendingModuleChanges.AddOrMerge(Change);
		}
		return;
	}
	if (not GetIsValidModuleTank())
	{
		return;
	}

	TGuardValue<bool> DispatchGuard(bM_IsDispatchingModuleChanges, true);
	FModuleChangeBatch CurrentBatch = Changes;
	while (not CurrentBatch.IsEmpty() && M_ModuleTank.IsValid())
	{
		M_ModuleTank->OnModuleStateBatchCommitted(CurrentBatch);
		CurrentBatch = TakePendingModuleChanges();
	}
}

void UArmorCalculation::RestoreDestroyedModulesToDamaged(const EModuleChangeCause Cause)
{
	if (M_DestroyedModuleCount <= 0)
	{
		return;
	}
	{
		TGuardValue<bool> MutationGuard(bM_IsMutatingModules, true);
		for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
		{
			const FVehicleModule& Module = M_Modules[SlotIndex];
			if (not Module.bInstalled || Module.State != EVehicleModuleState::Destroyed)
			{
				continue;
			}
			SetModuleHealth(SlotIndex, Module.MaxHp * GetRecoveredHealth01(Module.Type), Cause);
		}
	}
	DispatchModuleChangesAfterMutation(TakePendingModuleChanges());
}

void UArmorCalculation::RestoreAllModulesToHealthy()
{
	if (M_NonHealthyModuleCount <= 0)
	{
		return;
	}
	{
		TGuardValue<bool> MutationGuard(bM_IsMutatingModules, true);
		for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
		{
			const FVehicleModule& Module = M_Modules[SlotIndex];
			if (not Module.bInstalled || Module.State == EVehicleModuleState::Healthy)
			{
				continue;
			}
			// One final-state transition per module; no intermediate yellow state for red modules.
			SetModuleHealth(SlotIndex, Module.MaxHp, EModuleChangeCause::FullService);
		}
	}
	DispatchModuleChangesAfterMutation(TakePendingModuleChanges());
}

bool UArmorCalculation::RestoreDestroyedModuleToDamaged(const int32 ModuleId)
{
	const int32 SlotIndex = GetModuleSlotById(ModuleId);
	if (SlotIndex == INDEX_NONE || M_Modules[SlotIndex].State != EVehicleModuleState::Destroyed)
	{
		return false;
	}
	{
		TGuardValue<bool> MutationGuard(bM_IsMutatingModules, true);
		const FVehicleModule& Module = M_Modules[SlotIndex];
		SetModuleHealth(SlotIndex, Module.MaxHp * GetRecoveredHealth01(Module.Type), EModuleChangeCause::CrewRepair);
	}
	DispatchModuleChangesAfterMutation(TakePendingModuleChanges());
	return true;
}
