// Copyright (C) Bas Blokzijl - All rights reserved.


#include "BuildingExpansionOwner.h"


#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpOwnerComp/BuildingExpansionOwnerComp.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/AsyncBxpLoadingType/AsyncBxpLoadingType.h"


FString IBuildingExpansionOwner::GetOwnerName() const
{
	if (const AActor* OwnerActor = Cast<AActor>(this))
	{
		return OwnerActor->GetName();
	}
	return FString("Unknown");
}

bool IBuildingExpansionOwner::GetIsAsyncBatchLoadingAttachedPackedExpansions() const
{
	return GetBuildingExpansionData().GetIsAsyncBatchLoadingPackedExpansions();
}

float IBuildingExpansionOwner::GetBxpExpansionRange() const
{
	return float();
}

FVector IBuildingExpansionOwner::GetBxpOwnerLocation() const
{
	return FVector();
}

UStaticMeshComponent* IBuildingExpansionOwner::GetAttachToMeshComponent() const
{
	return nullptr;
}

TArray<FBuildingExpansionItem>& IBuildingExpansionOwner::GetBuildingExpansions() const
{
	return GetBuildingExpansionData().M_TBuildingExpansionItems;
}


// void IBuildingExpansionOwner::CancelBxpThatWasNotLoaded(
// 	const int Index,
// 	const bool bIsCancelledPackedExpansion)
// {
// 	if (Index != INDEX_NONE)
// 	{
// 		UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
// 		// If we destroy a cancel expansion we set the status to packed.
// 		const EBuildingExpansionStatus Status = bIsCancelledPackedExpansion
// 			                                        ? EBuildingExpansionStatus::BXS_PackedUp
// 			                                        : EBuildingExpansionStatus::BXS_UnlockedButNotBuild;
// 		// If we destroy a packed expansion we keep the type.
// 		const EBuildingExpansionType Type = bIsCancelledPackedExpansion
// 			                                    ? BuildingExpansionData.m_TBuildingExpansionItems[Index].ExpansionType
// 			                                    : EBuildingExpansionType::BXT_Invalid;
// 		// Update the data in our expansion array.
// 		BuildingExpansionData.m_TBuildingExpansionItems[Index].Expansion = nullptr;
// 		BuildingExpansionData.m_TBuildingExpansionItems[Index].ExpansionStatus = Status;
// 		BuildingExpansionData.m_TBuildingExpansionItems[Index].ExpansionType = Type;
// 		// Update the main game UI with the new widget state if this is the main selected actor.
// 		BuildingExpansionData.UpdateMainGameUIWithStatusChanges(Index,
// 		                                                        Status,
// 		                                                        Type);
// 	}
// 	else
// 	{
// 		RTSFunctionLibrary::ReportError("Attempted to Cancel a not loaded Bxp with INDEX_NONE!."
// 								  "See IBuildingExpansionOwner::CancelBxpThatWasNotLoaded.");
// 	}
// }

ABuildingExpansion* IBuildingExpansionOwner::GetBuildingExpansionAtIndex(const int Index) const
{
	if (Index >= 0 && Index < GetBuildingExpansionData().M_TBuildingExpansionItems.Num())
	{
		return GetBuildingExpansionData().M_TBuildingExpansionItems[Index].Expansion;
	}
	return nullptr;
}

FBuildingExpansionItem* IBuildingExpansionOwner::GetBuildingExpansionItemAtIndex(const int Index) const
{
	if (Index >= 0 && Index < GetBuildingExpansionData().M_TBuildingExpansionItems.Num())
	{
		return &GetBuildingExpansionData().M_TBuildingExpansionItems[Index];
	}
	return nullptr;
}

