// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ResourceDropOff.h"

#include "NavigationSystem.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "Components/MeshComponent.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GameResourceManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageOwner.h"
#include "RTS_Survival/Resources/UnloadStation/ResourceUnloadStation.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values for this component's properties
UResourceDropOff::UResourceDropOff()
	: M_OwnerMeshComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
	bIsDropOffActive = true;
}

int32 UResourceDropOff::DropOffResources(const ERTSResourceType ResourceType, const int32 Amount)
{
	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
	{
		DebugDroppingOff(ResourceType, Amount);
	}

	if (not M_ResourceDropOffCapacity.Contains(ResourceType))
	{
		RTSFunctionLibrary::ReportError("Resource type not found in drop off capacity!"
			"\n ResourceDropOff: " + GetName());
		return Amount;
	}
	const int32 AmountLeft = AddDroppedOffResources(ResourceType, Amount);
	CreateTextOfDropOff(ResourceType, Amount - AmountLeft);
	return AmountLeft;
}

int32 UResourceDropOff::DropOffResourcesFromCrane(const ERTSResourceType ResourceType, const int32 Amount,
                                                  const FVector& TextLocation)
{
	if (not M_ResourceDropOffCapacity.Contains(ResourceType))
	{
		RTSFunctionLibrary::ReportError("Crane load of a resource the drop off does not store!"
			"\n ResourceDropOff: " + GetName());
		return Amount;
	}
	const int32 AmountLeft = AddDroppedOffResources(ResourceType, Amount);
	CreateTextOfDropOffAt(ResourceType, Amount - AmountLeft, TextLocation);
	return AmountLeft;
}

int32 UResourceDropOff::AddDroppedOffResources(const ERTSResourceType ResourceType, const int32 Amount)
{
	const int32 AmountLeft = M_ResourceDropOffCapacity[ResourceType].AddResource(Amount);
	// An AI player's drop-off keeps the delivery in its cargo slots only: that is its stockpile.
	if (UPlayerResourceManager* HumanResourceManager = GetHumanPlayerResourceManager())
	{
		// Update player resource system without affecting the DropOffs as we will store the resources locally.
		HumanResourceManager->DropOffNoVisualUpdate_AddResource(ResourceType, Amount - AmountLeft);
	}
	UpdateDropOffVisuals(ResourceType);
	return AmountLeft;
}

void UResourceDropOff::SetUnloadStation(AResourceUnloadStation* Station)
{
	M_UnloadStation = Station;
}

AResourceUnloadStation* UResourceDropOff::GetUnloadStation() const
{
	return M_UnloadStation.Get();
}

void UResourceDropOff::AddResourcesNotHarvested(const ERTSResourceType ResourceType, const int32 Amount)
{
	if (not M_ResourceDropOffCapacity.Contains(ResourceType) || Amount + M_ResourceDropOffCapacity[ResourceType].
		CurrentAmount > M_ResourceDropOffCapacity[ResourceType].MaxCapacity)
	{
		RTSFunctionLibrary::ReportError("Resource type not found in drop off capacity or already full!"
			"\n ResourceDropOff: " + GetName() +
			"\n At function AddResourcesNotHarvested"
			"\n Resource: " + Global_GetResourceTypeAsString(ResourceType) +
			"\n Requested amount to add: " + FString::FromInt(Amount));
		return;
	}
	M_ResourceDropOffCapacity[ResourceType].AddResource(Amount);
	UpdateDropOffVisuals(ResourceType);
}

void UResourceDropOff::UpgradeDropOffCapacity(const ERTSResourceType ResourceType, const int32 Amount,
                                              const bool bAlsoAddAmount)
{
	if (not M_ResourceDropOffCapacity.Contains(ResourceType))
	{
		const FString AlsoUpdateAmount = bAlsoAddAmount ? "Also add amount: " : "Only upgrade capacity: ";
		RTSFunctionLibrary::ReportError("Attempted to upgrade DropOff but the resource type is not supported!"
			"\n Resource type: " + Global_GetResourceTypeAsString(ResourceType) +
			"\n ResourceDropOff: " + GetName() +
			"\n wanted behaviour: " + AlsoUpdateAmount + " " + FString::FromInt(Amount));
		return;
	}
	M_ResourceDropOffCapacity[ResourceType].MaxCapacity += Amount;
	if (bAlsoAddAmount)
	{
		M_ResourceDropOffCapacity[ResourceType].CurrentAmount += Amount;
	}
	UpdateDropOffVisuals(ResourceType);
}

