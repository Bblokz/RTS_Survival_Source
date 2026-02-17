#pragma once

#include "ERTSRadiusType.generated.h"

/**
 * @brief Types of RTS helper radii we can render.
 * Keep this lightweight; materials are mapped per type in URadiusPoolSettings.
 * 
* also adjust the new cases in  bool URTSRadiusPoolSubsystem::GetIsFullCircleRadiusType(const ERTSRadiusType Type) const
* to help determine which types are full circles.
 */
UENUM(BlueprintType)
enum class ERTSRadiusType : uint8
{
	None                        UMETA(DisplayName="None"),
	RangeBorderOnly                       UMETA(DisplayName="Range"),
	PostConstructionBuildingRangeBorderOnly UMETA(DisplayName="PostConstructionBuildingRange"),
	RadarBorderOnly                       UMETA(DisplayName="Radar"),
	FUllCircle_Weaponrange 			  UMETA(DisplayName="FullCircle_Weaponrange"),
	FullCircle_RepairRange			  UMETA(DisplayName="FullCircle_RepairRange"),
	FullCircle_CommandAura			  UMETA(DisplayName="FullCircle_CommandAura"),
	FullCircle_ReinforcementAura		  UMETA(DisplayName="FullCircle_ReinforcementAura"),
	FullCircle_ImprovedRangeArea	  UMETA(DisplayName="FullCircle_ImprovedRangeArea"),
	FullCircle_RadiationAura	      UMETA(DisplayName="FullCircle_RadiationAura"),
	Fullcircle_HealAura	          UMETA(DisplayName="FullCircle_HealAura"),
	FullCircle_PulseRepair          UMETA(DisplayName="FullCircle_PulseRepair")
};
