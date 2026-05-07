#include "EnemyDirectControlComponent.h"

#include "DrawDebugHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/EnemyFieldConstructionComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlConstants.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace
{
	/**
	 * @brief Chooses a random pick count in a safe range for min/max selection rules.
	 * @details This helper protects callers from invalid configuration and impossible requests.
	 * It first rejects invalid ranges (negative min, non-positive max, min > max) and also
	 * rejects cases where available units are below the required minimum. If valid, it clamps
	 * the upper bound to what is actually available, then returns a random integer in that
	 * inclusive range using Unreal's RNG.
	 * @param MinUnitsToPick Minimum required units from policy/config.
	 * @param MaxUnitsToPick Maximum allowed units from policy/config.
	 * @param AvailableUnitCount Number of currently available candidates.
	 * @return A valid random count, or 0 when request/config is not satisfiable.
	 * @note Why: Centralizes guard logic so selection call sites do not duplicate validation.
	 */
	int32 GetRandomMinMaxPickCount(
		const int32 MinUnitsToPick,
		const int32 MaxUnitsToPick,
		const int32 AvailableUnitCount)
	{
		if (MinUnitsToPick < 0 || MaxUnitsToPick <= 0)
		{
			return 0;
		}

		if (MinUnitsToPick > MaxUnitsToPick || AvailableUnitCount < MinUnitsToPick)
		{
			return 0;
		}

		const int32 MaxAllowedPickCount = FMath::Min(MaxUnitsToPick, AvailableUnitCount);
		return FMath::RandRange(MinUnitsToPick, MaxAllowedPickCount);
	}

	/**
	 * @brief Checks whether a unit type is supported for this direct-control idle-unit flow.
	 * @details Only squad and tank are currently accepted into the blackboard idle-entry path.
	 * Unsupported types are filtered out early.
	 * @param UnitType Unit type to check.
	 * @return True when type is supported by this feature.
	 * @note Why: Keeps inclusion policy explicit in one place for future extension.
	 */
	bool GetIsSupportedDirectControlUnitType(const EAllUnitType UnitType)
	{
		return UnitType == EAllUnitType::UNType_Squad || UnitType == EAllUnitType::UNType_Tank;
	}

	/**
	 * @brief Converts an actor into a valid idle-unit blackboard entry when possible.
	 * @details Performs strict guards in sequence: actor valid, RTS component exists,
	 * component type is supported. Only then writes output fields. Returning false means
	 * caller should ignore this actor for idle selection.
	 * @param UnitActor Source actor candidate.
	 * @param OutIdleUnitEntry Output entry to populate on success.
	 * @return True if output entry was successfully initialized.
	 * @note Why: Isolates actor-to-entry translation and all eligibility checks in one function.
	 */
	bool TrySetupIdleUnitEntryFromActor(AActor* UnitActor, FBlackboardIdleUnitEntry& OutIdleUnitEntry)
	{
		if (not IsValid(UnitActor))
		{
			return false;
		}

		const URTSComponent* RTSComponent = UnitActor->FindComponentByClass<URTSComponent>();
		if (not IsValid(RTSComponent))
		{
			return false;
		}

		if (not GetIsSupportedDirectControlUnitType(RTSComponent->GetUnitType()))
		{
			return false;
		}

		OutIdleUnitEntry.SetIdleUnitActor(UnitActor);
		OutIdleUnitEntry.UnitType = RTSComponent->GetUnitType();
		OutIdleUnitEntry.UnitSubtypeRaw = static_cast<int32>(RTSComponent->GetUnitSubType());
		return true;
	}

	/**
	 * @brief Predicate: checks whether an entry is a squad.
	 * @details Simple reusable predicate used in filtering/picking code.
	 * @param Entry Idle entry to test.
	 * @return True when entry type is squad.
	 * @note Why: Improves readability when passed as a predicate function.
	 */
	bool GetIsSquadEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		return Entry.UnitType == EAllUnitType::UNType_Squad;
	}

	/**
	 * @brief Predicate: checks whether an entry is a tank.
	 * @details Simple reusable predicate used in filtering/picking code.
	 * @param Entry Idle entry to test.
	 * @return True when entry type is tank.
	 * @note Why: Avoids repeating type comparisons at call sites.
	 */
	bool GetIsTankEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		return Entry.UnitType == EAllUnitType::UNType_Tank;
	}

	/**
	 * @brief Predicate: checks whether an entry is specifically a Hazmat Engineer squad.
	 * @details Verifies squad type first, then interprets raw subtype and compares against
	 * Hazmat Engineer enum value.
	 * @param Entry Idle entry to test.
	 * @return True when entry is a Hazmat Engineer squad.
	 * @note Why: Encapsulates subtype cast/compare details in one place.
	 */
	bool GetIsHazmatEngineerSquadEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		if (Entry.UnitType != EAllUnitType::UNType_Squad)
		{
			return false;
		}

		const ESquadSubtype SquadSubtype = static_cast<ESquadSubtype>(Entry.UnitSubtypeRaw);
		return SquadSubtype == ESquadSubtype::Squad_Rus_HazmatEngineers;
	}

	/**
	 * @brief Predicate inverse of Hazmat Engineer squad check.
	 * @details Returns true for every entry that is not Hazmat Engineer squad.
	 * @param Entry Idle entry to test.
	 * @return True when entry is not Hazmat Engineer squad.
	 * @note Why: Improves expressiveness in call sites requiring exclusion filters.
	 */
	bool GetIsNotHazmatEngineerSquadEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		return not GetIsHazmatEngineerSquadEntry(Entry);
	}

	/**
	 * @brief Validity predicate for idle entries.
	 * @details Thin wrapper over entry's internal validity check.
	 * @param Entry Idle entry to test.
	 * @return True when entry has a valid backing actor/reference.
	 * @note Why: Useful as a named predicate when passing into generic picker/filter functions.
	 */
	bool GetIsValidIdleUnitEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		return Entry.IsValid();
	}

	/**
	 * @brief Matches a tank subtype against a high-level tank category.
	 * @details Maps category enum to existing global helper classifiers.
	 * @param TankSubtype Concrete tank subtype.
	 * @param TankCategory Category rule to satisfy.
	 * @return True when subtype belongs to requested category.
	 * @note Why: Separates category semantics from rule-evaluation core logic.
	 */
	bool GetIsTankCategoryMatch(
		const ETankSubtype TankSubtype,
		const EIdleUnitSelectionTankCategory TankCategory)
	{
		switch (TankCategory)
		{
		case EIdleUnitSelectionTankCategory::ArmoredCar:
			return Global_GetIsArmoredCar(TankSubtype);
		case EIdleUnitSelectionTankCategory::LightTank:
			return Global_GetIsLightTank(TankSubtype);
		case EIdleUnitSelectionTankCategory::MediumTank:
			return Global_GetIsMediumTank(TankSubtype);
		case EIdleUnitSelectionTankCategory::HeavyTank:
			return Global_GetIsHeavyTank(TankSubtype);
		}

		return false;
	}

	/**
	 * @brief Evaluates whether one idle entry satisfies one selection rule.
	 * @details This is the central rule matcher. It rejects invalid entries first, then dispatches
	 * by rule type and checks exact constraints (unit class, subtype equality, or tank category).
	 * @param Entry Candidate entry.
	 * @param Rule Required selection rule from policy.
	 * @return True when candidate satisfies the rule.
	 * @note Why: Single source of truth for rule semantics keeps behavior consistent everywhere.
	 */
	bool GetDoesEntryMatchSelectionRule(
		const FBlackboardIdleUnitEntry& Entry,
		const FIdleUnitSelectionRule& Rule)
	{
		if (not Entry.IsValid())
		{
			return false;
		}

		switch (Rule.RuleType)
		{
		case EIdleUnitSelectionRuleType::AnyIdleUnit:
			return true;
		case EIdleUnitSelectionRuleType::AnyTank:
			return Entry.UnitType == EAllUnitType::UNType_Tank;
		case EIdleUnitSelectionRuleType::TankSubtype:
			return Entry.UnitType == EAllUnitType::UNType_Tank
				&& Entry.UnitSubtypeRaw == static_cast<int32>(Rule.RequiredTankSubtype);
		case EIdleUnitSelectionRuleType::SquadSubtype:
			return Entry.UnitType == EAllUnitType::UNType_Squad
				&& Entry.UnitSubtypeRaw == static_cast<int32>(Rule.RequiredSquadSubtype);
		case EIdleUnitSelectionRuleType::TankCategory:
			return Entry.UnitType == EAllUnitType::UNType_Tank
				&& GetIsTankCategoryMatch(static_cast<ETankSubtype>(Entry.UnitSubtypeRaw), Rule.RequiredTankCategory);
		}

		return false;
	}

	/**
	 * @brief Counts only valid idle entries from an array.
	 * @details Iterates entries and increments count when entry validity passes.
	 * @param IdleEntries Candidate entries.
	 * @return Number of valid entries.
	 * @note Why: Lets higher-level logic reason about usable capacity, not raw array size.
	 */
	int32 CountValidIdleUnitEntries(const TArray<FBlackboardIdleUnitEntry>& IdleEntries)
	{
		int32 ValidEntryCount = 0;
		for (const FBlackboardIdleUnitEntry& IdleEntry : IdleEntries)
		{
			if (not IdleEntry.IsValid())
			{
				continue;
			}

			++ValidEntryCount;
		}

		return ValidEntryCount;
	}

	/**
	 * @brief Chooses a random final total pick count under current policy bounds.
	 * @details Enforces two dynamic constraints: cannot pick below already-picked count,
	 * and cannot pick above available units. If these constraints make the range invalid,
	 * returns 0.
	 * @param SelectionPolicy Policy containing min/max desired picks.
	 * @param AlreadyPickedUnitCount Count already guaranteed by required-rule phase.
	 * @param AvailableUnitCount Count still available for consideration.
	 * @return Random target total pick count, or 0 when impossible.
	 * @note Why: Required picks and random extra picks must stay coherent with runtime state.
	 */
	int32 GetRandomPickCountForPolicy(
		const FIdleUnitSelectionPolicy& SelectionPolicy,
		const int32 AlreadyPickedUnitCount,
		const int32 AvailableUnitCount)
	{
		const int32 MinUnitsToPick = FMath::Max(SelectionPolicy.MinUnitsToPick, AlreadyPickedUnitCount);
		const int32 MaxUnitsToPick = FMath::Min(SelectionPolicy.MaxUnitsToPick, AvailableUnitCount);
		if (MinUnitsToPick > MaxUnitsToPick)
		{
			return 0;
		}

		return FMath::RandRange(MinUnitsToPick, MaxUnitsToPick);
	}

	/**
	 * @brief Removes picked actors from the blackboard pool to avoid duplicate future picks.
	 * @details Iterates picked entries, validates actor pointers, and removes matching actor
	 * entries from source blackboard list.
	 * @param PickedEntries Entries already selected.
	 * @param BlackboardEntries Mutable source pool to prune.
	 * @note Why: Enforces uniqueness across selection stages and future policy applications.
	 */
	void RemovePickedEntriesFromBlackboard(
		const TArray<FBlackboardIdleUnitEntry>& PickedEntries,
		TArray<FBlackboardIdleUnitEntry>& BlackboardEntries)
	{
		for (const FBlackboardIdleUnitEntry& PickedEntry : PickedEntries)
		{
			AActor* const PickedActor = PickedEntry.Get();
			if (not IsValid(PickedActor))
			{
				continue;
			}

			FBlackboardIdleUnitEntry::RemoveByUnitActor(BlackboardEntries, PickedActor);
		}
	}

	/**
	 * @brief Template helper that picks exactly one random entry matching a caller-provided predicate.
	 * @details The function scans the array, collects indices that satisfy EntryPredicate, randomly
	 * chooses one matching index, copies that entry to OutPickedEntry, and removes it from IdleEntries
	 * via RemoveAtSwap for efficient O(1) removal.
	 * @tparam EntryPredicateType Callable type taking `(const FBlackboardIdleUnitEntry&)` and returning bool.
	 * @param IdleEntries Mutable candidate pool. Chosen entry is removed.
	 * @param EntryPredicate Matching logic supplied by caller (lambda/function/functor).
	 * @param OutPickedEntry Output selected entry.
	 * @return True when a matching entry was found and removed; false otherwise.
	 * @note Why: Generic building block so many rule types can reuse the same randomized pick flow.
	 */
	template <typename EntryPredicateType>
	bool TryPickRandomMatchingEntry(
		TArray<FBlackboardIdleUnitEntry>& IdleEntries,
		const EntryPredicateType& EntryPredicate,
		FBlackboardIdleUnitEntry& OutPickedEntry)
	{
		TArray<int32> MatchingEntryIndices;
		MatchingEntryIndices.Reserve(IdleEntries.Num());

		for (int32 EntryIndex = 0; EntryIndex < IdleEntries.Num(); ++EntryIndex)
		{
			if (EntryPredicate(IdleEntries[EntryIndex]))
			{
				MatchingEntryIndices.Add(EntryIndex);
			}
		}

		if (MatchingEntryIndices.Num() <= 0)
		{
			return false;
		}

		const int32 RandomMatchingEntryArrayIndex = FMath::RandRange(0, MatchingEntryIndices.Num() - 1);
		const int32 RandomIdleEntryIndex = MatchingEntryIndices[RandomMatchingEntryArrayIndex];
		OutPickedEntry = IdleEntries[RandomIdleEntryIndex];
		IdleEntries.RemoveAtSwap(RandomIdleEntryIndex);
		return true;
	}

	/**
	 * @brief Template helper that repeatedly picks random matching entries until a target total is reached.
	 * @details Uses TryPickRandomMatchingEntry in a loop. Stops when OutPickedEntries reaches TargetPickCount
	 * or when no more matching entries can be found. It never forces invalid picks.
	 * @tparam EntryPredicateType Callable type taking `(const FBlackboardIdleUnitEntry&)` and returning bool.
	 * @param IdleEntries Mutable candidate pool (shrinks as picks succeed).
	 * @param EntryPredicate Matching logic supplied by caller.
	 * @param TargetPickCount Desired final total count in OutPickedEntries.
	 * @param OutPickedEntries Accumulator containing picks from previous and current stages.
	 * @note Why: Encodes required-rule and random-fill behavior once, while allowing custom match logic.
	 */
	template <typename EntryPredicateType>
	void PickRandomEntriesFromMatchingSet(
		TArray<FBlackboardIdleUnitEntry>& IdleEntries,
		const EntryPredicateType& EntryPredicate,
		const int32 TargetPickCount,
		TArray<FBlackboardIdleUnitEntry>& OutPickedEntries)
	{
		while (OutPickedEntries.Num() < TargetPickCount)
		{
			FBlackboardIdleUnitEntry PickedEntry;
			if (not TryPickRandomMatchingEntry(IdleEntries, EntryPredicate, PickedEntry))
			{
				return;
			}

			OutPickedEntries.Add(PickedEntry);
		}
	}

	/**
	 * @brief Creates a result object from final picked entries.
	 * @details Wraps result construction and setup in one helper for concise call sites.
	 * @param PickedEntries Final selected entries.
	 * @return Result object ready for caller consumption.
	 * @note Why: Keeps selection code focused on picking logic, not output-object ceremony.
	 */
	FBlackboardIdleUnitsResult SetupResultFromPickedEntries(const TArray<FBlackboardIdleUnitEntry>& PickedEntries)
	{
		FBlackboardIdleUnitsResult PickedUnits;
		PickedUnits.SetupResultForPickedEntries(PickedEntries);
		return PickedUnits;
	}
}

