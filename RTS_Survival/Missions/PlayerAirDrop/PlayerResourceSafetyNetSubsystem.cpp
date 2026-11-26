// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlayerResourceSafetyNetSubsystem.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "BuildingFreeDropSpots.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ResourceDrop/GA_ResourceDrop.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/PickupItems/Items/ResourcePickup/SupplyCratePickup.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace PlayerResourceSafetyNetConstants
{
	// The human player in the unit registry (FRTSPlayers numbering).
	constexpr uint8 LocalPlayer = 1;
}

UPlayerResourceSafetyNetSubsystem* UPlayerResourceSafetyNetSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return IsValid(World) ? World->GetSubsystem<UPlayerResourceSafetyNetSubsystem>() : nullptr;
}

void UPlayerResourceSafetyNetSubsystem::StartSafetyNet(const FPlayerResourceSafetyNetSettings& Settings,
                                                       UMissionBase* StartedBy)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (Settings.RequiredPurchases.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Player resource safety net started without required purchases; it does "
			"nothing.");
		return;
	}
	M_Settings = Settings;
	M_StartedBy = StartedBy;
	if (not CacheDropSettings())
	{
		return;
	}
	bM_IsWatching = true;
	bM_ReportedMissingCosts = false;
	M_ShortfallStartWorldSeconds = -1.f;
	World->GetTimerManager().SetTimer(M_CheckTimer, this, &UPlayerResourceSafetyNetSubsystem::CheckResources,
	                                  M_Settings.CheckIntervalSeconds, true);
}

void UPlayerResourceSafetyNetSubsystem::StopSafetyNet()
{
	bM_IsWatching = false;
	M_StartedBy.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CheckTimer);
	}
}

int32 UPlayerResourceSafetyNetSubsystem::GetOutstandingCrateCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AParachutedResourcePayload>& Falling : M_FallingCrates)
	{
		Count += Falling.IsValid() ? 1 : 0;
	}
	for (const TWeakObjectPtr<ASupplyCratePickup>& Landed : M_LandedCrates)
	{
		Count += Landed.IsValid() ? 1 : 0;
	}
	return Count;
}

TMap<ERTSResourceType, int32> UPlayerResourceSafetyNetSubsystem::GetRequiredResources() const
{
	TMap<ERTSResourceType, int32> Required;
	for (const FPlayerResourceSafetyNetPurchase& Purchase : M_Settings.RequiredPurchases)
	{
		if (GetIsPurchaseOwned(Purchase))
		{
			continue;
		}
		for (const TPair<ERTSResourceType, int32>& Cost : GetPurchaseCost(Purchase))
		{
			int32& Needed = Required.FindOrAdd(Cost.Key);
			Needed = FMath::Max(Needed, Cost.Value);
		}
	}
	return Required;
}

void UPlayerResourceSafetyNetSubsystem::Deinitialize()
{
	StopSafetyNet();
	Super::Deinitialize();
}

void UPlayerResourceSafetyNetSubsystem::CheckResources()
{
	if (GetShouldStop())
	{
		StopSafetyNet();
		return;
	}
	// Nothing can be bought before the HQ stands, so being broke before that is not a shortfall.
	if (not GetIsPlayerHQStanding())
	{
		M_ShortfallStartWorldSeconds = -1.f;
		return;
	}
	if (not GetHasUnownedPurchase())
	{
		// Every required purchase is owned: the tutorial no longer depends on the player's money.
		StopSafetyNet();
		return;
	}
	const TMap<ERTSResourceType, int32> Required = GetRequiredResources();
	if (Required.IsEmpty())
	{
		if (not bM_ReportedMissingCosts)
		{
			bM_ReportedMissingCosts = true;
			RTSFunctionLibrary::ReportError("Player resource safety net: a required purchase is not owned but has no "
				"valid cost; no crates can be sized.");
		}
		return;
	}
	const bool bIsShort = not GetShortfall(Required).IsEmpty();
	if (not GetIsShortfallLongEnough(bIsShort))
	{
		return;
	}
	if (GetOutstandingCrateCount() > 0 || GetIsDropOnCooldown() || M_DropCount >= M_Settings.MaxDrops)
	{
		return;
	}
	SendDrop(Required);
}

bool UPlayerResourceSafetyNetSubsystem::GetShouldStop() const
{
	// A mission that completes or fails stops the net itself (UMissionBase::StopPlayerResourceSafetyNetIfOwner).
	return not M_StartedBy.IsValid();
}

bool UPlayerResourceSafetyNetSubsystem::GetHasUnownedPurchase() const
{
	return M_Settings.RequiredPurchases.ContainsByPredicate([this](const FPlayerResourceSafetyNetPurchase& Purchase)
	{
		return not GetIsPurchaseOwned(Purchase);
	});
}

bool UPlayerResourceSafetyNetSubsystem::GetIsPlayerHQStanding() const
{
	ANomadicVehicle* HQ = FRTS_Statics::GetPlayerHq(this);
	return RTSFunctionLibrary::RTSIsValid(HQ) && HQ->GetNomadicStatus() == ENomadStatus::Building;
}