void UResourceDropOff::AddExpansionStorage(const ERTSResourceType ResourceType, const int32 ExtraCapacity)
{
	FHarvesterCargoSlot* CargoSlot = M_ResourceDropOffCapacity.Find(ResourceType);
	if (not CargoSlot)
	{
		RTSFunctionLibrary::ReportError("Attempted to add expansion storage for a resource this drop off does not hold!"
			"\n Resource type: " + Global_GetResourceTypeAsString(ResourceType) +
			"\n ResourceDropOff: " + GetName());
		return;
	}
	CargoSlot->MaxCapacity += ExtraCapacity;
	if (UPlayerResourceManager* HumanResourceManager = GetHumanPlayerResourceManager())
	{
		constexpr int32 NoResourcesLost = 0;
		HumanResourceManager->OnDropOffStorageChanged(this, ResourceType, ExtraCapacity, NoResourcesLost);
	}
	UpdateDropOffVisuals(ResourceType);
}

void UResourceDropOff::RemoveExpansionStorage(const ERTSResourceType ResourceType, const int32 ExtraCapacity)
{
	FHarvesterCargoSlot* CargoSlot = M_ResourceDropOffCapacity.Find(ResourceType);
	if (not CargoSlot)
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to remove expansion storage for a resource this drop off does not hold!"
			"\n Resource type: " + Global_GetResourceTypeAsString(ResourceType) +
			"\n ResourceDropOff: " + GetName());
		return;
	}
	const int32 CapacityRemoved = FMath::Min(ExtraCapacity, CargoSlot->MaxCapacity);
	CargoSlot->MaxCapacity -= CapacityRemoved;
	const int32 ResourcesLost = FMath::Max(0, CargoSlot->CurrentAmount - CargoSlot->MaxCapacity);
	CargoSlot->CurrentAmount -= ResourcesLost;
	if (UPlayerResourceManager* HumanResourceManager = GetHumanPlayerResourceManager())
	{
		HumanResourceManager->OnDropOffStorageChanged(this, ResourceType, -CapacityRemoved, ResourcesLost);
	}
	UpdateDropOffVisuals(ResourceType);
}

void UResourceDropOff::RemoveResources(ERTSResourceType ResourceType, int32 Amount)
{
	if (not M_ResourceDropOffCapacity.Contains(ResourceType) || M_ResourceDropOffCapacity[ResourceType].CurrentAmount <
		Amount)
	{
		RTSFunctionLibrary::ReportError(
			"Resource type not found in drop off capacity or not enough resources to remove!"
			"\n At function UResourceDropOff::RemoveResources"
			"\n Amount requested to remove: " + FString::FromInt(Amount) +
			"\n Of Resource type: " + Global_GetResourceTypeAsString(ResourceType) +
			"\n ResourceDropOff: " + GetName());
		return;
	}
	M_ResourceDropOffCapacity[ResourceType].CurrentAmount -= Amount;
	UpdateDropOffVisuals(ResourceType);
}

FVector UResourceDropOff::GetDropOffLocationNotThreadSafe() const
{
	const TWeakObjectPtr<AActor> Owner = GetOwner();
	if (not Owner.IsValid(false, false))
	{
		RTSFunctionLibrary::ReportError("Owner is not valid on resource dropoff!"
			"\n ResourceDropOff: " + GetName());
		return FVector::ZeroVector;
	}

	// A crane whose bay covers the old drop-off point sends bay-less harvesters to its overflow point instead.
	FVector OverflowLocation;
	if (M_UnloadStation.IsValid() && M_UnloadStation->GetOverflowDropOffLocation(OverflowLocation))
	{
		return OverflowLocation;
	}

	if (not GetIsValidOwnerMeshComponent())
	{
		return Owner->GetActorLocation();
	}

	return M_OwnerMeshComponent->GetSocketLocation(M_DropOffSocketName);
}

