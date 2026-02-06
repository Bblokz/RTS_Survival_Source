#pragma once

#include "CoreMinimal.h"

#include "NomadicLayoutBuildingType.generated.h"

UENUM(BlueprintType)
enum class ENomadicLayoutBuildingType : uint8
{
    Building_None,
    Building_Barracks,
    Building_Forge,
    Building_MechanicalDepot,
    Building_T2Factory,
    Building_Airbase,
    Building_Experimental
};

static FString Global_GetNomadicLayoutBuildingTypeString(const ENomadicLayoutBuildingType BuildingType)
{
    switch (BuildingType)
    {
    case ENomadicLayoutBuildingType::Building_Barracks:
        return "Building_Barracks";
    case ENomadicLayoutBuildingType::Building_Forge:
        return "Building_Forge";
    case ENomadicLayoutBuildingType::Building_MechanicalDepot:
        return "Building_MechanicalDepot";
    case ENomadicLayoutBuildingType::Building_T2Factory:
        return "Building_T2Factory";
        case ENomadicLayoutBuildingType::Building_Airbase:
        return "Building_Airbase";
    case ENomadicLayoutBuildingType::Building_Experimental:
        return "Building_Experimental";
    default:
        return "Building_None";
    }
}
