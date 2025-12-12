// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ResourceComponent.h"

#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GameResourceManager.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "NavigationSystem.h"
#include "Sockets.h"
#include "Components/MeshComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageOwner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values for this component's properties
UResourceComponent::UResourceComponent()
	: ResourceType(ERTSResourceType::Resource_None),
	  AmountPerHarvest(0),
	  TotalAmount(0),
	  GatherType(EGatherType::Gather_None),
	  M_OriginalTotalAmount(0)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UResourceComponent::RegisterOccupiedLocation(const FHarvestLocation& HarvestingPosition, const bool bIsOccupied)
{
	if (not M_IsHarvestLocationDirectionTaken.Contains(HarvestingPosition.Direction))
	{
		const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("Failed to change state of HarvestingPosition as occupied or not."
			"\n at function RegisterOccupiedLocation in ResourceComponent.cpp."
			"\n At this resource, this PositionDirection is not recongnized: " +
			Global_GetHarvestLocationDirectionAsString(HarvestingPosition.Direction) +
			"\n Resource: " + OwnerName);
		return;
	}
	M_IsHarvestLocationDirectionTaken[HarvestingPosition.Direction] = bIsOccupied;
}


int32 UResourceComponent::HarvestResource(const int32 MaxAmountHarvesterCapacity)
{
	if (StillContainsResources())
	{
		int32 AmountHarvested = 0;
		if (TotalAmount >= AmountPerHarvest)
		{
			// We can harvest the amount that is standard on this resource
			if (MaxAmountHarvesterCapacity >= AmountPerHarvest)
			{
				TotalAmount -= AmountPerHarvest;
				AmountHarvested = AmountPerHarvest;
			}
			else
			{
				// We can only harvest a part of the standard amount.
				TotalAmount -= MaxAmountHarvesterCapacity;
				AmountHarvested = MaxAmountHarvesterCapacity;
			}
		}
		else if (MaxAmountHarvesterCapacity >= TotalAmount)
		{
			// We cannot harvest AmountPerHarvest as there is not enough, but we can take all that is left.
			AmountHarvested = TotalAmount;
			TotalAmount = 0;
		}
		else
		{
			// What is left is more than the harvester can take, take as much the harvester can store.
			TotalAmount -= MaxAmountHarvesterCapacity;
			AmountHarvested = MaxAmountHarvesterCapacity;
		}
		if (M_ResourceOwner && IsValid(M_ResourceOwner.GetObject()))
		{
			const int32 PercentageResourcesFilled = M_OriginalTotalAmount > 0
				                                        ? FMath::CeilToInt(
					                                        static_cast<float>(100 * TotalAmount /
						                                        M_OriginalTotalAmount))
				                                        : 0;
			M_ResourceOwner->OnResourceStorageChanged(PercentageResourcesFilled, ResourceType);
			if (PercentageResourcesFilled == 0)
			{
				M_ResourceOwner->OnResourceStorageEmpty();
			}
			if (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
			{
				DebugPercentage(PercentageResourcesFilled);
			}
		}
		return AmountHarvested;
	}
	return int32();
}

bool UResourceComponent::GetHarvestLocationClosestTo(const FVector& HarvesterLocation,
                                                     UHarvester* RequestingHarvester,
                                                     const float HarvesterRadius, FHarvestLocation& OutHarvestLocation)
{
	TArray<EHarvestLocationDirection> VacantDirections = GetVacantDirections();
	TArray<FHarvestLocation> PositionsFree = GenerateHarvestLocations(HarvesterRadius, GetVacantDirections(),
	                                                                  HarvesterLocation);
	if (PositionsFree.Num() == 0)
	{
		const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError(
			"Harvester Requesting harvesting location but all locations are taken on resource:"
			"\n " + OwnerName);
		return false;
	}

	PositionsFree.Sort([&HarvesterLocation](const FHarvestLocation& A, const FHarvestLocation& B) -> bool
	{
		return FVector::DistSquared(A.Location, HarvesterLocation) <
			FVector::DistSquared(B.Location, HarvesterLocation);
	});
	for (auto EachPosition : PositionsFree)
	{
		if (GetHarvesterPositionProjected(EachPosition))
		{
			OutHarvestLocation = EachPosition;
			return true;
		}
	}
	return false;
}


bool UResourceComponent::GetHarvesterPositionProjected(
	FHarvestLocation& OutHarvesterPosition) const
{
	using DeveloperSettings::GamePlay::Navigation::HarvesterPositionProjectedExtent;
	if (not IsValid(M_NavigationSystem))
	{
		return false;
	}
	FNavLocation ProjectedLocation;
	const FVector Extent = FVector(HarvesterPositionProjectedExtent, HarvesterPositionProjectedExtent,
	                               HarvesterPositionProjectedExtent);
	const FVector OriginalHarvestLocation = OutHarvesterPosition.Location;
	const bool ProjectionSuccesfull = M_NavigationSystem->ProjectPointToNavigation(
		OriginalHarvestLocation, ProjectedLocation, Extent);
	if (ProjectionSuccesfull)
	{
		OutHarvesterPosition.Location = ProjectedLocation.Location;
		return true;
	}
	return false;
}


void UResourceComponent::DebugPercentage(const int32 PercentageLeft)
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		const FVector OwnerLocation = Owner->GetActorLocation();
		const FVector DebugLocation = OwnerLocation + FVector(0.f, 0.f, 400.f);
		const FString DebugMessage = "Percentage left: " + FString::FromInt(PercentageLeft);
		DrawDebugString(GetWorld(), DebugLocation, DebugMessage, nullptr, FColor::Green, 5.f, false);
	}
}