TArray<FBxpOptionData> IBuildingExpansionOwner::GetUnlockedBuildingExpansionTypes() const
{
	TArray<FBxpOptionData> UnlockedBxpData;
	auto& MyData = GetBuildingExpansionData();
	for (const auto EachOption : MyData.M_TUnlockedBuildingExpansionTypes)
	{
		const bool bIsUniqueType = EachOption.BxpConstructionRules.ConstructionType ==
			EBxpConstructionType::AtBuildingOrigin;
		// if (bIsUniqueType && GetHasAlreadyBuiltType(EachOption.ExpansionType))
		// {
		// 	const FString BxpName = Global_GetBxpTypeEnumAsString(EachOption.ExpansionType);
		// 	RTSFunctionLibrary::PrintString("The bxp option: " + BxpName +
		// 		" has already been built and depends on origin location! Cannot add it again. "
		// 		"\n See IBuildingExpansionOwner::GetUnlockedBuildingExpansionTypes");
		// 	continue;
		// }
		UnlockedBxpData.Add(EachOption);
	}
	return UnlockedBxpData;
}

EBxpOptionSection IBuildingExpansionOwner::GetBxpOptionTypeFromBxpType(const EBuildingExpansionType BxpType) const
{
	auto Types = GetUnlockedBuildingExpansionTypes();
	for(auto EachOption : Types)
	{
		if(EachOption.ExpansionType == BxpType)
		{
			return EachOption.Section;
		}
	}
	RTSFunctionLibrary::ReportError("Could not find bxp option type from bxp type!"
		"\n See IBuildingExpansionOwner::GetBxpOptionTypeFromBxpType");
	return EBxpOptionSection::BOS_Tech;
}

bool IBuildingExpansionOwner::IsBuildingAbleToExpand() const
{
	return false;
}

void IBuildingExpansionOwner::BatchAsyncLoadAttachedPackedBxps(
	const ACPPController* PlayerController,
	const TScriptInterface<IBuildingExpansionOwner>& OwnerSmartPointer) const
{
	if (not IsValid(PlayerController) || not OwnerSmartPointer)
	{
		RTSFunctionLibrary::ReportError("cannot batch instant place bxps for IBuildingExp owner"
			"as the player controller is invalid or the Smart pointer is invalid!");
		return;
	}
	ARTSAsyncSpawner* AsyncSpawner = PlayerController->GetRTSAsyncSpawner();
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Cannot batch instant place bxps for IBuildingExp owner"
			"as the player controller does not have a valid RTSAsyncSpawner!");
		return;
	}
	const TArray<FBuildingExpansionItem>& BuildingExpansionItems = GetBuildingExpansions();
	TArray<EBuildingExpansionType> ToSpawnBxps;
	TArray<int32> ToSpawnBxpIndexes;
	// Used on instant placement in controller; to set up the bxp at the correct spot.
	TArray<FBxpOptionData> ToSpawnBxpTypeAndConstructionRules;
	int32 BxpIndex = 0;
	for (auto EachBxp : BuildingExpansionItems)
	{
		if (EachBxp.ExpansionStatus != EBuildingExpansionStatus::BXS_PackedUp)
		{
			continue;
		}
		const bool bIsSocketConstruction = EachBxp.BxpConstructionRules.ConstructionType ==
			EBxpConstructionType::Socket;
		const bool bIsOriginConstruction = EachBxp.BxpConstructionRules.ConstructionType ==
			EBxpConstructionType::AtBuildingOrigin;
		if (bIsOriginConstruction || bIsSocketConstruction)
		{
			ToSpawnBxps.Add(EachBxp.ExpansionType);
			ToSpawnBxpIndexes.Add(BxpIndex);
			FBxpOptionData OptionData;
			OptionData.ExpansionType = EachBxp.ExpansionType;
			OptionData.BxpConstructionRules = EachBxp.BxpConstructionRules;
			ToSpawnBxpTypeAndConstructionRules.Add(OptionData);
		}
		++BxpIndex;
	}
	AsyncSpawner->AsyncBatchLoadInstantPlaceExpansions(
		ToSpawnBxps,
		ToSpawnBxpIndexes,
		OwnerSmartPointer,
		ToSpawnBxpTypeAndConstructionRules);

	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	// Prevents packed expansions (that are attached)
	// that are manually clicked in the UI from being loaded individually as we would for free placement bxps.
	BuildingExpansionData.SetIsAsyncBatchLoadingPackedExpansions(true);
}