UEnemyDirectControlComponent::UEnemyDirectControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyDirectControlComponent::InitDirectControlComponent(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
}

void UEnemyDirectControlComponent::BeginPlay()
{
	Super::BeginPlay();
	StartDirectControlTickTimer();
}

void UEnemyDirectControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDirectControlTickTimer();
	Super::EndPlay(EndPlayReason);
}

bool UEnemyDirectControlComponent::EnsureEnemyControllerIsValid() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_EnemyController",
		"UEnemyDirectControlComponent::EnsureEnemyControllerIsValid",
		this);
	return false;
}

bool UEnemyDirectControlComponent::EnsureIsValidBlackboard(FStrategicAIBlackboard*& OutBlackboard) const
{
	if (not EnsureEnemyControllerIsValid())
	{
		OutBlackboard = nullptr;
		return false;
	}
	FStrategicAIBlackboard* Blackboard = M_EnemyController->GetStrategicAIBlackboard();
	if (not Blackboard)
	{
		RTSFunctionLibrary::ReportError(
			"Enemy direct control component requires a valid strategic AI blackboard but it cannot be found on the enemy controller.");
		OutBlackboard = nullptr;
		return false;
	}
	OutBlackboard = Blackboard;
	return true;
}

UEnemyStrategicAIComponent* UEnemyDirectControlComponent::GetValidStrategicAIComponent() const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return nullptr;
	}
	UEnemyStrategicAIComponent* StrategicAIComponent = M_EnemyController->GetEnemyStrategicAIComponent();
	if (IsValid(StrategicAIComponent))
	{
		return StrategicAIComponent;
	}
	RTSFunctionLibrary::ReportError(
		"Enemy direct control component requires a valid strategic AI component but it cannot be"
		"found on the enemy controller.");
	return nullptr;
}