bool UPlayerResourceSafetyNetSubsystem::GetIsPurchaseOwned(const FPlayerResourceSafetyNetPurchase& Purchase) const
{
	if (not Purchase.bIsBuildingExpansion)
	{
		return GetIsUnitOwned(Purchase.Unit);
	}
	const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(this);
	return IsValid(UnitManager) && UnitManager->GetPlayerBxpTotalCountOfTypes(
		PlayerResourceSafetyNetConstants::LocalPlayer, {Purchase.BuildingExpansion}) > 0;
}

bool UPlayerResourceSafetyNetSubsystem::GetIsUnitOwned(const FTrainingOption& Unit) const
{
	const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(UnitManager))
	{
		return false;
	}
	if (Unit.UnitType == EAllUnitType::UNType_Squad)
	{
		return UnitManager->GetPlayerSquadCountOfTypes(PlayerResourceSafetyNetConstants::LocalPlayer,
		                                               {static_cast<ESquadSubtype>(Unit.SubtypeValue)}) > 0;
	}
	// Nomadics (trucks and buildings) are told apart by class and nomadic subtype, as GetPlayerNomadicsOfType does;
	// a harvester is trained as a tank option but may register as a harvester, so other vehicles match on the tank
	// subtype whatever their unit type.
	const bool bWantsNomadic = Unit.UnitType == EAllUnitType::UNType_Nomadic;
	for (const ATankMaster* Tank : UnitManager->GetTanksOfPlayerView(PlayerResourceSafetyNetConstants::LocalPlayer))
	{
		const URTSComponent* RTSComponent = IsValid(Tank) ? Tank->GetRTSComponent() : nullptr;
		if (not IsValid(RTSComponent) || Tank->IsA<ANomadicVehicle>() != bWantsNomadic)
		{
			continue;
		}
		const uint8 Subtype = bWantsNomadic
			                      ? static_cast<uint8>(RTSComponent->GetSubtypeAsNomadicSubtype())
			                      : static_cast<uint8>(RTSComponent->GetSubtypeAsTankSubtype());
		if (Subtype == Unit.SubtypeValue)
		{
			return true;
		}
	}
	return false;
}

TMap<ERTSResourceType, int32> UPlayerResourceSafetyNetSubsystem::GetPurchaseCost(
	const FPlayerResourceSafetyNetPurchase& Purchase) const
{
	bool bIsValidCost = false;
	if (Purchase.bIsBuildingExpansion)
	{
		const FBxpData BxpData = FRTS_Statics::BP_GetPlayerBxpData(Purchase.BuildingExpansion, this, bIsValidCost);
		return bIsValidCost ? BxpData.Cost.ResourceCosts : TMap<ERTSResourceType, int32>();
	}
	TMap<ERTSResourceType, int32> Cost = FRTS_Statics::GetResourceCostsOfTrainingOption(Purchase.Unit, bIsValidCost,
		this);
	return bIsValidCost ? Cost : TMap<ERTSResourceType, int32>();
}

TMap<ERTSResourceType, int32> UPlayerResourceSafetyNetSubsystem::GetShortfall(
	const TMap<ERTSResourceType, int32>& Required) const
{
	TMap<ERTSResourceType, int32> Shortfall;
	const UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(ResourceManager))
	{
		return Shortfall;
	}
	for (const TPair<ERTSResourceType, int32>& Need : Required)
	{
		const int32 Missing = Need.Value - ResourceManager->GetResourceAmount(Need.Key);
		if (Missing > 0)
		{
			Shortfall.Add(Need.Key, Missing);
		}
	}
	return Shortfall;
}

bool UPlayerResourceSafetyNetSubsystem::GetIsShortfallLongEnough(const bool bIsShort)
{
	if (not bIsShort)
	{
		M_ShortfallStartWorldSeconds = -1.f;
		return false;
	}
	const float Now = GetWorldSeconds();
	if (M_ShortfallStartWorldSeconds < 0.f)
	{
		M_ShortfallStartWorldSeconds = Now;
	}
	return Now - M_ShortfallStartWorldSeconds >= M_Settings.ShortfallSeconds;
}

bool UPlayerResourceSafetyNetSubsystem::GetIsDropOnCooldown() const
{
	return M_LastDropWorldSeconds >= 0.f
		&& GetWorldSeconds() - M_LastDropWorldSeconds < M_Settings.MinSecondsBetweenDrops;
}

bool UPlayerResourceSafetyNetSubsystem::CacheDropSettings()
{
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	const UGA_ResourceDrop* SupplyDrop = IsValid(AbilitySet)
		                                     ? Cast<UGA_ResourceDrop>(
			                                     AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(
				                                     M_Settings.SupplyDropAbility))
		                                     : nullptr;
	if (not IsValid(SupplyDrop))
	{
		RTSFunctionLibrary::ReportError("Player resource safety net: " + UEnum::GetValueAsString(
			M_Settings.SupplyDropAbility) + " is not a resource drop ability in the global ability data asset; "
			"the safety net does not start.");
		return false;
	}
	M_DropSettings = SupplyDrop->GetResourceDropSettings();
	M_DropSettings.PayloadKind = EResourceDropPayloadKind::Pickup;
	M_DropSettings.PickupSettings = M_Settings.CrateLooks;
	return true;
}