void IBuildingExpansionOwner::BatchBxp_OnInstantPlacementBxpsSpawned(
	TArray<ABuildingExpansion*> SpawnedBxps,
	TArray<int32> BxpItemIndices, TArray<FBxpOptionData> BxpTypesAndConstructionRules,
	const ACPPController* PlayerController, const TScriptInterface<IBuildingExpansionOwner>& OwnerSmartPointer) const
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	BuildingExpansionData.SetIsAsyncBatchLoadingPackedExpansions(false);
	// Cleans up bxps if something goes wrong.
	if (not BatchBxp_EnsureInstantPlacementRequestValid(SpawnedBxps, PlayerController, OwnerSmartPointer))
	{
		return;
	}
	if (not IsBuildingAbleToExpand())
	{
		// Reset all indices back to packed state!
		BatchBxp_OnBuildingCanNoLongerExpand(SpawnedBxps, BxpItemIndices);
		return;
	}
	// The player controller function that handles bxp entry and ui updates for single loaded instant placement bxps
	// will here be used on the full batch which is why that function is public.
	for (int i = 0; i < SpawnedBxps.Num(); ++i)
	{
		ABuildingExpansion* Bxp = SpawnedBxps[i];
		if (not IsValid(Bxp))
		{
			RTSFunctionLibrary::ReportError(
				"Invalid bxp spawned at index: " + FString::FromInt(i) +
				"\n in IBuildingExpansionOwner::BatchBxp_OnInstantPlacementBxpsSpawned");
			ResetEntryAndUpdateUI(BxpItemIndices[i], true);
			continue;
		}
		const int32 BxpIndex = BxpItemIndices[i];
		const FBxpOptionData& BxpTypeAndConstructionRules = BxpTypesAndConstructionRules[i];
		PlayerController->OnBxpSpawnedAsync_InstantPlacement(
			SpawnedBxps[i],
			OwnerSmartPointer,
			BxpTypeAndConstructionRules,
			BxpIndex);
	}
}


void IBuildingExpansionOwner::OnBuildingExpansionCreated(
	ABuildingExpansion* BuildingExpansion,
	const int BuildingExpansionIndex,
	const FBxpOptionData& BuildingExpansionTypeData,
	const bool bIsUnpackedExpansion,
	const TScriptInterface<IBuildingExpansionOwner>& OwnerScriptInterface)
{
	// Get location of actor implementing this interface.
	const AActor* Owner = Cast<AActor>(this);

	if (!RTSFunctionLibrary::RTSIsValid(BuildingExpansion))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to spawn building expansion of owner: " + Owner->GetName() +
			"\n Cannot init array entry at OnBuildingExpansionCreated."
			"\n as bxp is invalid or null");
		return;
	}

	const EBuildingExpansionStatus Status = bIsUnpackedExpansion
		                                        ? EBuildingExpansionStatus::BXS_LookingForUnpackingLocation
		                                        : EBuildingExpansionStatus::BXS_LookingForPlacement;

	// Update the array of building expansions with the new expansion.
	// Needs to be initialised before the expansion calls OnBuildingExpansionCreatedByOwner.
	InitBuildingExpansionEntry(BuildingExpansionIndex, BuildingExpansionTypeData,
	                           Status, BuildingExpansion);

	// Update the main game UI with the new widget state if this is the main selected actor.
	BuildingExpansion->OnBuildingExpansionCreatedByOwner(OwnerScriptInterface, Status);

	// hide the building expansion until a location is determined.
	BuildingExpansion->SetActorHiddenInGame(true);

	// Disable all collision for the building expansion.
	BuildingExpansion->SetActorEnableCollision(false);
}