bool UEnemyDirectControlComponent::GetIsValidDirectControlUnitActor(const AActor* UnitActor) const
{
	if (not IsValid(UnitActor))
	{
		RTSFunctionLibrary::ReportError("Enemy direct control component received an invalid unit actor pointer.");
		return false;
	}

	return true;
}

void UEnemyDirectControlComponent::StartDirectControlTickTimer()
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_DirectControlTickTimerHandle,
		this,
		&UEnemyDirectControlComponent::TickDirectControl,
		EnemyDirectControlConstants::DirectControlTickRateSeconds,
		true);
}

void UEnemyDirectControlComponent::StopDirectControlTickTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_DirectControlTickTimerHandle);
}

bool UEnemyDirectControlComponent::RegisterDirectControlUnit(AActor* UnitActor)
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not GetIsValidDirectControlUnitActor(UnitActor) || not EnsureIsValidBlackboard(Blackboard))
	{
		return false;
	}

	if (TryGetCommandsInterface(UnitActor) == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"Enemy direct control component can only register units implementing ICommands.");
		return false;
	}

	if (Blackboard->IdleDirectControlUnits.Contains(UnitActor))
	{
		DebugReportRegisterDeregister("RegisterDirectControlUnit ignored duplicate unit: " + UnitActor->GetName());
		return false;
	}

	FBlackboardIdleUnitEntry IdleUnitEntry;
	if (not TrySetupIdleUnitEntryFromActor(UnitActor, IdleUnitEntry))
	{
		RTSFunctionLibrary::ReportError(
			"Enemy direct control component can only register squad or tank units with a valid RTS component.");
		return false;
	}

	Blackboard->IdleDirectControlUnits.Add(IdleUnitEntry);
	DebugReportRegisterDeregister("RegisterDirectControlUnit added unit: " + UnitActor->GetName());
	return true;
}

