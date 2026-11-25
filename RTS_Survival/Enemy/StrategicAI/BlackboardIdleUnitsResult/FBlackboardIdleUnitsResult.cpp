#include "FBlackboardIdleUnitsResult.h"

#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIHelpers.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

void FBlackboardIdleUnitsResult::SetupResultForPickedEntries(const TArray<FBlackboardIdleUnitEntry>& Entries)
{
	for (const FBlackboardIdleUnitEntry& Entry : Entries)
	{
		// Every pick (attack, defend, formation, direct control) ends here: a harvester that reached the blackboard is
		// dropped from it for good and keeps harvesting instead of being sent to fight.
		if (FStrategicAIHelpers::GetIsHarvesterUnit(Entry.UnitType, Entry.UnitSubtypeRaw)
			|| FStrategicAIHelpers::GetIsExcludedFromEnemyUnitPools(Entry.IdleUnit.Get()))
		{
			continue;
		}
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