void IBuildingExpansionOwner::DestroyBuildingExpansion(ABuildingExpansion* BuildingExpansion,
                                                       const bool bDestroyPackedBxp)
{
	if (IsValid(BuildingExpansion))
	{
		OnBuildingExpansionDestroyed(BuildingExpansion, bDestroyPackedBxp);
		BuildingExpansion->Destroy();
		return;
	}
	RTSFunctionLibrary::ReportError("Building expansion is null! or invalid!"
		"\n At function DestroyBuildingExpansion in BuildingExpansionOwner.cpp"
		"\n Owner: " + GetOwnerName());
}

void IBuildingExpansionOwner::UpdateExpansionData(
	const ABuildingExpansion* BuildingExpansion,
	const bool bCalledUponDestroy) const
{
	if (not IsValid(BuildingExpansion))
	{
		if (not bCalledUponDestroy)
		{
			RTSFunctionLibrary::ReportError("Building expansion is null or Invalid!"
				"\n At function UpdateExpansionData in BuildingExpansionOwner.cpp"
				"\n Owner: " + GetOwnerName());
		}
		return;
	}

	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (int i = 0; i < BuildingExpansionData.M_TBuildingExpansionItems.Num(); ++i)
	{
		if (BuildingExpansionData.M_TBuildingExpansionItems[i].Expansion == BuildingExpansion)
		{
			BuildingExpansionData.M_TBuildingExpansionItems[i].ExpansionStatus = BuildingExpansion->
				GetBuildingExpansionStatus();
			const FBxpConstructionRules& BxpConstructionRules =
				BuildingExpansionData.M_TBuildingExpansionItems[i].BxpConstructionRules;
			// Update the main game UI with the new widget state if this is the main selected actor.
			BuildingExpansionData.UpdateMainGameUIWithStatusChanges(i,
			                                                        BuildingExpansion->GetBuildingExpansionStatus(),
			                                                        BuildingExpansionData.M_TBuildingExpansionItems[
				                                                        i].
			                                                        ExpansionType,
			                                                        BxpConstructionRules);
			break;
		}
	}
}

void IBuildingExpansionOwner::OnBxpConstructionStartUpdateExpansionData(
	const ABuildingExpansion* BuildingExpansion,
	const FRotator& BxpRotation, const FName& AttachedToSocketName) const
{
	if (not IsValid(BuildingExpansion))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to update the constructionStart--Expansion data on building expansion owner"
			"but the provided building expansion is not valid!");
		return;
	}
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (int i = 0; i < BuildingExpansionData.M_TBuildingExpansionItems.Num(); ++i)
	{
		if (BuildingExpansionData.M_TBuildingExpansionItems[i].Expansion == BuildingExpansion)
		{
			// Update the status and socket name of the building expansion.
			BuildingExpansionData.M_TBuildingExpansionItems[i].BxpConstructionRules.SocketName = AttachedToSocketName;
			BuildingExpansionData.M_TBuildingExpansionItems[i].ExpansionStatus = BuildingExpansion->
				GetBuildingExpansionStatus();
			const auto& [Expansion, ExpansionType, ExpansionStatus, BxpConstructionRules] =
				BuildingExpansionData.M_TBuildingExpansionItems[i];

			// Update the main game UI with the new widget state if this is the main selected actor.
			BuildingExpansionData.UpdateMainGameUIWithStatusChanges(i,
			                                                        ExpansionStatus,
			                                                        ExpansionType,
			                                                        BxpConstructionRules);
			break;
		}
	}
}

void IBuildingExpansionOwner::OnBuildingExpansionDestroyed(
	ABuildingExpansion* BuildingExpansion,
	const bool bDestroyedPackedExpansion)
{
	if (not IsValid(BuildingExpansion))
	{
		RTSFunctionLibrary::ReportError("Building expansion is null or invalid!"
			"\n At function OnBuildingExpansionDestroyed in BuildingExpansionOwner.cpp"
			"\n Owner: " + GetOwnerName());
	}

	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	// Get the index of the building expansion in the array.
	const int Index = BuildingExpansionData.M_TBuildingExpansionItems.FindLastByPredicate(
		[&BuildingExpansion](const FBuildingExpansionItem& ExpansionItem)
		{
			return ExpansionItem.Expansion == BuildingExpansion;
		});
	if (Index != INDEX_NONE)
	{
		ResetEntryAndUpdateUI(Index, bDestroyedPackedExpansion);
	}
}