bool UEnemyDirectControlComponent::DeregisterDirectControlUnit(AActor* UnitActor)
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not GetIsValidDirectControlUnitActor(UnitActor) || not EnsureIsValidBlackboard(Blackboard))
	{
		return false;
	}

	const int32 RemovedCount = FBlackboardIdleUnitEntry::RemoveByUnitActor(
		Blackboard->IdleDirectControlUnits,
		UnitActor);
	if (RemovedCount <= 0)
	{
		DebugReportRegisterDeregister("DeregisterDirectControlUnit could not find unit: " + UnitActor->GetName());
		return false;
	}

	DebugReportRegisterDeregister("DeregisterDirectControlUnit removed unit: " + UnitActor->GetName());
	return true;
}

void UEnemyDirectControlComponent::ClearDirectControlUnits()
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureEnemyControllerIsValid() || not EnsureIsValidBlackboard(Blackboard))
	{
		return;
	}
	Blackboard->IdleDirectControlUnits.Empty();
	DebugReportRegisterDeregister("ClearDirectControlUnits removed all direct control units.");
}

TArray<AActor*> UEnemyDirectControlComponent::GetRegisteredDirectControlUnits() const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	TArray<AActor*> RegisteredActors;

	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return RegisteredActors;
	}
	RegisteredActors.Reserve(Blackboard->IdleDirectControlUnits.Num());

	for (const auto& RegisteredUnit : Blackboard->IdleDirectControlUnits)
	{
		if (not RegisteredUnit.IsValid())
		{
			continue;
		}

		RegisteredActors.Add(RegisteredUnit.Get());
	}

	return RegisteredActors;
}

FBlackboardIdleUnitsResult UEnemyDirectControlComponent::PickRandomMaxIdleBlackboardUnits(const int32 MaxUnitsToPick) const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return FBlackboardIdleUnitsResult();
	}

	if (MaxUnitsToPick <= 0 || Blackboard->IdleDirectControlUnits.Num() <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	const int32 MaxPicksToProcess = FMath::Min(MaxUnitsToPick, Blackboard->IdleDirectControlUnits.Num());
	TArray<FBlackboardIdleUnitEntry> PickedBlackboardEntries;
	PickedBlackboardEntries.Reserve(MaxPicksToProcess);

	while (PickedBlackboardEntries.Num() < MaxPicksToProcess && Blackboard->IdleDirectControlUnits.Num() > 0)
	{
		const int32 RandomIdleUnitIndex = FMath::RandRange(0, Blackboard->IdleDirectControlUnits.Num() - 1);
		const FBlackboardIdleUnitEntry PickedEntry = Blackboard->IdleDirectControlUnits[RandomIdleUnitIndex];

		// Remove immediately so the same unit can never be picked twice.
		Blackboard->IdleDirectControlUnits.RemoveAtSwap(RandomIdleUnitIndex);
		PickedBlackboardEntries.Add(PickedEntry);
	}

	return SetupResultFromPickedEntries(PickedBlackboardEntries);
}

