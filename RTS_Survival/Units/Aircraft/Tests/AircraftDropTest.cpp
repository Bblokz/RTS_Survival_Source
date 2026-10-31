// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftDropValidate: the German vehicle drop global abilities (UAircraftTestSubsystem).

#include "AircraftTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftDropTest, Log, All);

namespace AircraftDropTestPrivate
{
	struct FExpectedDropVehicle
	{
		const TCHAR* ClassName;
		int32 Count;
	};

	const TCHAR* const DropAircraftClassMarker = TEXT("Ger_Aircraft_JU390B_AirborneUnarmed");
	constexpr int32 DroppingPlayer = 1;
	constexpr int32 ExpectedAircraftCount = 12;
	// The aircraft come in from the edge of the map, land, wait and take off again.
	constexpr float DropTimeoutSeconds = 240.f;
	// Far enough apart that twelve aircraft do not land on each other.
	constexpr float DropAnchorSpacing = 4000.f;
	constexpr float DroppedVehicleMaxHeightAboveAnchor = 500.f;

	const TArray<EGlobalAbility> DropAbilities = {
		EGlobalAbility::GA_GerPzIIIFlak41Drop,
		EGlobalAbility::GA_GerSdkfz7FlakDrop,
		EGlobalAbility::GA_GerRadixitePzIIMarderDrop,
		EGlobalAbility::GA_GerFlakLuchsStugDrop,
		EGlobalAbility::GA_GerSpotterPzIIIDrop
	};

	const TArray<FExpectedDropVehicle> ExpectedVehicles = {
		{TEXT("Ger_Vehicle_PanzerIII_AA_Flak41_C"), 2},
		{TEXT("Ger_Vehicle_Sdkfz7Flak_C"), 1},
		{TEXT("Ger_Vehicle_Sdkfz7Flak_Flak36_C"), 1},
		{TEXT("Ger_Vehicle_PanzerII_F_RadixiteAutocannon_C"), 2},
		{TEXT("Ger_Vehicle_Marder_DualBarrel_C"), 1},
		{TEXT("Ger_Vehicle_PanzerII_Luchs_FlakLuchs_C"), 2},
		{TEXT("Ger_Vehicle_StugIII_C"), 1},
		{TEXT("Ger_Vehicle_PanzerIV_F1_Spotter_C"), 1},
		{TEXT("Ger_Vehicle_PanzerIII_J_C"), 1}
	};
}

void UAircraftTestSubsystem::StartAircraftDrop()
{
	using namespace AircraftDropTestPrivate;
	EnterPhase(EAircraftTestPhase::AircraftDropRunning);
	// A map started directly has no commander, so the abilities are built from their templates as the manager does.
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	Check(IsValid(Manager), TEXT("drop_player_ability_manager"));
	Check(IsValid(AbilitySet), TEXT("drop_ability_set"));
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		Finish();
		return;
	}
	for (int32 Index = 0; Index < DropAbilities.Num(); ++Index)
	{
		UGlobalAbility* Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(DropAbilities[Index]);
		const FString AbilityName = UEnum::GetValueAsString(DropAbilities[Index]);
		Check(IsValid(Cast<UGA_AircraftDrop>(Ability)), TEXT("drop_ability_template"), AbilityName);
		if (not IsValid(Ability))
		{
			continue;
		}
		M_DropAbilities.Add(Ability);
		Ability->InitGlobalAbility(DroppingPlayer, Manager, Controller);
		const float SideOffset = (static_cast<float>(Index) - 2.f) * DropAnchorSpacing;
		Ability->ExecuteAbilityAtLocation(M_Anchor + FVector(0.f, SideOffset, 0.f));
		UE_LOG(LogRTSAircraftDropTest, Display, TEXT("RTS_AIRCRAFT_TEST DROP_EXECUTED %s"), *AbilityName);
	}
}

void UAircraftTestSubsystem::TickAircraftDrop()
{
	using namespace AircraftDropTestPrivate;
	int32 AircraftCount = 0;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		AircraftCount += It->GetClass()->GetName().Contains(DropAircraftClassMarker) ? 1 : 0;
	}
	M_DropMostAircraftSeen = FMath::Max(M_DropMostAircraftSeen, AircraftCount);

	TMap<FString, int32> SpawnedByClass;
	TMap<FString, int32> LandedByClass;
	for (TActorIterator<ATankMaster> It(GetWorld()); It; ++It)
	{
		const ATankMaster* Tank = *It;
		const FString ClassName = Tank->GetClass()->GetName();
		SpawnedByClass.FindOrAdd(ClassName)++;
		const float HeightAboveAnchor = Tank->GetActorLocation().Z - M_Anchor.Z;
		if (Tank->GetAttachParentActor() == nullptr && HeightAboveAnchor < DroppedVehicleMaxHeightAboveAnchor)
		{
			LandedByClass.FindOrAdd(ClassName)++;
		}
	}

	bool bAllLanded = true;
	for (const FExpectedDropVehicle& Expected : ExpectedVehicles)
	{
		bAllLanded &= LandedByClass.FindRef(Expected.ClassName) >= Expected.Count;
	}
	const float Waited = GetNow() - M_PhaseStartSeconds;
	if (not bAllLanded && Waited < DropTimeoutSeconds)
	{
		return;
	}

	Check(M_DropMostAircraftSeen >= ExpectedAircraftCount, TEXT("drop_twelve_ju390_spawned"),
	      FString::Printf(TEXT("seen=%d"), M_DropMostAircraftSeen));
	for (const FExpectedDropVehicle& Expected : ExpectedVehicles)
	{
		Check(LandedByClass.FindRef(Expected.ClassName) == Expected.Count, FString(TEXT("drop_landed_")) + Expected.ClassName,
		      FString::Printf(TEXT("spawned=%d landed=%d expected=%d after=%.1fs"),
		                      SpawnedByClass.FindRef(Expected.ClassName), LandedByClass.FindRef(Expected.ClassName),
		                      Expected.Count, Waited));
	}
	Finish();
}

#endif