TArray<FName> IBuildingExpansionOwner::GetOccupiedSocketNames(const int32 ExpansionSlotToIgnore) const
{
	TArray<FBuildingExpansionItem>& BuildingExpansionItems = GetBuildingExpansionData().M_TBuildingExpansionItems;

	TArray<FName> Sockets = {};
	int32 Index = 0;
	for (const auto EachBxpItem : BuildingExpansionItems)
	{
		const auto MyConstructionRules = EachBxpItem.BxpConstructionRules;
		if (MyConstructionRules.SocketName == NAME_None &&
			MyConstructionRules.ConstructionType == EBxpConstructionType::Socket)
		{
			if (not Index == ExpansionSlotToIgnore)
			{
				Error_BxpSetToSocketNoName(EachBxpItem.Expansion);
				continue;
			}
			// ignore this one.
			continue;
		}
		if (MyConstructionRules.SocketName != NAME_None &&
			MyConstructionRules.ConstructionType != EBxpConstructionType::Socket)
		{
			Error_BxpNoSocketRuleButHasSocketName(EachBxpItem.Expansion, MyConstructionRules.SocketName);
			continue;
		}
		if (MyConstructionRules.SocketName != NAME_None && MyConstructionRules.ConstructionType ==
			EBxpConstructionType::Socket)
		{
			Sockets.Add(MyConstructionRules.SocketName);
		}
		++Index;
	}
	return Sockets;
}

bool IBuildingExpansionOwner::HasBxpItemOfType(const EBuildingExpansionType Type) const
{
	if (Type == EBuildingExpansionType::BXT_Invalid)
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to check if a building expansion owner has a bxp item of type BXT_Invalid!"
			"\n See IBuildingExpansionOwner::HasBxpItemOfType");
		return false;
	}
	TArray<FBuildingExpansionItem>& BuildingExpansionItems = GetBuildingExpansionData().M_TBuildingExpansionItems;
	for (const auto eachBxpItem : BuildingExpansionItems)
	{
		if (eachBxpItem.ExpansionType == Type)
		{
			return true;
		}
	}
	return false;
}

EBuildingExpansionType IBuildingExpansionOwner::GetTypeOfBxpLookingForPlacement(bool& bOutIsLookingToUnpack) const
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (const auto& EachBxpItem : BuildingExpansionData.M_TBuildingExpansionItems)
	{
		if (EachBxpItem.ExpansionStatus == EBuildingExpansionStatus::BXS_LookingForPlacement ||
			EachBxpItem.ExpansionStatus == EBuildingExpansionStatus::BXS_LookingForUnpackingLocation)
		{
			bOutIsLookingToUnpack = EachBxpItem.ExpansionStatus ==
				EBuildingExpansionStatus::BXS_LookingForUnpackingLocation;
			return EachBxpItem.ExpansionType;
		}
	}
	return EBuildingExpansionType::BXT_Invalid;
}


void IBuildingExpansionOwner::InitBuildingExpansionOptions(
	const int NewAmountBuildingExpansions,
	const TArray<FBxpOptionData> NewUnlockedBuildingExpansionTypes)
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	BuildingExpansionData.M_TUnlockedBuildingExpansionTypes = NewUnlockedBuildingExpansionTypes;

	// Sets all elements with status EBuildingExpansionStatus::BES_NotBuilt;
	// and with type EBuildingExpansionType::BXT_Invalid;
	BuildingExpansionData.M_TBuildingExpansionItems.SetNum(NewAmountBuildingExpansions);
}