FBlackboardIdleUnitsResult UEnemyDirectControlComponent::PickRandomMinMaxIdleBlackboardUnits(
	const int32 MinUnitsToPick,
	const int32 MaxUnitsToPick) const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return FBlackboardIdleUnitsResult();
	}

	const int32 UnitsToPick = GetRandomMinMaxPickCount(
		MinUnitsToPick,
		MaxUnitsToPick,
		Blackboard->IdleDirectControlUnits.Num());
	if (UnitsToPick <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	TArray<FBlackboardIdleUnitEntry> PickedBlackboardEntries;
	PickedBlackboardEntries.Reserve(UnitsToPick);

	while (PickedBlackboardEntries.Num() < UnitsToPick)
	{
		const int32 RandomIdleUnitIndex = FMath::RandRange(0, Blackboard->IdleDirectControlUnits.Num() - 1);
		PickedBlackboardEntries.Add(Blackboard->IdleDirectControlUnits[RandomIdleUnitIndex]);
		Blackboard->IdleDirectControlUnits.RemoveAtSwap(RandomIdleUnitIndex);
	}

	return SetupResultFromPickedEntries(PickedBlackboardEntries);
}


FBlackboardIdleUnitsResult UEnemyDirectControlComponent::PickIdleBlackboardUnitsByPolicy(
	const FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return FBlackboardIdleUnitsResult();
	}

	const int32 AvailableUnitCount = CountValidIdleUnitEntries(Blackboard->IdleDirectControlUnits);
	// Avoid random/policy logic when there is no usable data to satisfy any rule.
	if (AvailableUnitCount <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	// We pick from a local copy first so the blackboard stays unchanged unless the full selection succeeds.
	TArray<FBlackboardIdleUnitEntry> CandidateBlackboardEntries = Blackboard->IdleDirectControlUnits;
	TArray<FBlackboardIdleUnitEntry> PickedBlackboardEntries;
	// Reserve upfront to reduce reallocations during repeated Add calls.
	PickedBlackboardEntries.Reserve(FMath::Min(SelectionPolicy.MaxUnitsToPick, AvailableUnitCount));

	for (const FIdleUnitSelectionRule& RequiredRule : SelectionPolicy.RequiredRules)
	{
		const int32 PickedCountBeforeRule = PickedBlackboardEntries.Num();
		const int32 TargetPickCountAfterRule = PickedCountBeforeRule + RequiredRule.RequiredAmount;

		// Required rules are enforced first so hard constraints are guaranteed before optional random fill.
		PickRandomEntriesFromMatchingSet(
			CandidateBlackboardEntries,
			[&RequiredRule](const FBlackboardIdleUnitEntry& IdleEntry)
			{
				return GetDoesEntryMatchSelectionRule(IdleEntry, RequiredRule);
			},
			TargetPickCountAfterRule,
			PickedBlackboardEntries);

		//  Partial rule satisfaction is treated as a full failure to keep behavior predictable.
		if (PickedBlackboardEntries.Num() < TargetPickCountAfterRule)
		{
			return FBlackboardIdleUnitsResult();
		}
	}

	// Final target count is decided after required picks so min/max policy stays coherent with guaranteed picks.
	const int32 UnitsToPick = GetRandomPickCountForPolicy(
		SelectionPolicy,
		PickedBlackboardEntries.Num(),
		AvailableUnitCount);
	if (UnitsToPick <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	// WHY: After hard constraints are met, we fill remaining slots from any valid leftover candidates.
	PickRandomEntriesFromMatchingSet(
		CandidateBlackboardEntries,
		GetIsValidIdleUnitEntry,
		UnitsToPick,
		PickedBlackboardEntries);

	// WHY: Commit to blackboard only at the end, so failed attempts never mutate shared state.
	RemovePickedEntriesFromBlackboard(PickedBlackboardEntries, Blackboard->IdleDirectControlUnits);
	return SetupResultFromPickedEntries(PickedBlackboardEntries);
}

FBlackboardIdleUnitsResult UEnemyDirectControlComponent::PreferSquad_PickRandomMinMax(
	const int32 MinUnitsToPick,
	const int32 MaxUnitsToPick) const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return FBlackboardIdleUnitsResult();
	}

	const int32 UnitsToPick = GetRandomMinMaxPickCount(
		MinUnitsToPick,
		MaxUnitsToPick,
		Blackboard->IdleDirectControlUnits.Num());
	if (UnitsToPick <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	TArray<FBlackboardIdleUnitEntry> PickedBlackboardEntries;
	PickedBlackboardEntries.Reserve(UnitsToPick);

	PickRandomEntriesFromMatchingSet(
		Blackboard->IdleDirectControlUnits,
		GetIsSquadEntry,
		UnitsToPick,
		PickedBlackboardEntries);
	PickRandomEntriesFromMatchingSet(
		Blackboard->IdleDirectControlUnits,
		GetIsTankEntry,
		UnitsToPick,
		PickedBlackboardEntries);

	return SetupResultFromPickedEntries(PickedBlackboardEntries);
}

FBlackboardIdleUnitsResult UEnemyDirectControlComponent::PreferTank_PickRandomMinMax(
	const int32 MinUnitsToPick,
	const int32 MaxUnitsToPick) const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return FBlackboardIdleUnitsResult();
	}

	const int32 UnitsToPick = GetRandomMinMaxPickCount(
		MinUnitsToPick,
		MaxUnitsToPick,
		Blackboard->IdleDirectControlUnits.Num());
	if (UnitsToPick <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	TArray<FBlackboardIdleUnitEntry> PickedBlackboardEntries;
	PickedBlackboardEntries.Reserve(UnitsToPick);

	PickRandomEntriesFromMatchingSet(
		Blackboard->IdleDirectControlUnits,
		GetIsTankEntry,
		UnitsToPick,
		PickedBlackboardEntries);
	PickRandomEntriesFromMatchingSet(
		Blackboard->IdleDirectControlUnits,
		GetIsSquadEntry,
		UnitsToPick,
		PickedBlackboardEntries);

	return SetupResultFromPickedEntries(PickedBlackboardEntries);
}

