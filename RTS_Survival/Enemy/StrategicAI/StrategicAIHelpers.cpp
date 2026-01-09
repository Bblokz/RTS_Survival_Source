#include "StrategicAIHelpers.h"

#include "RTS_Survival/Game/GameState/GameUnitManager/AsyncUnitDetailedState/AsyncUnitDetailedState.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

bool FStrategicAIHelpers::GetIsFlankableHeavyTank(const FAsyncDetailedUnitState& UnitState)
{
	if(UnitState.UnitType != EAllUnitType::UNType_Tank)
	{
		return false;
	}
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
	return Global_GetIsHeavyTank(TankSubtype);
}