void UResourceComponent::InitializeHarvesterPositionsAsVacant(const int32 MaxHarvesters)
{
	// Setup a hierarchy so that the postions are as far away from eachother as possible.
	TArray<EHarvestLocationDirection> DirectionHierarchy = {
		EHarvestLocationDirection::North,
		EHarvestLocationDirection::South,
		EHarvestLocationDirection::West,
		EHarvestLocationDirection::East,
		EHarvestLocationDirection::NorthEast,
		EHarvestLocationDirection::SouthWest,
		EHarvestLocationDirection::NorthWest,
		EHarvestLocationDirection::SouthEast,
	};
	for (auto EachDirection : DirectionHierarchy)
	{
		if (M_IsHarvestLocationDirectionTaken.Num() >= MaxHarvesters)
		{
			break;
		}
		// Initialize as not occupied.
		M_IsHarvestLocationDirectionTaken.Add(EachDirection, false);
	}
	InitializeDirectionVectors();
}

void UResourceComponent::InitializeDirectionVectors()
{
	// Clear any existing direction vectors to avoid stale data
	M_DirectionVectors.Empty();

	// Iterate through each direction in M_IsHarvestLocationDirectionTaken
	for (const auto& EachDirectionPair : M_IsHarvestLocationDirectionTaken)
	{
		EHarvestLocationDirection Direction = EachDirectionPair.Key;

		// Skip the INVALID direction as it does not represent a usable direction
		if (Direction == EHarvestLocationDirection::INVALID)
		{
			continue;
		}

		FVector DirectionVector;

		// Determine the base vector for each direction without considering rotation
		switch (Direction)
		{
		case EHarvestLocationDirection::North:
			// Forward direction
			DirectionVector = FVector(1, 0, 0);
			break;

		case EHarvestLocationDirection::NorthEast:
			// Diagonal between forward and right
			DirectionVector = FVector(1, 1, 0).GetSafeNormal();
			break;

		case EHarvestLocationDirection::East:
			// Right direction
			DirectionVector = FVector(0, 1, 0);
			break;

		case EHarvestLocationDirection::SouthEast:
			// Diagonal between backward and right
			DirectionVector = FVector(-1, 1, 0).GetSafeNormal();
			break;

		case EHarvestLocationDirection::South:
			// Backward direction
			DirectionVector = FVector(-1, 0, 0);
			break;

		case EHarvestLocationDirection::SouthWest:
			// Diagonal between backward and left
			DirectionVector = FVector(-1, -1, 0).GetSafeNormal();
			break;

		case EHarvestLocationDirection::West:
			// Left direction
			DirectionVector = FVector(0, -1, 0);
			break;

		case EHarvestLocationDirection::NorthWest:
			// Diagonal between forward and left
			DirectionVector = FVector(1, -1, 0).GetSafeNormal();
			break;

		default:
			// Handle any unexpected enum values
			RTSFunctionLibrary::ReportError("Encountered an unknown harvester position direction!");
			continue;
		}

		// Store the calculated direction vector in the map
		M_DirectionVectors.Add(Direction, DirectionVector);
	}
}