FBlackboardIdleUnitsResult UEnemyDirectControlComponent::PreferNoHazmats_PickRandomMinMax(
	const int32 MinUnitsToPick,
	const int32 MaxUnitsToPick) const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return FBlackboardIdleUnitsResult();
	}

	const int32 UnitsToPick = GetRandomMinMaxPickCount(
		MinUnitsToPick,
		MaxUnitsToPick,
		Blackboard->IdleDirectControlUnits.Num());
	if (UnitsToPick <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	TArray<FBlackboardIdleUnitEntry> PickedBlackboardEntries;
	PickedBlackboardEntries.Reserve(UnitsToPick);

	PickRandomEntriesFromMatchingSet(
		Blackboard->IdleDirectControlUnits,
		GetIsNotHazmatEngineerSquadEntry,
		UnitsToPick,
		PickedBlackboardEntries);

	if (PickedBlackboardEntries.Num() < MinUnitsToPick)
	{
		PickRandomEntriesFromMatchingSet(
			Blackboard->IdleDirectControlUnits,
			GetIsHazmatEngineerSquadEntry,
			MinUnitsToPick,
			PickedBlackboardEntries);
	}

	return SetupResultFromPickedEntries(PickedBlackboardEntries);
}

FBlackboardIdleUnitsResult UEnemyDirectControlComponent::IdleHazmatEngineers_PickRandomMinMax(
	const int32 MinUnitsToPick,
	const int32 MaxUnitsToPick) const
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return FBlackboardIdleUnitsResult();
	}

	int32 AvailableHazmatCount = 0;
	for (const FBlackboardIdleUnitEntry& IdleEntry : Blackboard->IdleDirectControlUnits)
	{
		if (GetIsHazmatEngineerSquadEntry(IdleEntry))
		{
			++AvailableHazmatCount;
		}
	}

	const int32 UnitsToPick = GetRandomMinMaxPickCount(
		MinUnitsToPick,
		MaxUnitsToPick,
		AvailableHazmatCount);
	if (UnitsToPick <= 0)
	{
		return FBlackboardIdleUnitsResult();
	}

	TArray<FBlackboardIdleUnitEntry> PickedBlackboardEntries;
	PickedBlackboardEntries.Reserve(UnitsToPick);
	PickRandomEntriesFromMatchingSet(
		Blackboard->IdleDirectControlUnits,
		GetIsHazmatEngineerSquadEntry,
		UnitsToPick,
		PickedBlackboardEntries);

	return SetupResultFromPickedEntries(PickedBlackboardEntries);
}


void UEnemyDirectControlComponent::RegisterDirectControlUnits(const TArray<AActor*>& UnitActors)
{
	for (AActor* UnitActor : UnitActors)
	{
		RegisterDirectControlUnit(UnitActor);
	}
}

void UEnemyDirectControlComponent::DeregisterDirectControlUnits(const TArray<AActor*>& UnitActors)
{
	for (AActor* UnitActor : UnitActors)
	{
		DeregisterDirectControlUnit(UnitActor);
	}
}

void UEnemyDirectControlComponent::TickDirectControl()
{
	RemoveInvalidRegisteredUnits();
	DebugDrawRegisteredDirectControlUnits();
}

void UEnemyDirectControlComponent::RemoveInvalidRegisteredUnits()
{
	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return;
	}

	for (int32 UnitIndex = Blackboard->IdleDirectControlUnits.Num() - 1; UnitIndex >= 0; --UnitIndex)
	{
		const TWeakObjectPtr<AActor> RegisteredUnit = Blackboard->IdleDirectControlUnits[UnitIndex];
		if (RegisteredUnit.IsValid())
		{
			continue;
		}

		Blackboard->IdleDirectControlUnits.RemoveAtSwap(UnitIndex);
	}
}

