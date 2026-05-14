#include "StrategicAIActionRequirements.h"

#include "GameFramework/Actor.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardQueries/BlackboardQueryHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace
{
	void AddUnitPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const EAITrainingFocus FocusPressure,
		const EAITrainingFocusSpecialty SpecialtyPressure,
		const float PressureAmount,
		const FString& SourceDebugName,
		const bool bSpreadFocusPressureAcrossAllBuckets = false)
	{
		FEnemyStrategicTrainingPressureContribution PressureContribution;
		PressureContribution.FocusPressure = FocusPressure;
		PressureContribution.SpecialtyPressure = SpecialtyPressure;
		PressureContribution.PressureAmount = PressureAmount;
		PressureContribution.SourceDebugName = SourceDebugName;
		PressureContribution.bSpreadFocusPressureAcrossAllBuckets = bSpreadFocusPressureAcrossAllBuckets;
		OutPressureContributions.Add(PressureContribution);
	}

	float GetRequirementPressureAmount(const float PressureAmount, const int32 RequiredAmount)
	{
		return PressureAmount * FMath::Max(1, RequiredAmount);
	}

	EAITrainingFocusSpecialty GetRequirementSpecialtyPressure(
		const EAITrainingFocusSpecialty BaseSpecialtyPressure)
	{
		if (BaseSpecialtyPressure == EAITrainingFocusSpecialty::NoTrainingPressure)
		{
			return EAITrainingFocusSpecialty::GeneralPurpose;
		}

		return BaseSpecialtyPressure;
	}
}

FString UStrategicAIActionRequirement::GetDebugString() const
{
	return "DO NOT USE default requirement!!";
}

bool UStrategicAIGameTimePassedRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	return GameTimeSeconds >= RequiredGameTimeSeconds;
}

FString UStrategicAIGameTimePassedRequirement::GetDebugString() const
{
		return FString::Printf(TEXT("Time Req: %f sec"), RequiredGameTimeSeconds);
}

bool UStrategicAIActorIsValidRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	return GetIsValidRequiredActor();
}

bool UStrategicAIActorIsValidRequirement::GetIsValidRequiredActor() const
{
	if (IsValid(RequiredActor))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Strategic AI actor requirement failed because RequiredActor is invalid."));

	return false;
}

FString UStrategicAIActorIsValidRequirement::GetDebugString() const
{
	return FString::Printf(TEXT("Actor Valid Req: %s"), *GetNameSafe(RequiredActor));
}

bool UStrategicAIHasPlayerHQLocationRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasValidPlayerHQLocation(Blackboard);
}

FString UStrategicAIHasPlayerHQLocationRequirement::GetDebugString() const
{
	return TEXT("Has Player HQ Location Req");
}

bool UStrategicAIHasPlayerResourceBuildingLocationsRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasValidPlayerResourceBuildingLocations(Blackboard);
}

FString UStrategicAIHasPlayerResourceBuildingLocationsRequirement::GetDebugString() const
{
	return TEXT("Has Player Resource Building Locations Req");
}

bool UStrategicAIHasAtLeastAnyIdleUnits::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastAnyXIdleUnits(Blackboard, AmountIdleNeeded);
}

void UStrategicAIHasAtLeastAnyIdleUnits::ContributeToIdleUnitSelectionPolicy(
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	SelectionPolicy.AddMinimumIdleUnitsContribution(AmountIdleNeeded);
}

bool UStrategicAIHasAtLeastAnyIdleUnits::GetShouldBlockTrainingPressureWhenUnmet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return false;
}

bool UStrategicAIHasAtLeastAnyIdleUnits::GetIsUnitTrainingPressureRequirement() const
{
	return true;
}

void UStrategicAIHasAtLeastAnyIdleUnits::AddMissingUnitTrainingPressureContribution(
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	const float PressureAmount,
	const FString& SourceDebugName,
	const EAITrainingFocusSpecialty BaseSpecialtyPressure) const
{
	AddUnitPressureContribution(
		OutPressureContributions,
		EAITrainingFocus::NoFocus,
		GetRequirementSpecialtyPressure(BaseSpecialtyPressure),
		GetRequirementPressureAmount(PressureAmount, AmountIdleNeeded),
		SourceDebugName,
		true);
}

FString UStrategicAIHasAtLeastAnyIdleUnits::GetDebugString() const
{
	return Super::GetDebugString();
}