TMap<ERTSResourceType, int32> UPlayerResourceSafetyNetSubsystem::BuildCrateContents(
	const TMap<ERTSResourceType, int32>& Required) const
{
	TMap<ERTSResourceType, int32> PerCrate;
	const UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(ResourceManager))
	{
		return PerCrate;
	}
	const int32 Crates = FMath::Max(1, M_Settings.CratesPerDrop);
	const int32 RoundTo = FMath::Max(1, M_Settings.RoundCrateContentsTo);
	for (const TPair<ERTSResourceType, int32>& Need : Required)
	{
		const int32 Target = FMath::CeilToInt(static_cast<float>(Need.Value) * (1.f + M_Settings.ExtraFraction));
		const int32 TopUp = Target - ResourceManager->GetResourceAmount(Need.Key);
		if (TopUp <= 0)
		{
			continue;
		}
		const int32 Share = FMath::DivideAndRoundUp(TopUp, Crates);
		PerCrate.Add(Need.Key, FMath::DivideAndRoundUp(Share, RoundTo) * RoundTo);
	}
	return PerCrate;
}

void UPlayerResourceSafetyNetSubsystem::SendDrop(const TMap<ERTSResourceType, int32>& Required)
{
	UWorld* World = GetWorld();
	const ANomadicVehicle* HQ = FRTS_Statics::GetPlayerHq(this);
	if (not IsValid(World) || not IsValid(HQ))
	{
		return;
	}
	M_DropSettings.PickupSettings.ResourcesPerPickup = BuildCrateContents(Required);
	if (M_DropSettings.PickupSettings.ResourcesPerPickup.IsEmpty())
	{
		return;
	}
	const FVector Anchor = HQ->GetActorLocation() + FVector(M_Settings.DropOffsetFromHQ.X,
	                                                        M_Settings.DropOffsetFromHQ.Y, 0.f);
	TArray<FVector> Spots;
	for (int32 CrateIndex = 0; CrateIndex < M_Settings.CratesPerDrop; ++CrateIndex)
	{
		const auto GetIsFarFromOtherCrates = [&Spots, this](const FVector& Candidate)
		{
			return not Spots.ContainsByPredicate([&Candidate, this](const FVector& Taken)
			{
				return FVector::Dist2D(Candidate, Taken) < M_Settings.CrateSpacing;
			});
		};
		FVector Spot;
		if (FBuildingFreeDropSpots::FindBuildingFreeSpot(this, Anchor, M_Settings.CrateBuildingClearance,
		                                                 GetIsFarFromOtherCrates, Spot)
			&& SpawnFallingCrate(*World, Spot))
		{
			Spots.Add(Spot);
		}
	}
	if (Spots.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Player resource safety net found no spot clear of buildings near the HQ; "
			"no crates were dropped.");
		return;
	}
	++M_DropCount;
	M_LastDropWorldSeconds = GetWorldSeconds();
	M_ShortfallStartWorldSeconds = -1.f;
	FVector Centre = FVector::ZeroVector;
	for (const FVector& Spot : Spots)
	{
		Centre += Spot / static_cast<float>(Spots.Num());
	}
	M_LastDropLocation = Centre;
	OnDropSent.Broadcast(Centre);
	RTSFunctionLibrary::PrintString(FString::Printf(TEXT("Resource safety net: %d supply crates dropped (drop %d)"),
	                                                Spots.Num(), M_DropCount));
}

bool UPlayerResourceSafetyNetSubsystem::SpawnFallingCrate(UWorld& World, const FVector& GroundLocation)
{
	const FVector ReleaseLocation = GroundLocation + FVector(0.f, 0.f, M_Settings.ReleaseHeight);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AParachutedResourcePayload* Crate = World.SpawnActor<AParachutedResourcePayload>(
		AParachutedResourcePayload::StaticClass(), FTransform(ReleaseLocation), SpawnParameters);
	if (not IsValid(Crate))
	{
		return false;
	}
	Crate->OnPayloadLanded.AddUObject(this, &UPlayerResourceSafetyNetSubsystem::OnCrateLanded);
	Crate->StartFall(ReleaseLocation, GroundLocation, ASupplyCratePickup::StaticClass(), M_DropSettings);
	M_FallingCrates.Add(Crate);
	return true;
}

void UPlayerResourceSafetyNetSubsystem::OnCrateLanded(AParachutedResourcePayload* Payload, AActor* SpawnedResource)
{
	M_FallingCrates.RemoveAll([Payload](const TWeakObjectPtr<AParachutedResourcePayload>& Falling)
	{
		return not Falling.IsValid() || Falling.Get() == Payload;
	});
	M_LandedCrates.RemoveAll([](const TWeakObjectPtr<ASupplyCratePickup>& Landed)
	{
		return not Landed.IsValid();
	});
	if (ASupplyCratePickup* Crate = Cast<ASupplyCratePickup>(SpawnedResource))
	{
		M_LandedCrates.Add(Crate);
	}
}

float UPlayerResourceSafetyNetSubsystem::GetWorldSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}
