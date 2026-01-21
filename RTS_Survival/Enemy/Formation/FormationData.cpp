#include "FormationData.h"

#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool FFormationUnitData::IsValidFormationUnit() const
{
	return Unit.IsValid();
}

bool FFormationData::CheckIfFormationReachedCurrentWayPoint(const TWeakInterfacePtr<ICommands> UnitThatReached,
                                                            UEnemyFormationController* EnemyFormationController)
{
	EnsureAllFormationUnitsAreValid(EnemyFormationController);
	if (UnitThatReached.IsValid())
	{
		bool bAllUnitsReached = true;
		bool bFoundReachedUnit = false;
		for (auto& EachUnit : FormationUnits)
		{
				if (EachUnit.Unit == UnitThatReached)
				{
					bFoundReachedUnit = true;
					EachUnit.bHasReachedNextDestination = true;
					EachUnit.StuckCounts = 0;
					EachUnit.M_LastKnownLocation = FVector::ZeroVector;
					EachUnit.bM_HasLastKnownLocation = false;
				}
			if (not EachUnit.bHasReachedNextDestination)
			{
				bAllUnitsReached = false;
			}
		}
		if (not bFoundReachedUnit)
		{
			OnReachedUnitNotInFormation(UnitThatReached);
			return false;
		}
		return bAllUnitsReached;
	}
	return false;
}

FVector FFormationData::GetFormationUnitLocation()
{
	for (auto EachUnit : FormationUnits)
	{
		if (EachUnit.IsValidFormationUnit())
		{
			return EachUnit.Unit->GetOwnerLocation();
		}
	}
	RTSFunctionLibrary::ReportError("Could not get formation unit location as there are no valid formation units.");
	return FVector::ZeroVector;
}

void FFormationData::EnsureAllFormationUnitsAreValid(UEnemyFormationController* EnemyFormationController)
{
	TArray<FFormationUnitData> ValidFormationUnits;
	for (auto EachUnit : FormationUnits)
	{
		if (EachUnit.IsValidFormationUnit())
		{
			ValidFormationUnits.Add(EachUnit);
			continue;
		}
		if (EnemyFormationController)
		{
			EnemyFormationController->RefundUnitWaveSupply(1);
		}
		if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Invalid unit in formation while checking if formation reached! Removing..."
				"\n At FFormationData::EnsureAllFormationUnitsAreValid()"
				"\n Formation waypoint index: " + FString::FromInt(CurrentWaypointIndex) +
				"\n Formation ID: " + FString::FromInt(FormationID));
		}
	}
	FormationUnits = MoveTemp(ValidFormationUnits);
}

void FFormationData::OnReachedUnitNotInFormation(const TWeakInterfacePtr<ICommands> UnitThatReached) const
{
	if (not UnitThatReached.IsValid())
	{
		return;
	}
	const FString UnitName = UnitThatReached->GetOwnerName();
	RTSFunctionLibrary::ReportError("The unit reached way point but is not in formation."
		"\n unit name : " + UnitName);
}