bool UStrategicAIHasAtLeastIdleSquads::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
                                                           const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXSquads(Blackboard, AmountIdleSpecificSquadsNeeded, RequiredSquadSubtype);	

}

void UStrategicAIHasAtLeastIdleSquads::ContributeToIdleUnitSelectionPolicy(
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	SelectionPolicy.AddRequiredRule(FIdleUnitSelectionRule::CreateSquadSubtypeRule(AmountIdleSpecificSquadsNeeded, RequiredSquadSubtype));
}

bool UStrategicAIHasAtLeastIdleSquads::GetShouldBlockTrainingPressureWhenUnmet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return false;
}

bool UStrategicAIHasAtLeastIdleSquads::GetIsUnitTrainingPressureRequirement() const
{
	return true;
}

void UStrategicAIHasAtLeastIdleSquads::AddMissingUnitTrainingPressureContribution(
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	const float PressureAmount,
	const FString& SourceDebugName,
	const EAITrainingFocusSpecialty BaseSpecialtyPressure) const
{
	AddUnitPressureContribution(
		OutPressureContributions,
		EAITrainingFocus::Infantry,
		GetRequirementSpecialtyPressure(BaseSpecialtyPressure),
		GetRequirementPressureAmount(PressureAmount, AmountIdleSpecificSquadsNeeded),
		SourceDebugName);
}

FString UStrategicAIHasAtLeastIdleSquads::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Squads Req: %d %s"), AmountIdleSpecificSquadsNeeded, *Global_GetSquadDisplayName(RequiredSquadSubtype));
}

bool UStrategicAIHasAtLeastIdleTanks::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
                                                          const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXTanks(Blackboard, AmountIdleOfSpecificTanksNeeded, RequiredTankSubtype);
}

void UStrategicAIHasAtLeastIdleTanks::ContributeToIdleUnitSelectionPolicy(
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	SelectionPolicy.AddRequiredRule(FIdleUnitSelectionRule::CreateTankSubtypeRule(AmountIdleOfSpecificTanksNeeded, RequiredTankSubtype));
}

bool UStrategicAIHasAtLeastIdleTanks::GetShouldBlockTrainingPressureWhenUnmet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return false;
}

bool UStrategicAIHasAtLeastIdleTanks::GetIsUnitTrainingPressureRequirement() const
{
	return true;
}

void UStrategicAIHasAtLeastIdleTanks::AddMissingUnitTrainingPressureContribution(
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	const float PressureAmount,
	const FString& SourceDebugName,
	const EAITrainingFocusSpecialty BaseSpecialtyPressure) const
{
	EAITrainingFocus FocusPressure = EAITrainingFocus::NoFocus;
	if (Global_GetIsLightTank(RequiredTankSubtype))
	{
		FocusPressure = EAITrainingFocus::LightTanks;
	}
	else if (Global_GetIsMediumTank(RequiredTankSubtype))
	{
		FocusPressure = EAITrainingFocus::MediumTanks;
	}
	else if (Global_GetIsHeavyTank(RequiredTankSubtype))
	{
		FocusPressure = EAITrainingFocus::HeavyTanks;
	}

	AddUnitPressureContribution(
		OutPressureContributions,
		FocusPressure,
		GetRequirementSpecialtyPressure(BaseSpecialtyPressure),
		GetRequirementPressureAmount(PressureAmount, AmountIdleOfSpecificTanksNeeded),
		SourceDebugName);
}

FString UStrategicAIHasAtLeastIdleTanks::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Tanks Req: %d %s"), AmountIdleOfSpecificTanksNeeded, *Global_GetTankDisplayName(RequiredTankSubtype));
}

bool UStrategicAIHasAtLeastAnyIdleTanks::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastAnyXTanks(Blackboard, AmountIdleTanksNeeded);
}

void UStrategicAIHasAtLeastAnyIdleTanks::ContributeToIdleUnitSelectionPolicy(
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	SelectionPolicy.AddRequiredRule(FIdleUnitSelectionRule::CreateAnyTankRule(AmountIdleTanksNeeded));
}

bool UStrategicAIHasAtLeastAnyIdleTanks::GetShouldBlockTrainingPressureWhenUnmet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return false;
}

bool UStrategicAIHasAtLeastAnyIdleTanks::GetIsUnitTrainingPressureRequirement() const
{
	return true;
}