void UEnemyDirectControlComponent::DebugDrawRegisteredDirectControlUnits() const
{
	if constexpr (not DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	FStrategicAIBlackboard* Blackboard = nullptr;
	if (not EnsureIsValidBlackboard(Blackboard))
	{
		return;
	}
	for (TWeakObjectPtr<AActor> RegisteredUnit : Blackboard->IdleDirectControlUnits)
	{
		if (not RegisteredUnit.IsValid())
		{
			continue;
		}

		const FVector UnitLocation = RegisteredUnit->GetActorLocation();

		if constexpr (
			EnemyDirectControlConstants::Debugging::DrawRegisteredUnitLabel
			&& DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
		{
			DrawDebugString(
				World,
				UnitLocation + FVector(0.f, 0.f, EnemyDirectControlConstants::DebugTextOffsetZ),
				TEXT("Direct Control"),
				nullptr,
				FColor::Red,
				EnemyDirectControlConstants::DebugDrawDurationSeconds,
				true);
		}

		if constexpr (
			EnemyDirectControlConstants::Debugging::DrawRegisteredUnitSphere
			&& DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
		{
			DrawDebugSphere(
				World,
				UnitLocation + FVector(0.f, 0.f, EnemyDirectControlConstants::DebugSphereOffsetZ),
				EnemyDirectControlConstants::DebugSphereRadius,
				EnemyDirectControlConstants::DebugSphereSegments,
				FColor::Red,
				false,
				EnemyDirectControlConstants::DebugDrawDurationSeconds,
				0,
				EnemyDirectControlConstants::DebugSphereThickness);
		}
	}
}

void UEnemyDirectControlComponent::DebugReportRegisterDeregister(const FString& Message) const
{
	if constexpr (
		EnemyDirectControlConstants::Debugging::ReportRegisterDeregister
		&& DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, FColor::Red);
	}
}

ICommands* UEnemyDirectControlComponent::TryGetCommandsInterface(AActor* UnitActor) const
{
	if (not GetIsValidDirectControlUnitActor(UnitActor))
	{
		return nullptr;
	}

	if (not UnitActor->GetClass()->ImplementsInterface(UCommands::StaticClass()))
	{
		return nullptr;
	}

	return Cast<ICommands>(UnitActor);
}

TArray<TWeakObjectPtr<AActor>> UEnemyDirectControlComponent::GetRetreatGroupUnitsToExclude() const
{
	TArray<TWeakObjectPtr<AActor>> UnitsToExclude;
	for (const FDamagedTanksRetreatGroup& RetreatGroup : M_RetreatCache.M_CachedRetreatGroups)
	{
		AppendRetreatGroupUnitsToExclude(RetreatGroup, UnitsToExclude);
	}

	return UnitsToExclude;
}

void UEnemyDirectControlComponent::AppendRetreatGroupUnitsToExclude(
	const FDamagedTanksRetreatGroup& RetreatGroup,
	TArray<TWeakObjectPtr<AActor>>& OutUnitsToExclude) const
{
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		if (not DamagedTank.IsValid())
		{
			continue;
		}

		OutUnitsToExclude.AddUnique(DamagedTank);
	}

	for (const FWeakActorLocations& HazmatData : RetreatGroup.HazmatsWithFormationLocations)
	{
		if (not HazmatData.Actor.IsValid())
		{
			continue;
		}

		OutUnitsToExclude.AddUnique(HazmatData.Actor);
	}
}


void UEnemyDirectControlComponent::HandleAsyncRetreatGroupResult(const FResultAlliedTanksToRetreat& RetreatResult)
{
	M_RetreatCache.M_LastRequestID = RetreatResult.RequestID;

	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group1);
	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group2);
	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group3);

	TArray<FDamagedTanksRetreatGroup> NewGroups = {RetreatResult.Group1, RetreatResult.Group2, RetreatResult.Group3};

	for (FDamagedTanksRetreatGroup& RetreatGroup : NewGroups)
	{
		OnRetreatGroupFound(RetreatGroup);
	}
}

void UEnemyDirectControlComponent::OnRetreatGroupFound(FDamagedTanksRetreatGroup& RetreatGroup)
{
	if (RetreatGroup.DamagedTanks.IsEmpty())
	{
		return;
	}

	RemoveRetreatGroupUnitsFromFormations(RetreatGroup);
	IgnoreHazmatsInFieldConstruction(RetreatGroup);
	IssueRetreatOrdersToDamagedTanks(RetreatGroup);
	if (IsRetreatGroupDamagedTanksOnly(RetreatGroup))
	{
		MarkRetreatGroupTanksOnlyRetreating(RetreatGroup);
		return;
	}

	IssueHazmatSupportOrders(RetreatGroup);
	MarkRetreatGroupHazmatsRepairing(RetreatGroup);
}


void UEnemyDirectControlComponent::RemoveRetreatGroupUnitsFromFormations(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	UEnemyFormationController* FormationController = nullptr;
	if (not EnsureFormationControllerIsValid(FormationController))
	{
		return;
	}

	TArray<ATankMaster*> DamagedTanks;
	TArray<ASquadController*> NoSquads;
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		ATankMaster* TankMaster = Cast<ATankMaster>(DamagedTank.Get());
		if (not IsValid(TankMaster))
		{
			continue;
		}

		DamagedTanks.Add(TankMaster);
	}

	for (const FWeakActorLocations& HazmatData : RetreatGroup.HazmatsWithFormationLocations)
	{
		ATankMaster* TankMaster = Cast<ATankMaster>(HazmatData.Actor.Get());
		if (IsValid(TankMaster))
		{
			DamagedTanks.AddUnique(TankMaster);
		}
	}

	TArray<AActor*> RemovedUnits = FormationController->RemoveUnitsFromAnyFormation(NoSquads, DamagedTanks);
	if constexpr (DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols &&
		EnemyDirectControlConstants::Debugging::DebugRemoveDirectControlUnitFromFormations)
	{
		Debug_RetreatUnitsRemovedFromFormations(RemovedUnits);
	}
}

void UEnemyDirectControlComponent::IgnoreHazmatsInFieldConstruction(FDamagedTanksRetreatGroup& RetreatGroup) const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UEnemyFieldConstructionComponent* EnemyFieldConstructionComponent = M_EnemyController->
		GetEnemyFieldConstructionComponent();
	if (not IsValid(EnemyFieldConstructionComponent))
	{
		return;
	}

	const TArray<AActor*> HazmatUnitsToIgnore = EnemyFieldConstructionComponent->
		GetHazmatUnitsAlreadyAssignedToFieldConstruction(RetreatGroup.HazmatsWithFormationLocations);
	if (HazmatUnitsToIgnore.IsEmpty())
	{
		return;
	}

	RetreatGroup.HazmatsWithFormationLocations.RemoveAll(
		[&HazmatUnitsToIgnore](const FWeakActorLocations& HazmatData)
		{
			return HazmatUnitsToIgnore.Contains(HazmatData.Actor.Get());
		});

	if constexpr (DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols &&
		EnemyDirectControlConstants::Debugging::DebugIgnoredFieldconstructionHazmats)
	{
		Debug_IgnoredFieldconstructionHazmats(HazmatUnitsToIgnore);
	}
}

