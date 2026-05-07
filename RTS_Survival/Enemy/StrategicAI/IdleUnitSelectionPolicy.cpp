#include "IdleUnitSelectionPolicy.h"

FIdleUnitSelectionRule FIdleUnitSelectionRule::CreateAnyTankRule(const int32 RequiredAmount)
{
	FIdleUnitSelectionRule Rule;
	Rule.RuleType = EIdleUnitSelectionRuleType::AnyTank;
	Rule.RequiredAmount = RequiredAmount;
	return Rule;
}

FIdleUnitSelectionRule FIdleUnitSelectionRule::CreateTankSubtypeRule(
	const int32 RequiredAmount,
	const ETankSubtype TankSubtype)
{
	FIdleUnitSelectionRule Rule;
	Rule.RuleType = EIdleUnitSelectionRuleType::TankSubtype;
	Rule.RequiredAmount = RequiredAmount;
	Rule.RequiredTankSubtype = TankSubtype;
	return Rule;
}

FIdleUnitSelectionRule FIdleUnitSelectionRule::CreateSquadSubtypeRule(
	const int32 RequiredAmount,
	const ESquadSubtype SquadSubtype)
{
	FIdleUnitSelectionRule Rule;
	Rule.RuleType = EIdleUnitSelectionRuleType::SquadSubtype;
	Rule.RequiredAmount = RequiredAmount;
	Rule.RequiredSquadSubtype = SquadSubtype;
	return Rule;
}

FIdleUnitSelectionRule FIdleUnitSelectionRule::CreateTankCategoryRule(
	const int32 RequiredAmount,
	const EIdleUnitSelectionTankCategory TankCategory)
{
	FIdleUnitSelectionRule Rule;
	Rule.RuleType = EIdleUnitSelectionRuleType::TankCategory;
	Rule.RequiredAmount = RequiredAmount;
	Rule.RequiredTankCategory = TankCategory;
	return Rule;
}

void FIdleUnitSelectionPolicy::SetupFallbackMinMax(
	const int32 FallbackMinUnitsToPick,
	const int32 FallbackMaxUnitsToPick)
{
	MinUnitsToPick = FallbackMinUnitsToPick;
	MaxUnitsToPick = FallbackMaxUnitsToPick;
	NormalizeMinMax();
}

void FIdleUnitSelectionPolicy::AddMinimumIdleUnitsContribution(const int32 RequiredAmount)
{
	if (RequiredAmount <= 0)
	{
		return;
	}

	bHasRequirementDrivenRules = true;
	MinUnitsToPick = FMath::Max(MinUnitsToPick, RequiredAmount);
	NormalizeMinMax();
}

void FIdleUnitSelectionPolicy::AddRequiredRule(const FIdleUnitSelectionRule& Rule)
{
	if (Rule.RequiredAmount <= 0)
	{
		return;
	}

	bHasRequirementDrivenRules = true;
	RequiredRules.Add(Rule);
	MinUnitsToPick = FMath::Max(MinUnitsToPick, GetRequiredRuleUnitCount());
	NormalizeMinMax();
}

void FIdleUnitSelectionPolicy::NormalizeMinMax()
{
	MinUnitsToPick = FMath::Max(0, MinUnitsToPick);
	MaxUnitsToPick = FMath::Max(0, MaxUnitsToPick);
	MaxUnitsToPick = FMath::Max(MinUnitsToPick, MaxUnitsToPick);
}

int32 FIdleUnitSelectionPolicy::GetRequiredRuleUnitCount() const
{
	int32 RequiredUnitCount = 0;
	for (const FIdleUnitSelectionRule& Rule : RequiredRules)
	{
		RequiredUnitCount += FMath::Max(0, Rule.RequiredAmount);
	}

	return RequiredUnitCount;
}