bool UResourceDropOff::GetProjectedBackupDropOffLocation(
	const FVector& HarvesterLocation,
	FVector& OutBackupLocation) const
{
	using DeveloperSettings::GamePlay::Navigation::HarvesterBackupDropOffMeshClearance;
	using DeveloperSettings::GamePlay::Navigation::HarvesterPositionProjectedExtent;

	if (not GetIsValidOwnerMeshComponent())
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		RTSFunctionLibrary::ReportError("Cannot calculate a backup drop-off location without a valid owner!"
			"\n ResourceDropOff: " + GetName());
		return false;
	}

	FVector DirectionToHarvester = HarvesterLocation - Owner->GetActorLocation();
	DirectionToHarvester.Z = 0.f;
	if (not DirectionToHarvester.Normalize())
	{
		DirectionToHarvester = Owner->GetActorForwardVector().GetSafeNormal2D();
	}

	const float OwnerMeshDistance = GetOwnerMeshDistanceTowardsHarvester(*Owner, DirectionToHarvester);
	if (OwnerMeshDistance <= 0.f)
	{
		return false;
	}

	const float BackupLocationOffset = OwnerMeshDistance
		+ HarvesterBackupDropOffMeshClearance;
	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector UnprojectedBackupLocation = OwnerLocation
		+ DirectionToHarvester * BackupLocationOffset;

	const UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not IsValid(NavigationSystem))
	{
		return false;
	}

	FNavLocation ProjectedBackupLocation;
	const FVector ProjectionExtent(HarvesterPositionProjectedExtent);
	if (not NavigationSystem->ProjectPointToNavigation(
		UnprojectedBackupLocation,
		ProjectedBackupLocation,
		ProjectionExtent))
	{
		return false;
	}

	OutBackupLocation = ProjectedBackupLocation.Location;
	return true;
}

bool UResourceDropOff::GetHasCapacityForDropOff(const ERTSResourceType ResourceType, const int32 Amount) const
{
	if (M_ResourceDropOffCapacity.Contains(ResourceType))
	{
		return M_ResourceDropOffCapacity[ResourceType].HasCapacityForAmount(Amount);
	}
	return false;
}

int32 UResourceDropOff::GetDropOffCapacity(const ERTSResourceType ResourceType) const
{
	int32 Amount = 0;
	if (M_ResourceDropOffCapacity.Contains(ResourceType))
	{
		Amount = M_ResourceDropOffCapacity[ResourceType].MaxCapacity - M_ResourceDropOffCapacity[ResourceType].
			CurrentAmount;
	}
	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
	{
		if (IsValid(GetOwner()))
		{
			FString DebugString = "DropOff Capacity check: res: " + Global_GetResourceTypeAsString(ResourceType) +
				" amount: " + FString::FromInt(Amount);
			DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + (0, 0, 400), DebugString, nullptr,
			                FColor::Purple, 1.f, false);
		}
	}
	return Amount;
}

int32 UResourceDropOff::GetDropOffAmountStored(ERTSResourceType ResourceType) const
{
	if (M_ResourceDropOffCapacity.Contains(ResourceType))
	{
		return M_ResourceDropOffCapacity[ResourceType].CurrentAmount;
	}
	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::ReportError("Drop off amount requested but does not support resource type!"
			"\n ResourceDropOff: " + GetName() +
			"\n Resource type: " + Global_GetResourceTypeAsString(ResourceType));
	}
	return 0;
}

TMap<ERTSResourceType, FHarvesterCargoSlot> UResourceDropOff::GetResourceDropOffCapacity() const
{
	return M_ResourceDropOffCapacity;
}

TArray<ERTSResourceType> UResourceDropOff::GetResourcesSupported() const
{
	TArray<ERTSResourceType> MySupportedResources;
	for (auto EachResource : M_ResourceDropOffCapacity)
	{
		MySupportedResources.Add(EachResource.Key);
	}
	return MySupportedResources;
}

// Called when the game starts
void UResourceDropOff::BeginPlay()
{
	Super::BeginPlay();
	if (ACPPController* NewPlayerController =
			Cast<ACPPController>(UGameplayStatics::GetPlayerController(this, 0));
		IsValid(NewPlayerController))
	{
		M_PlayerResourceManager = NewPlayerController->GetPlayerResourceManager();
		// Only the human's drop-offs need it; an AI drop-off never touches the human's resources.
		if (GetIsOwnedByHumanPlayer())
		{
			(void)GetIsValidPlayerResourceManager();
		}
	}
}

void UResourceDropOff::BeginDestroy()
{
	DeRegisterFromGameResourceManager();
	Super::BeginDestroy();
}