void IBuildingExpansionOwner::InitBuildingExpansionEntry(
	const int BuildingExpansionIndex,
	const FBxpOptionData& BxpConstructionRulesAndType,
	const EBuildingExpansionStatus BuildingExpansionStatus,
	ABuildingExpansion* BuildingExpansion) const
{
	if (IsValid(BuildingExpansion))
	{
		UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
		BuildingExpansionData.M_TBuildingExpansionItems[BuildingExpansionIndex].Expansion = BuildingExpansion;
		BuildingExpansionData.M_TBuildingExpansionItems[BuildingExpansionIndex].ExpansionType =
			BxpConstructionRulesAndType.
			ExpansionType;
		BuildingExpansionData.M_TBuildingExpansionItems[BuildingExpansionIndex].ExpansionStatus =
			BuildingExpansionStatus;
		// After spawning the bxp data array has now been updated with the construction rules of this bxp.
		BuildingExpansionData.M_TBuildingExpansionItems[BuildingExpansionIndex].BxpConstructionRules =
			BxpConstructionRulesAndType.BxpConstructionRules;
	}
}

void IBuildingExpansionOwner::ResetEntryAndUpdateUI(const int32 Index, const bool bResetForPackedUpExpansion) const
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	// If we destroy a packed expansion we set the status to packed.
	const EBuildingExpansionStatus Status = bResetForPackedUpExpansion
		                                        ? EBuildingExpansionStatus::BXS_PackedUp
		                                        : EBuildingExpansionStatus::BXS_UnlockedButNotBuild;
	// If we destroy a packed expansion we keep the type.
	const EBuildingExpansionType Type = bResetForPackedUpExpansion
		                                    ? BuildingExpansionData.M_TBuildingExpansionItems[Index].
		                                    ExpansionType
		                                    : EBuildingExpansionType::BXT_Invalid;
	// Update the data in our expansion array.
	BuildingExpansionData.M_TBuildingExpansionItems[Index].Expansion = nullptr;
	BuildingExpansionData.M_TBuildingExpansionItems[Index].ExpansionStatus = Status;
	BuildingExpansionData.M_TBuildingExpansionItems[Index].ExpansionType = Type;

	const FBxpConstructionRules EmptyRules = FBxpConstructionRules();
	FBxpConstructionRules NewRules;
	if (bResetForPackedUpExpansion)
	{
		// This was a packed expansion, we need to have access to the construction rules for a next placement.
		NewRules = BuildingExpansionData.M_TBuildingExpansionItems[Index].BxpConstructionRules;
	}
	else
	{
		// This was the first expansion for this slot that is now cancelled; clear construction rules.
		BuildingExpansionData.M_TBuildingExpansionItems[Index].BxpConstructionRules = EmptyRules;
		NewRules = EmptyRules;
	}
	// Update the main game UI with the new widget state if this is the main selected actor.
	BuildingExpansionData.UpdateMainGameUIWithStatusChanges(Index,
	                                                        Status,
	                                                        Type, NewRules);
}

void IBuildingExpansionOwner::StartPackUpAllExpansions(const float TotalOwnerPackUpTime) const
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (const auto [Expansion, ExpansionType, ExpansionStatus, ConstructionRules] : BuildingExpansionData.
	     M_TBuildingExpansionItems)
	{
		if (Expansion)
		{
			Expansion->StartPackUpBuildingExpansion(TotalOwnerPackUpTime);
		}
	}
}

void IBuildingExpansionOwner::CancelPackUpExpansions() const
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (const auto EachExpElm : BuildingExpansionData.M_TBuildingExpansionItems)
	{
		if (EachExpElm.Expansion)
		{
			EachExpElm.Expansion->CancelPackUpBuildingExpansion();
		}
	}
}

void IBuildingExpansionOwner::FinishPackUpAllExpansions() const
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (const auto EachExpElm : BuildingExpansionData.M_TBuildingExpansionItems)
	{
		if (EachExpElm.Expansion)
		{
			EachExpElm.Expansion->FinishPackUpExpansion();
		}
	}
}

