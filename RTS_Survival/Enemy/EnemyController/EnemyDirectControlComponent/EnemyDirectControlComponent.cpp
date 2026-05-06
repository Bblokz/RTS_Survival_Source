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

	bool GetIsSupportedDirectControlUnitType(const EAllUnitType UnitType)
	{
		return UnitType == EAllUnitType::UNType_Squad || UnitType == EAllUnitType::UNType_Tank;
	}

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

	bool GetIsSquadEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		return Entry.UnitType == EAllUnitType::UNType_Squad;
	}

	bool GetIsTankEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		return Entry.UnitType == EAllUnitType::UNType_Tank;
	}

	bool GetIsHazmatEngineerSquadEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		if (Entry.UnitType != EAllUnitType::UNType_Squad)
		{
			return false;
		}

		const ESquadSubtype SquadSubtype = static_cast<ESquadSubtype>(Entry.UnitSubtypeRaw);
		return SquadSubtype == ESquadSubtype::Squad_Rus_HazmatEngineers;
	}

	bool GetIsNotHazmatEngineerSquadEntry(const FBlackboardIdleUnitEntry& Entry)
	{
		return not GetIsHazmatEngineerSquadEntry(Entry);
	}

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