void UResourceDropOff::InitResourceDropOff(
	TMap<ERTSResourceType, int32> MaxDropOffPerResource,
	UMeshComponent* OwnerMeshComponent,
	FName DropOffSocketName, const float DropOffTextZOffset)
{
	M_DropOffTextZOffset = DropOffTextZOffset;
	for (auto EachResourceDropOff : MaxDropOffPerResource)
	{
		FHarvesterCargoSlot CargoSlot;
		CargoSlot.CurrentAmount = 0;
		CargoSlot.MaxCapacity = EachResourceDropOff.Value;
		CargoSlot.ResourceType = EachResourceDropOff.Key;
		M_ResourceDropOffCapacity.Add(EachResourceDropOff.Key, CargoSlot);
	}
	if (IsValid(OwnerMeshComponent))
	{
		M_OwnerMeshComponent = OwnerMeshComponent;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "OwnerMeshComponent", "InitResourceDropOff");
	}
	if (DropOffSocketName != NAME_None)
	{
		M_DropOffSocketName = DropOffSocketName;
	}
	else
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "DropOffSocketName", "InitResourceDropOff");
	}
	if (AActor* Owner = GetOwner())
	{
		if (IResourceStorageOwner* OwnerAsResourceStorage = Cast<IResourceStorageOwner>(Owner))
		{
			M_ResourceStorageOwner = TWeakInterfacePtr<IResourceStorageOwner>(OwnerAsResourceStorage);
			if (!M_ResourceStorageOwner.IsValid())
			{
				RTSFunctionLibrary::ReportNullErrorInitialisation(this, "IResourceStorageOwner", "BeginPlay");
			}
		}
		else
		{
			// AI buildings (the Soviet refinery is an ABuildingExpansion) have no storage visuals on purpose.
			bM_HasNoStorageVisualsOwner = not GetIsOwnedByHumanPlayer() || GetName().Contains("ResourceDropOff");
			if (not bM_HasNoStorageVisualsOwner)
			{
				RTSFunctionLibrary::ReportFailedCastError("Owner", "IResourceStorageOwner", "InitResourceDropOff");
			}
		}
	}
	// After initialization
	RegisterWithGameResourceManager();
	BindToOwnerOwningPlayerChanges();
}

void UResourceDropOff::MapTest_InitResourceDropOff(
	const TMap<ERTSResourceType, int32>& MaxDropOffPerResource,
	UMeshComponent* OwnerMeshComponent,
	const FName DropOffSocketName)
{
	InitResourceDropOff(MaxDropOffPerResource, OwnerMeshComponent, DropOffSocketName);
}

void UResourceDropOff::BindToOwnerOwningPlayerChanges()
{
	const AActor* Owner = GetOwner();
	URTSComponent* OwnerRTSComponent = IsValid(Owner) ? Owner->FindComponentByClass<URTSComponent>() : nullptr;
	if (not IsValid(OwnerRTSComponent))
	{
		return;
	}
	OwnerRTSComponent->OnOwningPlayerChanged.RemoveAll(this);
	OwnerRTSComponent->OnOwningPlayerChanged.AddUObject(this, &UResourceDropOff::OnOwnerOwningPlayerChanged);
}

void UResourceDropOff::OnOwnerOwningPlayerChanged(const uint8 OldOwningPlayer, const uint8 NewOwningPlayer)
{
	UGameResourceManager* GameResourceManager = GetGameResourceManager();
	if (not IsValid(GameResourceManager))
	{
		return;
	}
	// Re-register so harvesters of the new owner find it and the human's storage gains or loses it.
	GameResourceManager->RegisterResourceDropOff(this, false, static_cast<int8>(OldOwningPlayer));
	GameResourceManager->RegisterResourceDropOff(this, true, static_cast<int8>(NewOwningPlayer));
}

UGameResourceManager* UResourceDropOff::GetGameResourceManager() const
{
	const UWorld* World = GetWorld();
	const ACPPGameState* CPPGameState = IsValid(World) ? World->GetGameState<ACPPGameState>() : nullptr;
	return IsValid(CPPGameState) ? CPPGameState->GetGameResourceManager() : nullptr;
}