TArray<EHarvestLocationDirection> UResourceComponent::GetVacantDirections() const
{
	TArray<EHarvestLocationDirection> VacantDirections;
	for (auto EachDirection : M_IsHarvestLocationDirectionTaken)
	{
		if (not EachDirection.Value)
		{
			VacantDirections.Add(EachDirection.Key);
		}
	}
	return VacantDirections;
}

TArray<FHarvestLocation> UResourceComponent::GenerateHarvestLocations(const float HarvesterRadius,
                                                                      const TArray<EHarvestLocationDirection>
                                                                      & PositionDirections,
                                                                      const FVector& HarvesterLocation)
{
	TArray<FHarvestLocation> Locations;
	FHarvestLocation HarvesterPosition;
	if (!IsValid(GetOwner()))
	{
		// In case things go wrong; do not nav to a zero vector.
		HarvesterPosition.Direction = EHarvestLocationDirection::INVALID;
		HarvesterPosition.Location = HarvesterLocation;
		Locations.Add(HarvesterPosition);
		return Locations;
	}
	const FVector RootResourceLocation = M_CachedOwnerLocation;

	const float TotalRadius = HarvesterRadius + M_ResourceInnerRadius;

	// Iterate through each specified direction to generate harvest positions
	for (const EHarvestLocationDirection Direction : PositionDirections)
	{
		if (M_DirectionVectors.Contains(Direction))
		{
			FVector DirectionVector = M_DirectionVectors[Direction];
			const FVector HarvestPosition = RootResourceLocation + (DirectionVector * TotalRadius);
			HarvesterPosition.Direction = Direction;
			HarvesterPosition.Location = HarvestPosition;
			Locations.Add(HarvesterPosition);
		}
		else
		{
			FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NULL";
			FString DirectionName = Global_GetHarvestLocationDirectionAsString(Direction);
			RTSFunctionLibrary::ReportError("Direction vector not found for direction: " + DirectionName +
				"\n This should not happen as there have to be an equal amount of direction vectors"
				"saved as there are valid harveter direction postions."
				"at function GenerateHarvestPositions"
				"\n Owner of resource: " + OwnerName);
		}
	}
	if (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
	{
		DebugGeneratedLocations(Locations);
	}
	return Locations;
}

void UResourceComponent::DebugGeneratedLocations(const TArray<FHarvestLocation>& GeneratedLocations)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FHarvestLocation& HarvesterPos : GeneratedLocations)
	{
		FVector DebugPosition = HarvesterPos.Location + FVector(0.0f, 0.0f, 100.0f);
		FString DirectionString = Global_GetHarvestLocationDirectionAsString(HarvesterPos.Direction);

		DrawDebugString(
			World,
			DebugPosition,
			DirectionString,
			nullptr,
			FColor::Yellow,
			5.0f,
			true
		);
	}
}


FVector UResourceComponent::GetResourceLocationNotThreadSafe()
{
	return M_CachedOwnerLocation;
}

bool UResourceComponent::IsResourceFullyOccupiedByHarvesters() const
{
	bool bIsFullyOccupied = true;
	for(const auto EachHarvestDirection : M_IsHarvestLocationDirectionTaken)
	{
		if(not EachHarvestDirection.Value)
		{
			bIsFullyOccupied = false;
			break;
		}
	}
	return bIsFullyOccupied;
}


int32 UResourceComponent::GetCurrentHarvesters() const
{
	int32 Count = 0;
	for (const auto EachHarvestDirection : M_IsHarvestLocationDirectionTaken)
	{
		if (EachHarvestDirection.Value)
		{
			Count++;
		}
	}
	return Count;
}