void UStrategicAIHasAtLeastAnyIdleTanks::AddMissingUnitTrainingPressureContribution(
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	const float PressureAmount,
	const FString& SourceDebugName,
	const EAITrainingFocusSpecialty BaseSpecialtyPressure) const
{
	AddUnitPressureContribution(
		OutPressureContributions,
		EAITrainingFocus::MediumTanks,
		GetRequirementSpecialtyPressure(BaseSpecialtyPressure),
		GetRequirementPressureAmount(PressureAmount, AmountIdleTanksNeeded),
		SourceDebugName);
}

FString UStrategicAIHasAtLeastAnyIdleTanks::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Tanks Req: %d Any Tank Type"), AmountIdleTanksNeeded);
}

bool UStrategicAIHasAtLeastIdleAircraft::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
                                                             const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXAircraft(Blackboard, AmountIdleAircraftNeeded, RequiredAircraftSubtype);
}

FString UStrategicAIHasAtLeastIdleAircraft::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Aircraft Req: %d %s"), AmountIdleAircraftNeeded, *Global_GetAircraftDisplayName(RequiredAircraftSubtype));
}

bool UStrategicAIHasEnoughLightTanks::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXLightTanks(Blackboard, AmountIdleLightTanksNeeded);
}

void UStrategicAIHasEnoughLightTanks::ContributeToIdleUnitSelectionPolicy(
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	SelectionPolicy.AddRequiredRule(FIdleUnitSelectionRule::CreateTankCategoryRule(
		AmountIdleLightTanksNeeded,
		EIdleUnitSelectionTankCategory::LightTank));
}

bool UStrategicAIHasEnoughLightTanks::GetShouldBlockTrainingPressureWhenUnmet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return false;
}

bool UStrategicAIHasEnoughLightTanks::GetIsUnitTrainingPressureRequirement() const
{
	return true;
}

void UStrategicAIHasEnoughLightTanks::AddMissingUnitTrainingPressureContribution(
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	const float PressureAmount,
	const FString& SourceDebugName,
	const EAITrainingFocusSpecialty BaseSpecialtyPressure) const
{
	AddUnitPressureContribution(
		OutPressureContributions,
		EAITrainingFocus::LightTanks,
		GetRequirementSpecialtyPressure(BaseSpecialtyPressure),
		GetRequirementPressureAmount(PressureAmount, AmountIdleLightTanksNeeded),
		SourceDebugName);
}

FString UStrategicAIHasEnoughLightTanks::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Light Tanks Req: %d"), AmountIdleLightTanksNeeded);
}

bool UStrategicAIHasEnoughHeavyTanks::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXHeavyTanks(Blackboard, AmountIdleHeavyTanksNeeded);
}

void UStrategicAIHasEnoughHeavyTanks::ContributeToIdleUnitSelectionPolicy(
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	SelectionPolicy.AddRequiredRule(FIdleUnitSelectionRule::CreateTankCategoryRule(
		AmountIdleHeavyTanksNeeded,
		EIdleUnitSelectionTankCategory::HeavyTank));
}

bool UStrategicAIHasEnoughHeavyTanks::GetShouldBlockTrainingPressureWhenUnmet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return false;
}

bool UStrategicAIHasEnoughHeavyTanks::GetIsUnitTrainingPressureRequirement() const
{
	return true;
}

void UStrategicAIHasEnoughHeavyTanks::AddMissingUnitTrainingPressureContribution(
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	const float PressureAmount,
	const FString& SourceDebugName,
	const EAITrainingFocusSpecialty BaseSpecialtyPressure) const
{
	AddUnitPressureContribution(
		OutPressureContributions,
		EAITrainingFocus::HeavyTanks,
		GetRequirementSpecialtyPressure(BaseSpecialtyPressure),
		GetRequirementPressureAmount(PressureAmount, AmountIdleHeavyTanksNeeded),
		SourceDebugName);
}

FString UStrategicAIHasEnoughHeavyTanks::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Heavy Tanks Req: %d"), AmountIdleHeavyTanksNeeded);	
}

bool UStrategicAIDoesPlayerHaveHeavyTanks::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXPlayerHeavyTanks(Blackboard, AmountPlayerHeaviesNeeded);
}

FString UStrategicAIDoesPlayerHaveHeavyTanks::GetDebugString() const
{
	return FString::Printf(TEXT("Player Has Heavy Tanks Req: %d"), AmountPlayerHeaviesNeeded);
}

bool UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	return BlackboardQueries::HasValidPlayerHeavyTankFLankLocations(Blackboard);
}

FString UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions::GetDebugString() const
{
	return Super::GetDebugString();
}