void UResourceDropOff::RegisterWithGameResourceManager()
{
	if (const UWorld* World = GetWorld())
	{
		if (AGameStateBase* GameStateBase = World->GetGameState())
		{
			if (ACPPGameState* CPPGameState = Cast<ACPPGameState>(GameStateBase))
			{
				if (UGameResourceManager* GameResourceManager = CPPGameState->GetGameResourceManager())
				{
					GameResourceManager->RegisterResourceDropOff(this, true, GetOwningPlayer());
				}
				else
				{
					RTSFunctionLibrary::ReportNullErrorInitialisation(this, "GameResourceManager",
					                                                  "RegisterWithGameResourceManager");
				}
			}
			else
			{
				RTSFunctionLibrary::ReportFailedCastError("GameStateBase", "CPPGameState",
				                                          "RegisterWithGameResourceManager");
			}
		}
		else
		{
			RTSFunctionLibrary::ReportNullErrorInitialisation(this, "GameStateBase",
			                                                  "RegisterWithGameResourceManager");
		}
	}
}

void UResourceDropOff::DeRegisterFromGameResourceManager()
{
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* GameStateBase = World->GetGameState())
		{
			if (ACPPGameState* CPPGameState = Cast<ACPPGameState>(GameStateBase))
			{
				if (UGameResourceManager* GameResourceManager = CPPGameState->GetGameResourceManager())
				{
					GameResourceManager->RegisterResourceDropOff(this, false, GetOwningPlayer());
				}
				else
				{
					RTSFunctionLibrary::ReportNullErrorInitialisation(this, "GameResourceManager",
					                                                  "DeRegisterFromGameResourceManager");
				}
			}
			else
			{
				RTSFunctionLibrary::ReportFailedCastError("GameStateBase", "ACPPGameState",
				                                          "DeRegisterFromGameResourceManager");
			}
		}
	}
}


void UResourceDropOff::DebugDroppingOff(const ERTSResourceType ResourceType, const int32 Amount)
{
	if (IsValid(GetOwner()))
	{
		FString DebugString = "Dropping off: " + Global_GetResourceTypeAsString(ResourceType) +
			"\n amount: " + FString::FromInt(Amount);
		DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + (0, 0, 600), DebugString, nullptr, FColor::Green,
		                1.f, false);
	}
}

int8 UResourceDropOff::GetOwningPlayer() const
{
	if (not IsValid(GetOwner()))
	{
		RTSFunctionLibrary::ReportError("Owner invalid for: " + GetName());
		return 0;
	}
	if (const URTSComponent* OwnerRTSComp = GetOwner()->FindComponentByClass<URTSComponent>(); IsValid(OwnerRTSComp))
	{
		return OwnerRTSComp->GetOwningPlayer();
	}
	RTSFunctionLibrary::ReportError("Owner does not have RTSComponent for: " + GetName()
		+ "Will assume that this DropOff is for testing and treat it asif it is owned by player 1.");
	return 1;
}

void UResourceDropOff::UpdateDropOffVisuals(const ERTSResourceType ResourceTypeChanged)
{
	if (not M_ResourceDropOffCapacity.Contains(ResourceTypeChanged))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to change resource visuals for a resource that is not in the drop off capacity!"
			"\n ResourceDropOff: " + GetName());
		return;
	}
	if (not M_ResourceStorageOwner.IsValid())
	{
		if (bM_HasNoStorageVisualsOwner || not GetIsOwnedByHumanPlayer())
		{
			return;
		}
		RTSFunctionLibrary::ReportError("DropOff is not able to update visuals as the owner is not valid!"
			"\n ResourceDropOff: " + GetName());
		return;
	}
	M_ResourceStorageOwner->OnResourceStorageChanged(
		M_ResourceDropOffCapacity[ResourceTypeChanged].GetPercentageFilled(), ResourceTypeChanged);
}

void UResourceDropOff::CreateTextOfDropOff(const ERTSResourceType ResourceType, const int32 Amount) const
{
	if (Amount <= 0 || not GetIsOwnedByHumanPlayer() || not GetIsValidOwnerMeshComponent())
	{
		return;
	}
	CreateTextOfDropOffAt(ResourceType, Amount, GetDropOffLocationNotThreadSafe() + FVector(0, 0, M_DropOffTextZOffset));
}

