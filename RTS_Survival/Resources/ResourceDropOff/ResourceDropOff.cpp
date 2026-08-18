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
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"


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

	if (M_ResourceDropOffCapacity.Contains(ResourceType))
	{
		const int32 AmountLeft = M_ResourceDropOffCapacity[ResourceType].AddResource(Amount);
		if (GetIsValidPlayerResourceManager())
		{
			// Update player resource system without affecting the DropOffs as we will store the resources locally.
			M_PlayerResourceManager->DropOffNoVisualUpdate_AddResource(ResourceType, Amount - AmountLeft);
		}
		UpdateDropOffVisuals(ResourceType);
		CreateTextOfDropOff(ResourceType, Amount - AmountLeft);
		return AmountLeft;
	}
	RTSFunctionLibrary::ReportError("Resource type not found in drop off capacity!"
		"\n ResourceDropOff: " + GetName());
	return Amount;
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
		(void)GetIsValidPlayerResourceManager();
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
			if (!GetName().Contains("ResourceDropOff"))
			{
				RTSFunctionLibrary::ReportFailedCastError("Owner", "IResourceStorageOwner", "InitResourceDropOff");
			}
		}
	}
	// After initialization
	RegisterWithGameResourceManager();
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
		RTSFunctionLibrary::ReportError("DropOff is not able to update visuals as the owner is not valid!"
			"\n ResourceDropOff: " + GetName());
		return;
	}
	M_ResourceStorageOwner->OnResourceStorageChanged(
		M_ResourceDropOffCapacity[ResourceTypeChanged].GetPercentageFilled(), ResourceTypeChanged);
}

void UResourceDropOff::CreateTextOfDropOff(const ERTSResourceType ResourceType, const int32 Amount) const
{
	if (Amount <= 0 || not GetIsValidOwnerMeshComponent())
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
	FVector LocationOfDropOff = GetDropOffLocationNotThreadSafe() + FVector(0, 0, M_DropOffTextZOffset);
	const TEnumAsByte<ETextJustify::Type> Justify = ETextJustify::Center;
	URTSBlueprintFunctionLibrary::RTSSpawnVerticalAnimatedTextAtLocation(this,
	                                                                     BaseText, LocationOfDropOff, false, 0.f,
	                                                                     Justify,
	                                                                     Settings);
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
