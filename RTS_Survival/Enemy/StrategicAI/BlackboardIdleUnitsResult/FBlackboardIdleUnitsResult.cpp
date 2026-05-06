#include "FBlackboardIdleUnitsResult.h"

#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

void FBlackboardIdleUnitsResult::SetupResultForPickedEntries(const TArray<FBlackboardIdleUnitEntry>& Entries)
{
	ATankMaster* TankMasterPtr = nullptr;
	ASquadController* SquadControllerPtr = nullptr;
	for(const auto& Entry : Entries)
	{
		if(Entry.UnitType == EAllUnitType::UNType_Tank)
		{
			TankMasterPtr = Cast<ATankMaster>(Entry.IdleUnit.Get());
			TankMasters.Add(TankMasterPtr);
		}
		if(Entry.UnitType == EAllUnitType::UNType_Squad)
		{
			SquadControllerPtr = Cast<ASquadController>(Entry.IdleUnit.Get());
			SquadControllers.Add(SquadControllerPtr);
		}
	}
	
}