void UResourceDropOff::CreateTextOfDropOffAt(const ERTSResourceType ResourceType, const int32 Amount,
                                             const FVector& Location) const
{
	// The "+ amount" pop-up is the human's income feedback; AI deliveries stay silent.
	if (Amount <= 0 || not GetIsOwnedByHumanPlayer())
	{
		return;
	}
	FRTSVerticalAnimTextSettings Settings;
	Settings.DeltaZ = 150.f;
	Settings.VisibleDuration = 5.f;
	Settings.FadeOutDuration = 2.5f;

	FString BaseText = "+ " + FString::FromInt(Amount) + " " + Global_GetResourceTypeDisplayString(ResourceType);

	BaseText = FRTSRichTextConverter::MakeRTSRich(
		BaseText, FRTSRichTextConverter::ConvertResourceToRTSRichText(ResourceType));
	const TEnumAsByte<ETextJustify::Type> Justify = ETextJustify::Center;
	URTSBlueprintFunctionLibrary::RTSSpawnVerticalAnimatedTextAtLocation(this,
	                                                                     BaseText, Location, false, 0.f,
	                                                                     Justify,
	                                                                     Settings);
}

bool UResourceDropOff::GetIsOwnedByHumanPlayer() const
{
	constexpr uint8 HumanPlayer = 1;
	// Quiet on purpose (called per delivery): without an RTS component a drop-off counts as the human's, as at
	// registration (GetOwningPlayer reports that case once).
	const AActor* Owner = GetOwner();
	const URTSComponent* OwnerRTSComp = IsValid(Owner) ? Owner->FindComponentByClass<URTSComponent>() : nullptr;
	return not IsValid(OwnerRTSComp) || OwnerRTSComp->GetOwningPlayer() == HumanPlayer;
}

UPlayerResourceManager* UResourceDropOff::GetHumanPlayerResourceManager()
{
	if (not GetIsOwnedByHumanPlayer())
	{
		return nullptr;
	}
	if (not IsValid(M_PlayerResourceManager))
	{
		M_PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	}
	return GetIsValidPlayerResourceManager() ? M_PlayerResourceManager.Get() : nullptr;
}

bool UResourceDropOff::GetIsValidPlayerResourceManager() const
{
	if (IsValid(M_PlayerResourceManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PlayerResourceManager",
		"UResourceDropOff::GetIsValidPlayerResourceManager",
		this);
	return false;
}

bool UResourceDropOff::GetIsValidOwnerMeshComponent() const
{
	if (M_OwnerMeshComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerMeshComponent",
		"UResourceDropOff::GetIsValidOwnerMeshComponent",
		this);
	return false;
}

float UResourceDropOff::GetOwnerMeshDistanceTowardsHarvester(
	const AActor& Owner,
	const FVector& DirectionToHarvester) const
{
	if (not GetIsValidOwnerMeshComponent())
	{
		return 0.f;
	}

	const FTransform MeshToOwnerTransform = M_OwnerMeshComponent->GetComponentTransform().GetRelativeTransform(
		Owner.GetActorTransform());
	const FBox OwnerSpaceMeshBounds = M_OwnerMeshComponent->CalcBounds(MeshToOwnerTransform).GetBox();
	const FVector OwnerSpaceDirection = Owner.GetActorTransform().InverseTransformVectorNoScale(
		DirectionToHarvester).GetSafeNormal2D();

	const bool bIsHarvesterToLeftOrRight = FMath::Abs(OwnerSpaceDirection.Y)
		> FMath::Abs(OwnerSpaceDirection.X);
	const EAxis::Type SelectedHorizontalAxis = bIsHarvesterToLeftOrRight ? EAxis::Y : EAxis::X;
	const float DirectionOnSelectedAxis = OwnerSpaceDirection.GetComponentForAxis(SelectedHorizontalAxis);
	const float OwnerSpaceBound = DirectionOnSelectedAxis >= 0.f
		? OwnerSpaceMeshBounds.Max.GetComponentForAxis(SelectedHorizontalAxis)
		: -OwnerSpaceMeshBounds.Min.GetComponentForAxis(SelectedHorizontalAxis);
	const float OwnerScaleOnSelectedAxis = Owner.GetActorScale3D().GetAbs().GetComponentForAxis(
		SelectedHorizontalAxis);
	const float DirectionMagnitudeOnSelectedAxis = FMath::Abs(DirectionOnSelectedAxis);
	return FMath::Max(OwnerSpaceBound, 0.f) * OwnerScaleOnSelectedAxis
		/ FMath::Max(DirectionMagnitudeOnSelectedAxis, UE_SMALL_NUMBER);
}
