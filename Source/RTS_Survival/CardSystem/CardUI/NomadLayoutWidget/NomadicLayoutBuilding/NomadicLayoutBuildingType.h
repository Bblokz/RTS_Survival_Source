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
    default:
        return "Building_None";
    }
}
