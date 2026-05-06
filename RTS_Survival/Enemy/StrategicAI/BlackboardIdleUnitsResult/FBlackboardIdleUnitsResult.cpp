#include "FBlackboardIdleUnitsResult.h"

#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

void FBlackboardIdleUnitsResult::SetupResultForPickedEntries(const TArray<FBlackboardIdleUnitEntry>& Entries)
{
	for (const FBlackboardIdleUnitEntry& Entry : Entries)
	{
		if (Entry.UnitType == EAllUnitType::UNType_Tank)
		{
			ATankMaster* TankMaster = Cast<ATankMaster>(Entry.IdleUnit.Get());
			if (not IsValid(TankMaster))
			{
				continue;
			}

			TankMasters.Add(TankMaster);
		}

		if (Entry.UnitType == EAllUnitType::UNType_Squad)
		{
			ASquadController* SquadController = Cast<ASquadController>(Entry.IdleUnit.Get());
			if (not IsValid(SquadController))
			{
				continue;
			}

			SquadControllers.Add(SquadController);
		}
	}
}