void UResourceComponent::InitResourceComponent(
	const int32 MaxHarvesters,
	const float ResourceInnerRadius)
{
	if (not IsValid(GetOwner()))
	{
		RTSFunctionLibrary::ReportError("Owner is not valid in InitResourceComponent"
			"\n at function InitResourceComponent in ResourceComponent.cpp."
			"\n failed to create threadsafe position of resource.");
	}
	if (MaxHarvesters <= 0)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "MaxHarvesters",
		                                                      "InitResourceComponent", GetOwner());
		M_MaxHarvesters = 1;
	}
	else
	{
		M_MaxHarvesters = MaxHarvesters;
	}
	if (M_MaxHarvesters > 8)
	{
		RTSFunctionLibrary::ReportError("Warning, resources do not support more than 8 harvesting positions."
			"\n provided: " + FString::FromInt(M_MaxHarvesters) +
			" at function InitResourceComponent in ResourceComponent.cpp."
			"\n for resource: " + GetOwner()->GetName() +
			"\n setting to 8.");
		M_MaxHarvesters = 8;
	}
	InitializeHarvesterPositionsAsVacant(M_MaxHarvesters);

	if (ResourceInnerRadius <= 0)
	{
		RTSFunctionLibrary::ReportError("very small backupradius set; possibly not navigable; adjusting to 25");
		M_ResourceInnerRadius = 25.f;
	}
	else
	{
		M_ResourceInnerRadius = ResourceInnerRadius;
	}
}

// Called when the game starts
void UResourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if(not IsValid(GetOwner()))
	{
		RTSFunctionLibrary::ReportError("Resource Owner Invalid?!"
								  "\n Component: " + GetName());
		return;
	}

	bool bIsInitialised = true;
	if (ResourceType == ERTSResourceType::Resource_None)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "ResourceType",
		                                                      "BeginPlay", GetOwner());
		bIsInitialised = false;
	}
	if (AmountPerHarvest <= 0)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "AmountPerHarvest",
		                                                      "BeginPlay", GetOwner());
		AmountPerHarvest = 1;
		bIsInitialised = false;
	}
	if (TotalAmount <= 0)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "TotalAmount",
		                                                      "BeginPlay", GetOwner());
		M_OriginalTotalAmount = TotalAmount;
		bIsInitialised = false;
	}
	else
	{
		M_OriginalTotalAmount = TotalAmount;
	}
	if (GatherType == EGatherType::Gather_None)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "GatherType",
		                                                      "BeginPlay", GetOwner());
		bIsInitialised = false;
	}
	if (AActor* Owner = GetOwner())
	{
		M_ResourceOwner.SetInterface(Cast<IResourceStorageOwner>(Owner));
		M_ResourceOwner.SetObject(GetOwner());
		if (!IsValid(M_ResourceOwner.GetObject()))
		{
			RTSFunctionLibrary::ReportFailedCastError("Owner", "IResourceStorageOwner",
			                                          "BeginPlay");
			bIsInitialised = false;
		}
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "Owner",
		                                                  "BeginPlay");
		bIsInitialised = false;
	}
	if (bIsInitialised)
	{
		RegisterWithGameResourceManager();
	}
	if (UWorld* World = GetWorld())
	{
		M_NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	}
	if (not IsValid(M_NavigationSystem))
	{
		RTSFunctionLibrary::ReportError("Navigation system not found in ResourceComponent.cpp"
			"\n at function BeginPlay in ResourceComponent.cpp.");
	}
	// Make sure this is done AFTER the navigation system is set.
	M_CachedOwnerLocation = GetOwnerLocation(FVector::ZeroVector); 
	
}

void UResourceComponent::RegisterWithGameResourceManager()
{
	if (const UWorld* World = GetWorld())
	{
		if (AGameStateBase* GameStateBase = World->GetGameState())
		{
			if (const ACPPGameState* CPPGameState = Cast<ACPPGameState>(GameStateBase))
			{
				if (UGameResourceManager* GameResourceManager = CPPGameState->GetGameResourceManager())
				{
					GameResourceManager->RegisterMapResource(this, true);
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

FVector UResourceComponent::GetOwnerLocation(const FVector& FallBackLocation) const
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{

		return Owner->GetActorLocation();
	}
	RTSFunctionLibrary::ReportNullErrorInitialisation(this, "Owner",
	                                                  "GetOwnerLocation");
	return FallBackLocation;
}