bool IBuildingExpansionOwner::GetHasAlreadyBuiltType(const EBuildingExpansionType BuildingExpansionType) const
{
	UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (const auto& ExpansionItem : BuildingExpansionData.M_TBuildingExpansionItems)
	{
		if (ExpansionItem.ExpansionType == BuildingExpansionType)
		{
			// Already built one of these!
			return true;
		}
	}
	return false;
}

void IBuildingExpansionOwner::Error_BxpSetToSocketNoName(const ABuildingExpansion* Bxp) const
{
	const FString BxpName = IsValid(Bxp) ? Bxp->GetName() : "Invalid Bxp";
	const FString OwnerName = GetOwnerName();
	RTSFunctionLibrary::ReportError(
		"When checking occupied sockets at IBuildingExpansionOwner::GetOccupiedSocketNames"
		"\n we found a Bxp item with a NONE socket name but set to use sockets in the construction rules!"
		"\n Bxp Name: " + BxpName +
		"\n Owner Name: " + OwnerName);
}

void IBuildingExpansionOwner::Error_BxpNoSocketRuleButHasSocketName(
	const ABuildingExpansion* Bxp,
	const FName& SocketName) const
{
	const FString BxpName = IsValid(Bxp) ? Bxp->GetName() : "Invalid Bxp";
	const FString OwnerName = GetOwnerName();
	RTSFunctionLibrary::ReportError(
		"When Checking occupied sockets at IBuildingExpansionOWner::GetOccupiedSocketNames"
		"\n We found a Bxp item set to NOT USE SOCKETS in construction rules but has"
		"a socket at the bxp item with name: " + SocketName.ToString() +
		"\n Bxp Name: " + BxpName +
		"\n Owner Name: " + OwnerName);
}

bool IBuildingExpansionOwner::BatchBxp_EnsureInstantPlacementRequestValid(TArray<ABuildingExpansion*> SpawnedBxps,
                                                                          const ACPPController* PlayerController,
                                                                          const TScriptInterface<
	                                                                          IBuildingExpansionOwner>&
                                                                          OwnerSmartPointer) const
{
	if (not PlayerController || not OwnerSmartPointer)
	{
		RTSFunctionLibrary::ReportError("Bxp batch loaded and spawned but failed to init entries"
			"as Bxp owner smart pointer or player controller is null!"
			"\n See IBuildingExpansionOwner::BatchBxp_OnInstantPlacementBxpsSpawned");
		for (auto EachBxp : SpawnedBxps)
		{
			if (not IsValid(EachBxp))
			{
				continue;
			}
			EachBxp->Destroy();
		}
		return false;
	}
	return true;
}

void IBuildingExpansionOwner::BatchBxp_OnBuildingCanNoLongerExpand(
	TArray<ABuildingExpansion*> SpawnedBxps,
	TArray<int32> IndicesToReset) const
{
	if constexpr (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		const FString OwnerName = GetOwnerName();
		const FString HowMany = FString::FromInt(SpawnedBxps.Num());
		RTSFunctionLibrary::DisplayNotification(FText::FromString(""
			"The building owner: " + OwnerName +
			"Has loaded a batch of bxps with amount of " + HowMany + " expansions."
			"\n However, the building can no longer expand; destroying them now and"
			"resetting the building expansion entries to packed state!"));
	}
	for (auto EachBxp : SpawnedBxps)
	{
		if (IsValid(EachBxp))
		{
			EachBxp->Destroy();
		}
	}
	const UBuildingExpansionOwnerComp& BuildingExpansionData = GetBuildingExpansionData();
	for (const auto Index : IndicesToReset)
	{
		if (Index >= 0 && Index < BuildingExpansionData.M_TBuildingExpansionItems.Num())
		{
			ResetEntryAndUpdateUI(Index, true);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Invalid index to reset building expansion entry at"
				"\n See IBuildingExpansionOwner::BatchBxp_OnBuildingCanNoLongerExpand");
		}
	}
}