bool UEnemyDirectControlComponent::IsRetreatGroupDamagedTanksOnly(const FDamagedTanksRetreatGroup& RetreatGroup) const
{
	return RetreatGroup.HazmatsWithFormationLocations.IsEmpty();
}

void UEnemyDirectControlComponent::MarkRetreatGroupTanksOnlyRetreating(FDamagedTanksRetreatGroup& RetreatGroup) const
{
	RetreatGroup.M_CurrentRetreatGroupState = EEnemyRetreatGroupState::TanksOnlyRetreating;
}

void UEnemyDirectControlComponent::IssueRetreatOrdersToDamagedTanks(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		AActor* DamagedTankActor = DamagedTank.Get();
		ICommands* Commands = TryGetCommandsInterface(DamagedTankActor);
		if (Commands == nullptr)
		{
			continue;
		}

		FVector ProjectedLocation = FVector::ZeroVector;
		if (not TryGetProjectedLocation(DamagedTankActor->GetActorLocation(), ProjectedLocation))
		{
			continue;
		}

		Commands->ReverseUnitToLocation(ProjectedLocation, true);
	}
}

void UEnemyDirectControlComponent::IssueHazmatSupportOrders(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	AActor* FirstDamagedTank = GetFirstValidDamagedTank(RetreatGroup);
	if (not IsValid(FirstDamagedTank))
	{
		return;
	}

	int32 DamagedTankIndex = 0;
	for (const FWeakActorLocations& HazmatData : RetreatGroup.HazmatsWithFormationLocations)
	{
		ICommands* HazmatCommands = TryGetCommandsInterface(HazmatData.Actor.Get());
		if (HazmatCommands == nullptr || HazmatData.Locations.IsEmpty())
		{
			continue;
		}

		FVector ProjectedLocation = FVector::ZeroVector;
		if (not TryGetProjectedLocation(HazmatData.Locations[0], ProjectedLocation))
		{
			continue;
		}

		HazmatCommands->MoveToLocation(ProjectedLocation, true, FRotator::ZeroRotator, false);

		AActor* DamagedTankTarget = FirstDamagedTank;
		if (RetreatGroup.DamagedTanks.IsValidIndex(DamagedTankIndex) && RetreatGroup.DamagedTanks[DamagedTankIndex].
			IsValid())
		{
			DamagedTankTarget = RetreatGroup.DamagedTanks[DamagedTankIndex].Get();
		}

		HazmatCommands->RepairActor(DamagedTankTarget, false);
		DamagedTankIndex = (DamagedTankIndex + 1) % FMath::Max(RetreatGroup.DamagedTanks.Num(), 1);
	}
}

void UEnemyDirectControlComponent::MarkRetreatGroupHazmatsRepairing(FDamagedTanksRetreatGroup& RetreatGroup) const
{
	RetreatGroup.M_CurrentRetreatGroupState = EEnemyRetreatGroupState::HazmatsMoveAndRepairTanks;
}

AActor* UEnemyDirectControlComponent::GetFirstValidDamagedTank(const FDamagedTanksRetreatGroup& RetreatGroup) const
{
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		if (DamagedTank.IsValid())
		{
			return DamagedTank.Get();
		}
	}

	return nullptr;
}

bool UEnemyDirectControlComponent::TryGetProjectedLocation(const FVector& OriginalLocation,
                                                           FVector& OutProjectedLocation) const
{
	OutProjectedLocation = OriginalLocation;

	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	UEnemyNavigationAIComponent* EnemyNavigationAIComponent = M_EnemyController->GetEnemyNavigationAIComponent();
	if (not IsValid(EnemyNavigationAIComponent))
	{
		return false;
	}

	return EnemyNavigationAIComponent->GetNavigablePoint(OriginalLocation, 1.f, OutProjectedLocation);
}

bool UEnemyDirectControlComponent::EnsureFormationControllerIsValid(
	UEnemyFormationController*& OutFormationController) const
{
	OutFormationController = nullptr;

	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	OutFormationController = M_EnemyController->GetEnemyFormationController();
	if (IsValid(OutFormationController))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Enemy direct control component requires a valid enemy formation controller.");
	return false;
}

void UEnemyDirectControlComponent::Debug_RetreatUnitsRemovedFromFormations(const TArray<AActor*>& RemovedUnits) const
{
	for (const auto EachUnit : RemovedUnits)
	{
		if (not IsValid(EachUnit))
		{
			continue;
		}
		using namespace EnemyDirectControlConstants::Debugging;
		const FVector Location = EachUnit->GetActorLocation() + FVector(0, 0, RemoveFromFormationZOffset);

		DrawDebugString(GetWorld(), Location, "REMOVED from formations for retreat", nullptr,
		                RemoveFromFormationColor, RemoveFromLocationDebugTime, false, 1);
	}
}

void UEnemyDirectControlComponent::Debug_IgnoredFieldconstructionHazmats(
	const TArray<AActor*>& IgnoredHazmatUnits) const
{
	for (const AActor* IgnoredHazmatUnit : IgnoredHazmatUnits)
	{
		if (not IsValid(IgnoredHazmatUnit))
		{
			continue;
		}

		using namespace EnemyDirectControlConstants::Debugging;
		const FVector Location = IgnoredHazmatUnit->GetActorLocation() + FVector(0, 0, RemoveFromFormationZOffset);

		DrawDebugString(GetWorld(), Location, "IGNORED for retreat (field construction)", nullptr,
		                RemoveFromFormationColor, RemoveFromLocationDebugTime, false, 1);
	}
}
