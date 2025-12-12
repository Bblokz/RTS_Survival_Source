#pragma once

#include "CoreMinimal.h"
#include "HarvestLocation.generated.h"


UENUM()
enum class EHarvestLocationDirection : uint8
{
	INVALID,
	North,
	NorthEast,
	NorthWest,
	East,
	SouthEast,
	South,
	SouthWest,
	West
	
};

USTRUCT()
struct FHarvestLocation
{
	GENERATED_BODY()
	EHarvestLocationDirection Direction;
	FVector Location;
};

static FString Global_GetHarvestLocationDirectionAsString(const EHarvestLocationDirection Direction)
{
	switch (Direction)
	{
	case EHarvestLocationDirection::North:
		return "North";
	case EHarvestLocationDirection::NorthEast:
		return "NorthEast";
	case EHarvestLocationDirection::NorthWest:
		return "NorthWest";
	case EHarvestLocationDirection::East:
		return "East";
	case EHarvestLocationDirection::SouthEast:
		return "SouthEast";
	case EHarvestLocationDirection::South:
		return "South";
	case EHarvestLocationDirection::SouthWest:
		return "SouthWest";
	case EHarvestLocationDirection::West:
		return "West";
	default:
		return "INVALID";
	}
}


UENUM()
enum class ECannotMoveToResource
{
	INVALID,
	ResourceFullyOccupied,
	CannotProjectLocation
};

static FString Global_GetCannotMoveToResourceAsString(const ECannotMoveToResource CannotMoveToResource)
{
	switch (CannotMoveToResource)
	{
	case ECannotMoveToResource::ResourceFullyOccupied:
		return "ResourceFullyOccupied";
	case ECannotMoveToResource::CannotProjectLocation:
		return "CannotProjectLocation";
	default:
		return "INVALID";
	}
}
