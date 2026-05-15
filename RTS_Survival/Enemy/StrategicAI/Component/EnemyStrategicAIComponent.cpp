// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyStrategicAIComponent.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardQueries/BlackboardQueryHelpers.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticHelpers/StochasticHelpers.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyTrainingHelpers/EnemyTrainingHelpers.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UEnemyStrategicAIComponent::UEnemyStrategicAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CacheGenerationSeedFromGameInstance();
}

void UEnemyStrategicAIComponent::InitStrategicAIComponent(AEnemyController* EnemyController,
                                                          UStochasticDecisionTree* StochasticDecisionTree)
{
	M_StochasticDecisionTree = StochasticDecisionTree;
	M_EnemyController = EnemyController;
	CacheGenerationSeedFromGameInstance();
	const float Now = GetWorld()->GetTimeSeconds();
	PreThinKStep_InitThinkingTimers(Now);
	CacheGenerationSeedFromGameInstance();
	StartStrategicAIThinkingTimer();
}


void UEnemyStrategicAIComponent::QueueFindClosestFlankableEnemyHeavyRequest(
	const FFindClosestFlankableEnemyHeavy& Request)
{
	M_PendingRequests.FindClosestFlankableEnemyHeavyRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueGetPlayerUnitCountsAndBaseRequest(
	const FGetPlayerUnitCountsAndBase& Request)
{
	M_PendingRequests.GetPlayerUnitCountsAndBaseRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindAlliedTanksToRetreatRequest(
	const FFindAlliedTanksToRetreat& Request)
{
	M_PendingRequests.FindAlliedTanksToRetreatRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindEnemyBaseClustersRequest(const FFindEnemyBaseClusters& Request)
{
	M_PendingRequests.FindEnemyBaseClustersRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindLocationsUnderPlayerAttackRequest(
	const FFindLocationsUnderPlayerAttack& Request)
{
	M_PendingRequests.FindLocationsUnderPlayerAttackRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindPlayerUnitBulkLocationsRequest(
	const FFindPlayerUnitBulkLocations& Request)
{
	M_PendingRequests.FindPlayerUnitBulkLocationsRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindConstructionLocationsRequest(
	const FFindConstructionLocations& Request)
{
	M_PendingRequests.FindConstructionLocationsRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindPlayerHeavyTankFlankLocationsRequest(
	const FFindClosestFlankableEnemyHeavy& Request)
{
	M_PendingRequests.FindClosestFlankableEnemyHeavyRequests.Add(Request);
}

void UEnemyStrategicAIComponent::RequestRetreatDamagedTanks(const FFindAlliedTanksToRetreat& Request)
{
	FFindAlliedTanksToRetreat RequestToQueue = Request;
	FillRetreatRequestExcludedUnits(RequestToQueue);
	QueueFindAlliedTanksToRetreatRequest(RequestToQueue);
}

const FStrategicAIResultBatch& UEnemyStrategicAIComponent::GetLatestStrategicAIResults() const
{
	return M_LatestResults;
}

const FStrategicAIBlackboard& UEnemyStrategicAIComponent::GetStrategicAIBlackboard() const
{
	return M_Blackboard;
}

FStrategicAIBlackboard& UEnemyStrategicAIComponent::GetEditableStrategicAIBlackboard()
{
	return M_Blackboard;
}

FEnemyStrategicTrainingState& UEnemyStrategicAIComponent::GetEditableEnemyTrainingState()
{
	return M_TrainingState;
}


void UEnemyStrategicAIComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyStrategicAIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStrategicAIThinkingTimer();
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		for (FEnemyStrategicAITrainingSpawnBatch& SpawnBatch : M_ActiveTrainingSpawnBatches)
		{
			World->GetTimerManager().ClearTimer(SpawnBatch.TimerHandle);
		}
	}

	M_ActiveTrainingSpawnBatches.Empty();
	Super::EndPlay(EndPlayReason);
}

void UEnemyStrategicAIComponent::BeginPlay_CacheUnitManager()
{
	M_GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	(void)EnsureIsValidUnitManager();
}

bool UEnemyStrategicAIComponent::EnsureIsValidUnitManager()
{
	if (not M_GameUnitManager.IsValid())
	{
		M_GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
		if (not M_GameUnitManager.IsValid())
		{
			RTSFunctionLibrary::ReportError("Enemy strategic AI requires a valid game unit manager but "
				"cannot get it through helper function in FRTS_Statics.");
			return false;
		}
	}
	return true;
}


bool UEnemyStrategicAIComponent::GetIsAllowedDirectControlUnits() const
{
	return M_Blackboard.StrategicAIMissionSettings.bAllowDirectControlStochasticDecisionTree;
}

bool UEnemyStrategicAIComponent::GetIsAllowedUnitTraining() const
{
	return M_Blackboard.StrategicAIMissionSettings.bAllowUnitTraining;
}

void UEnemyStrategicAIComponent::PreThinKStep_InitThinkingTimers(const float Now)
{
	M_AIBaseLocationThinkTimer.LastTimeThought = Now;
	M_AIBaseLocationThinkTimer.ThinkingInterval = EnemyAISettings::ThinkingTimers::UpdateAIBaseLocations_Interval;
	M_AIBaseLocationThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::AIBaseLocation_ThinkStep);
	M_AIThinkTimers.Add(&M_AIBaseLocationThinkTimer);

	M_PlayerUnitCountsBuildingCountsThinkTimer.LastTimeThought = Now;
	M_PlayerUnitCountsBuildingCountsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerCountsBaseLocations_Interval;
	M_PlayerUnitCountsBuildingCountsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerUnitCountsBuildingCounts_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerUnitCountsBuildingCountsThinkTimer);

	M_LocationsUnderAttackThinkTimer.LastTimeThought = Now;
	M_LocationsUnderAttackThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateLocationsUnderPlayerAttack_Interval;
	M_LocationsUnderAttackThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::LocationsUnderAttack_ThinkStep);
	M_AIThinkTimers.Add(&M_LocationsUnderAttackThinkTimer);

	M_PlayerUnitBulkLocationsThinkTimer.LastTimeThought = Now;
	M_PlayerUnitBulkLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerUnitBulkLocations_Interval;
	M_PlayerUnitBulkLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerUnitBulkLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerUnitBulkLocationsThinkTimer);

	M_ConstructionLocationsThinkTimer.LastTimeThought = Now;
	M_ConstructionLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateConstructionLocations_Interval;
	M_ConstructionLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::ConstructionLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_ConstructionLocationsThinkTimer);

	M_PlayerHeavyTankFlankLocationsThinkTimer.LastTimeThought = Now;
	M_PlayerHeavyTankFlankLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerHeavyTankFlank_Interval;
	M_PlayerHeavyTankFlankLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerHeavyTankFlankLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerHeavyTankFlankLocationsThinkTimer);

	M_TrainingPressureThinkTimer.LastTimeThought = Now;
	M_TrainingPressureThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateEnemyTrainingPressure_Interval;
	M_TrainingPressureThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::TrainingPressure_ThinkStep);
	M_AIThinkTimers.Add(&M_TrainingPressureThinkTimer);

	M_TrainingRequirementsThinkTimer.LastTimeThought = Now;
	M_TrainingRequirementsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateAITechLevel_Interval;
	M_TrainingRequirementsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::TrainingRequirements_ThinkStep);
	M_AIThinkTimers.Add(&M_TrainingRequirementsThinkTimer);

	M_TrainingPointsThinkTimer.LastTimeThought = Now;
	M_TrainingPointsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateEnemyTrainingPoints_Interval;
	M_TrainingPointsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::GetTrainingPoints_ThinkStep);
	M_AIThinkTimers.Add(&M_TrainingPointsThinkTimer);
}

void UEnemyStrategicAIComponent::AIBaseLocation_ThinkStep()
{
	// Add player avg attackers locations and add hq, resource building locations for potential attack locs.
	AddPlayerUnitLocationsForDefensePositionsOfEnemyBases(FindEnemyBase_TimerRequest);
	QueueFindEnemyBaseClustersRequest(FindEnemyBase_TimerRequest);
}

void UEnemyStrategicAIComponent::AddPlayerUnitLocationsForDefensePositionsOfEnemyBases(
	FFindEnemyBaseClusters& OutFindBaseRequest)
{
	if (BlackboardQueries::HasValidPlayerAttackingLocations(M_Blackboard))
	{
		for (const auto& EachUnderAttLoc : M_Blackboard.CurrentLocationsUnderPlayerAttack.
		                                                LocationsUnderAttack)
		{
			// Always checked for near zero on async processor!
			OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Add(EachUnderAttLoc.AverageAttackerLocation);
		}
	}
	if (BlackboardQueries::HasValidPlayerUnitBulkLocations(M_Blackboard))
	{
		// Always checked for near zero on async processor!
		for (const auto& EachBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
		{
			OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Add(EachBulk.BulkLocation);
		}
	}
	if (BlackboardQueries::HasValidPlayerHQLocation(M_Blackboard))
	{
		OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Add(
			M_Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation);
	}
	if (BlackboardQueries::HasValidPlayerResourceBuildingLocations(M_Blackboard))
	{
		// Note that any locations in a locations array on the blackboard are always checked for near zero!
		OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Append(
			M_Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings);
	}
}

void UEnemyStrategicAIComponent::PlayerUnitCountsBuildingCounts_ThinkStep()
{
	QueueGetPlayerUnitCountsAndBaseRequest(FindPlayerCountBase_TimerRequest);
}

void UEnemyStrategicAIComponent::LocationsUnderAttack_ThinkStep()
{
	QueueFindLocationsUnderPlayerAttackRequest(FindLocationsUnderAttack_TimerRequest);
}

void UEnemyStrategicAIComponent::PlayerUnitBulkLocations_ThinkStep()
{
	QueueFindPlayerUnitBulkLocationsRequest(FindPlayerUnitBulkLocations_TimerRequest);
}

void UEnemyStrategicAIComponent::ConstructionLocations_ThinkStep()
{
	FFindConstructionLocations RequestToQueue = FindConstructionLocations_TimerRequest;
	FillConstructionLocationsTimerRequest(RequestToQueue);
	if (RequestToQueue.DefensePositions.IsEmpty() || RequestToQueue.PlayerBulkLocations.IsEmpty())
	{
		return;
	}

	QueueFindConstructionLocationsRequest(RequestToQueue);
}

void UEnemyStrategicAIComponent::FillConstructionLocationsTimerRequest(
	FFindConstructionLocations& RequestToFill) const
{
	RequestToFill.DefensePositions = M_Blackboard.CurrentBaseDefensePositions;
	RequestToFill.PlayerBulkLocations.Reset();
	RequestToFill.PlayerBulkLocations.Reserve(M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks.Num());
	for (const FPlayerUnitBulkLocation& PlayerUnitBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
	{
		if (PlayerUnitBulk.BulkLocation.IsNearlyZero())
		{
			continue;
		}

		RequestToFill.PlayerBulkLocations.Add(PlayerUnitBulk.BulkLocation);
	}
}

void UEnemyStrategicAIComponent::PlayerHeavyTankFlankLocations_ThinkStep()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	RemoveExpiredHeavyTankFlankingResults(World->GetTimeSeconds());
	FindPlayerHeavyTankFlankLocations_TimerRequest.RequestID = 0;
	QueueFindPlayerHeavyTankFlankLocationsRequest(FindPlayerHeavyTankFlankLocations_TimerRequest);
}

void UEnemyStrategicAIComponent::RemoveExpiredHeavyTankFlankingResults(const float CurrentTimeSeconds)
{
	const float ResultMaxAgeSeconds = FindPlayerHeavyTankFlankLocations_TimerRequest.ResultMaxAgeSeconds;
	if (ResultMaxAgeSeconds < 0.f)
	{
		return;
	}

	M_Blackboard.AgreggatedHeavyTankFlankingResults.RemoveAll(
		[CurrentTimeSeconds, ResultMaxAgeSeconds](const FResultClosestFlankableEnemyHeavy& Result)
		{
			return Result.ResultTimestampSeconds < 0.f
				|| CurrentTimeSeconds - Result.ResultTimestampSeconds > ResultMaxAgeSeconds;
		});
}

/**
* AI chooses a batch.

AI cannot afford it.

AI stores it as M_TrainingReservation.

Every later Training_ThinkStep() tries to pay for that exact batch first.

If still too expensive, the reservation blocks new random training choices.

Training points accumulate elsewhere over time.

Eventually the AI has enough points.

Training_TrySpendTrainingPoints() succeeds.

The reserved batch is created.

M_TrainingReservation.Reset() clears the reservation.

The next training think step can evaluate fresh pressure again.
 */
void UEnemyStrategicAIComponent::Training_ThinkStep()
{
	// Reserved batches are handled first so saved pressure cannot be replaced by cheaper random picks.
	// “Do I currently have at least ReservedTrainingBatch.TrainingPointCost training points?”
	if (Training_HandleReservedTrainingBatchIfAny())
	{
		return;
	}

	// Build the legal unit pool first, because pressure should only be spent when trainable options exist.
	TArray<ETankSubtype> AvailableTankSubtypes;
	TArray<ESquadSubtype> AvailableSquadSubtypes;
	Training_CollectAvailableTrainingOptions(AvailableTankSubtypes, AvailableSquadSubtypes);
	if (AvailableTankSubtypes.IsEmpty() && AvailableSquadSubtypes.IsEmpty())
	{
		return;
	}

	// Spending the selected buckets here prevents the same strategic need from dominating every think step.
	// As picked buckets get pressure removed.
	const FEnemyStrategicTrainingSelection Selection = M_TrainingState.PickAndSpendTrainingSelection();
	const bool bHasTrainingSelection = Selection.Focus != EAITrainingFocus::NoFocus
		|| Selection.Specialty != EAITrainingFocusSpecialty::NoTrainingPressure;
	if (not bHasTrainingSelection)
	{
		return;
	}

	// Apply the broad strategic selection after unlock checks so random batching only sees useful,
	// currently trainable and selection-compatible choices.
	TArray<ETankSubtype> FilteredTankSubtypes;
	TArray<ESquadSubtype> FilteredSquadSubtypes;
	Training_FilterTrainingOptionsForSelection(
		Selection,
		AvailableTankSubtypes,
		AvailableSquadSubtypes,
		FilteredTankSubtypes,
		FilteredSquadSubtypes);
	if (FilteredTankSubtypes.IsEmpty() && FilteredSquadSubtypes.IsEmpty())
	{
		return;
	}

	// Pick exact units last so availability, pressure, and cost all describe the same batch.
	const FEnemyStrategicAITrainingBatch TrainingBatch = Training_BuildRandomTrainingBatch(
		FilteredTankSubtypes,
		FilteredSquadSubtypes);
	if (TrainingBatch.IsEmpty())
	{
		return;
	}

	// Immediate spending keeps affordable responses fast; reservation preserves unaffordable intent for future income.
	if (Training_TrySpendTrainingPoints(TrainingBatch.TrainingPointCost))
	{
		CreateTrainingBatch(TrainingBatch.TankSubtypes, TrainingBatch.SquadSubtypes);
		return;
	}

	M_TrainingReservation.ReserveBatch(TrainingBatch);
}

bool UEnemyStrategicAIComponent::Training_HandleReservedTrainingBatchIfAny()
{
	// Returning false lets the caller continue with new pressure only when no previous choice is waiting.
	if (not M_TrainingReservation.HasReservation())
	{
		return false;
	}

	const FEnemyStrategicAITrainingBatch& ReservedTrainingBatch = M_TrainingReservation.ReservedTrainingBatch;
	// “Do I currently have at least ReservedTrainingBatch.TrainingPointCost training points?”
	if (not Training_TrySpendTrainingPoints(ReservedTrainingBatch.TrainingPointCost))
	{
		// The reservation still owns this think step, preventing new random batches from stealing saved points.
		return true;
	}

	// Once the exact reserved batch is paid for, clear the reservation so fresh pressure can be evaluated next time.
	CreateTrainingBatch(ReservedTrainingBatch.TankSubtypes, ReservedTrainingBatch.SquadSubtypes);
	M_TrainingReservation.Reset();
	return true;
}

void UEnemyStrategicAIComponent::Training_CollectAvailableTrainingOptions(
	TArray<ETankSubtype>& OutTankSubtypes,
	TArray<ESquadSubtype>& OutSquadSubtypes) const
{
	const FEnemyLevelTraining& EnemyLevelTraining = M_TrainingState.EnemyLevelTraining;
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.BasicInfantryOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.LightTankOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.MediumTankOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.Tier2Options, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.AdvancedInfantryOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.Tier3Options, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.ExperimentalOptions, OutTankSubtypes, OutSquadSubtypes);
}

void UEnemyStrategicAIComponent::Training_AddUnlockedTrainingOptions(
	const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
	TArray<ETankSubtype>& OutTankSubtypes,
	TArray<ESquadSubtype>& OutSquadSubtypes) const
{
	const bool* const bIsTechLevelUnlocked = M_TrainingState.TechLevelUnlockedMap.Find(TrainingOptions.TechLevel);
	if (bIsTechLevelUnlocked == nullptr || not*bIsTechLevelUnlocked)
	{
		return;
	}

	OutTankSubtypes.Append(TrainingOptions.TrainableTankSubtypes);
	OutSquadSubtypes.Append(TrainingOptions.TrainableSquadSubtypes);
}

void UEnemyStrategicAIComponent::Training_FilterTrainingOptionsForSelection(
	const FEnemyStrategicTrainingSelection& Selection,
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes,
	TArray<ETankSubtype>& OutTankSubtypes,
	TArray<ESquadSubtype>& OutSquadSubtypes) const
{
	// Keep tanks and squads in separate pools because later batch construction must preserve their different spawn paths.
	for (const ETankSubtype TankSubtype : TankSubtypes)
	{
		if (Training_GetDoesTankFitSelection(TankSubtype, Selection))
		{
			OutTankSubtypes.Add(TankSubtype);
		}
	}

	// Run the same selection over infantry so mixed focus/specialty pressure can still pick either type.
	for (const ESquadSubtype SquadSubtype : SquadSubtypes)
	{
		if (Training_GetDoesSquadFitSelection(SquadSubtype, Selection))
		{
			OutSquadSubtypes.Add(SquadSubtype);
		}
	}
}

bool UEnemyStrategicAIComponent::Training_GetDoesTankFitSelection(
	const ETankSubtype TankSubtype,
	const FEnemyStrategicTrainingSelection& Selection) const
{
	if (Selection.Focus == EAITrainingFocus::Infantry)
	{
		return false;
	}

	if (Selection.Focus == EAITrainingFocus::LightTanks && not Global_GetIsLightTank(TankSubtype))
	{
		return false;
	}

	if (Selection.Focus == EAITrainingFocus::MediumTanks && not Global_GetIsMediumTank(TankSubtype))
	{
		return false;
	}

	if (Selection.Focus == EAITrainingFocus::HeavyTanks && not Global_GetIsHeavyTank(TankSubtype))
	{
		return false;
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiTank)
	{
		return Global_GetIsTankDestroyer(TankSubtype);
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiInfantry)
	{
		return not Global_GetIsTankDestroyer(TankSubtype);
	}

	return true;
}

bool UEnemyStrategicAIComponent::Training_GetDoesSquadFitSelection(
	const ESquadSubtype SquadSubtype,
	const FEnemyStrategicTrainingSelection& Selection) const
{
	if (Selection.Focus != EAITrainingFocus::NoFocus && Selection.Focus != EAITrainingFocus::Infantry)
	{
		return false;
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiTank)
	{
		return Global_GetIsAntiTankSquad(SquadSubtype);
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiInfantry)
	{
		return not Global_GetIsAntiTankSquad(SquadSubtype);
	}

	return true;
}

FEnemyStrategicAITrainingBatch UEnemyStrategicAIComponent::Training_BuildRandomTrainingBatch(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes) const
{
	FEnemyStrategicAITrainingBatch TrainingBatch;
	const int32 TotalOptionCount = TankSubtypes.Num() + SquadSubtypes.Num();
	if (TotalOptionCount <= 0)
	{
		return TrainingBatch;
	}

	const FEnemyAIMissionSettings& MissionSettings = M_Blackboard.StrategicAIMissionSettings;
	const int32 MinUnitsTrainedPerBatch = FMath::Max(1, MissionSettings.MinUnitsTrainedPerBatch);
	const int32 MaxUnitsTrainedPerBatch = FMath::Max(MinUnitsTrainedPerBatch, MissionSettings.MaxUnitsTrainedPerBatch);
	const int32 UnitPickRange = MaxUnitsTrainedPerBatch - MinUnitsTrainedPerBatch + 1;
	const int32 UnitsToPick = MinUnitsTrainedPerBatch + GetSeededIndex(UnitPickRange);
	for (int32 UnitPickIndex = 0; UnitPickIndex < UnitsToPick; ++UnitPickIndex)
	{
		const int32 OptionIndex = GetSeededIndex(TotalOptionCount, UnitPickIndex);
		if (TankSubtypes.IsValidIndex(OptionIndex))
		{
			TrainingBatch.TankSubtypes.Add(TankSubtypes[OptionIndex]);
			continue;
		}

		const int32 SquadOptionIndex = OptionIndex - TankSubtypes.Num();
		if (SquadSubtypes.IsValidIndex(SquadOptionIndex))
		{
			TrainingBatch.SquadSubtypes.Add(SquadSubtypes[SquadOptionIndex]);
		}
	}

	TrainingBatch.TrainingPointCost = Training_GetTrainingBatchCost(
		TrainingBatch.TankSubtypes,
		TrainingBatch.SquadSubtypes);
	return TrainingBatch;
}

int32 UEnemyStrategicAIComponent::Training_GetTrainingBatchCost(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes) const
{
	int32 TrainingPointCost = 0;
	for (const ETankSubtype TankSubtype : TankSubtypes)
	{
		TrainingPointCost += EnemyTrainingHelpers::GetTankTrainingPointsCost(TankSubtype);
	}

	for (const ESquadSubtype SquadSubtype : SquadSubtypes)
	{
		TrainingPointCost += EnemyTrainingHelpers::GetSquadTrainingPointCost(SquadSubtype);
	}

	return TrainingPointCost;
}

bool UEnemyStrategicAIComponent::Training_TrySpendTrainingPoints(const int32 TrainingPointCost)
{
	if (TrainingPointCost <= 0)
	{
		return false;
	}

	if (M_TrainingState.TrainingPoints < TrainingPointCost)
	{
		return false;
	}

	M_TrainingState.TrainingPoints -= TrainingPointCost;
	return true;
}

void UEnemyStrategicAIComponent::CreateTrainingBatch(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes)
{
	TArray<FTrainingOption> TrainingOptionsToSpawn;
	CreateTrainingOptionsToSpawn(TankSubtypes, SquadSubtypes, TrainingOptionsToSpawn);
	if (TrainingOptionsToSpawn.IsEmpty())
	{
		return;
	}

	FTransform SpawnTransform;
	if (not GetValidTrainingSpawnTransform(SpawnTransform))
	{
		return;
	}

	FVector ProjectedSpawnLocation = FVector::ZeroVector;
	if (not TryProjectTrainingLocationToNavmesh(SpawnTransform.GetLocation(), ProjectedSpawnLocation))
	{
		return;
	}

	StartTrainingSpawnBatchTimer(TrainingOptionsToSpawn, ProjectedSpawnLocation);
}

void UEnemyStrategicAIComponent::CreateTrainingOptionsToSpawn(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes,
	TArray<FTrainingOption>& OutTrainingOptions) const
{
	OutTrainingOptions.Reset();
	OutTrainingOptions.Reserve(TankSubtypes.Num() + SquadSubtypes.Num());
	for (const ETankSubtype TankSubtype : TankSubtypes)
	{
		if (TankSubtype == ETankSubtype::Tank_None)
		{
			continue;
		}

		OutTrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtype)));
	}

	for (const ESquadSubtype SquadSubtype : SquadSubtypes)
	{
		if (SquadSubtype == ESquadSubtype::Squad_None)
		{
			continue;
		}

		OutTrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype)));
	}
}

bool UEnemyStrategicAIComponent::TryProjectTrainingLocationToNavmesh(
	const FVector& TrainingLocation,
	FVector& OutProjectedTrainingLocation) const
{
	return StochasticHelpers::CanProjectNavigable_TrainingLocation(
		this,
		TrainingLocation,
		OutProjectedTrainingLocation);
}

void UEnemyStrategicAIComponent::StartTrainingSpawnBatchTimer(
	const TArray<FTrainingOption>& TrainingOptionsToSpawn,
	const FVector& SpawnLocation)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	FEnemyStrategicAITrainingSpawnBatch& SpawnBatch = M_ActiveTrainingSpawnBatches.AddDefaulted_GetRef();
	SpawnBatch.TrainingOptionsToSpawn = TrainingOptionsToSpawn;
	SpawnBatch.SpawnLocation = SpawnLocation;
	SpawnBatch.BatchID = M_NextTrainingSpawnBatchID++;

	const TWeakObjectPtr<UEnemyStrategicAIComponent> WeakThis(this);
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindLambda([WeakThis, BatchID = SpawnBatch.BatchID]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->TrainingSpawnBatchTimerTick(BatchID);
	});

	constexpr float MinimumTrainingSpawnInterval = 0.01f;
	const float TrainingSpawnInterval = FMath::Max(
		EnemyAISettings::UnitTraining::TrainingBatchSpawnInterval,
		MinimumTrainingSpawnInterval);
	World->GetTimerManager().SetTimer(
		SpawnBatch.TimerHandle,
		TimerDelegate,
		TrainingSpawnInterval,
		true,
		TrainingSpawnInterval);
}

void UEnemyStrategicAIComponent::TrainingSpawnBatchTimerTick(const int32 BatchID)
{
	FEnemyStrategicAITrainingSpawnBatch* SpawnBatch = M_ActiveTrainingSpawnBatches.FindByPredicate(
		[BatchID](const FEnemyStrategicAITrainingSpawnBatch& Batch)
		{
			return Batch.BatchID == BatchID;
		});
	if (SpawnBatch == nullptr || not SpawnBatch->HasMoreTrainingOptions())
	{
		StopTrainingSpawnBatchTimer(BatchID);
		return;
	}

	ARTSAsyncSpawner* RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(RTSAsyncSpawner))
	{
		StopTrainingSpawnBatchTimer(BatchID);
		return;
	}

	const FTrainingOption TrainingOption = SpawnBatch->TrainingOptionsToSpawn[SpawnBatch->NextTrainingOptionIndex];
	const int32 SpawnID = SpawnBatch->NextTrainingOptionIndex;
	++SpawnBatch->NextTrainingOptionIndex;

	const FVector TrainingLocation = SpawnBatch->SpawnLocation;
	const TWeakObjectPtr<UEnemyStrategicAIComponent> WeakThis(this);
	const bool bRequestStarted = RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
		TrainingOption,
		TrainingLocation,
		this,
		SpawnID,
		[WeakThis, TrainingLocation](const FTrainingOption& SpawnedTrainingOption, AActor* SpawnedActor, const int32 ID)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->OnBlackboardUnitSpawned(SpawnedTrainingOption, SpawnedActor, ID, TrainingLocation);
		});

	if (not bRequestStarted)
	{
		RTSFunctionLibrary::ReportError(
			"Enemy strategic AI failed to start async spawn for training option: " + TrainingOption.GetTrainingName());
	}

	if (not SpawnBatch->HasMoreTrainingOptions())
	{
		StopTrainingSpawnBatchTimer(BatchID);
	}
}

void UEnemyStrategicAIComponent::StopTrainingSpawnBatchTimer(const int32 BatchID)
{
	const int32 BatchIndex = M_ActiveTrainingSpawnBatches.IndexOfByPredicate(
		[BatchID](const FEnemyStrategicAITrainingSpawnBatch& Batch)
		{
			return Batch.BatchID == BatchID;
		});
	if (not M_ActiveTrainingSpawnBatches.IsValidIndex(BatchIndex))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_ActiveTrainingSpawnBatches[BatchIndex].TimerHandle);
	}

	M_ActiveTrainingSpawnBatches.RemoveAtSwap(BatchIndex);
}

void UEnemyStrategicAIComponent::OnBlackboardUnitSpawned(
	const FTrainingOption& TrainingOption,
	AActor* SpawnedActor,
	const int32 ID,
	const FVector& TrainingLocation)
{
	(void)ID;
	if (not IsValid(SpawnedActor))
	{
		return;
	}

	DebugTrainedUnit(TrainingOption, TrainingLocation);
	if (TrainingOption.UnitType != EAllUnitType::UNType_Squad)
	{
		IssueOrdersToSpawnedBlackboardUnit(SpawnedActor);
		return;
	}

	ASquadController* SquadController = Cast<ASquadController>(SpawnedActor);
	if (not IsValid(SquadController))
	{
		return;
	}

	if (SquadController->GetIsSquadFullyLoadedAndInitialized())
	{
		IssueOrdersToSpawnedBlackboardUnit(SquadController);
		return;
	}

	SquadController->OnSquadFullyLoaded.AddUObject(this, &UEnemyStrategicAIComponent::OnBlackboardSquadFullyLoaded);
}

void UEnemyStrategicAIComponent::OnBlackboardSquadFullyLoaded(ASquadController* SquadController)
{
	if (not IsValid(SquadController))
	{
		return;
	}

	SquadController->OnSquadFullyLoaded.RemoveAll(this);
	IssueOrdersToSpawnedBlackboardUnit(SquadController);
}

void UEnemyStrategicAIComponent::IssueOrdersToSpawnedBlackboardUnit(AActor* SpawnedActor)
{
	ICommands* Commands = Cast<ICommands>(SpawnedActor);
	if (Commands == nullptr)
	{
		return;
	}

	constexpr bool bResetOrderQueue = true;
	if (Commands->RegisterAsBlackboardIdle(bResetOrderQueue) != ECommandQueueError::NoError)
	{
		return;
	}

	FDefensePositions DefensePosition;
	if (not TryGetRandomBlackboardDefensePosition(DefensePosition))
	{
		return;
	}

	constexpr bool bQueueOrder = false;
	Commands->MoveToLocation(
		DefensePosition.Location,
		bQueueOrder,
		FRotator(0.f, DefensePosition.YawRotation, 0.f),
		true);
}

bool UEnemyStrategicAIComponent::TryGetRandomBlackboardDefensePosition(FDefensePositions& OutDefensePosition) const
{
	if (M_Blackboard.CurrentBaseDefensePositions.IsEmpty())
	{
		return false;
	}

	const int32 DefensePositionIndex = GetSeededIndex(M_Blackboard.CurrentBaseDefensePositions.Num());
	if (not M_Blackboard.CurrentBaseDefensePositions.IsValidIndex(DefensePositionIndex))
	{
		return false;
	}

	OutDefensePosition = M_Blackboard.CurrentBaseDefensePositions[DefensePositionIndex];
	return true;
}

void UEnemyStrategicAIComponent::DebugTrainedUnit(
	const FTrainingOption& TrainingOption,
	const FVector& TrainingLocation) const
{
	constexpr float TrainedUnitDebugSphereRadius = 100.f;
	const FString TrainedUnitDebugText = FString::Printf(
		TEXT("trained : %s"),
		*TrainingOption.GetDisplayName());
	DebugPoint(
		TrainingLocation,
		TrainedUnitDebugSphereRadius,
		FColor::Green,
		EnemyAISettings::Debugging::TrainingPressureDebugDuration,
		TrainedUnitDebugText);
}

bool UEnemyStrategicAIComponent::GetValidTrainingSpawnTransform(FTransform& OutTransform)
{
	if (not GetValidTrainerComponentLocationFromBlackboard(OutTransform))
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::TrainingPressureDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"Enemy strategic AI cannot create training batch because it cannot get valid spawn "
				"transform from trainer component on blackboard.", FColor::Red);
		}
		if (not GetRandomEnemyBaseClusterSpawnTransform(OutTransform))
		{
			if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
				EnemyAISettings::Debugging::TrainingPressureDebugging)
			{
				RTSFunctionLibrary::PrintString(
					"Enemy strategic AI cannot create training batch because it cannot get valid spawn "
					"transform from random enemy base cluster either.", FColor::Red);
				return false;
			}
		}
		return true;
	}
	return true;
}

bool UEnemyStrategicAIComponent::GetValidTrainerComponentLocationFromBlackboard(FTransform& OutSpawnTransform) const
{
	OutSpawnTransform = FTransform::Identity;
	for (TWeakObjectPtr<UTrainerComponent> EachEnemyTrainer : M_Blackboard.TrainerComponents)
	{
		if (not EachEnemyTrainer.IsValid())
		{
			continue;
		}
		if (not EachEnemyTrainer->GetSpawnTransform(OutSpawnTransform))
		{
			continue;
		}
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"Enemy strategic AI could not get valid trainer component location from blackboard.");
	return false;
}

bool UEnemyStrategicAIComponent::GetRandomEnemyBaseClusterSpawnTransform(FTransform& OutSpawnTransform) const
{
	if (M_Blackboard.EnemyBasePoints.IsEmpty())
	{
		return false;
	}
	const FEnemyBasePointCoreBuildings RandomBasePoint = M_Blackboard.EnemyBasePoints[FMath::RandRange(
		0, M_Blackboard.EnemyBasePoints.Num() - 1)];
	OutSpawnTransform.SetLocation(RandomBasePoint.BaseLocation);
	return true;
}


void UEnemyStrategicAIComponent::TrainingPressure_ThinkStep()
{
	if (not GetIsAllowedUnitTraining() || not EnsureStochasticDecisionTreeIsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float GameTimeSeconds = World->GetTimeSeconds();
	const TArray<FStrategicAIAction>& ActionDefinitions = M_StochasticDecisionTree->GetActionDefinitions();
	for (const FStrategicAIAction& ActionDefinition : ActionDefinitions)
	{
		for (const TObjectPtr<UStrategicAISubAction>& SubAction : ActionDefinition.GetSubActions())
		{
			AccumulateTrainingPressureFromSubAction(SubAction.Get(), GameTimeSeconds);
		}
	}
}

void UEnemyStrategicAIComponent::AccumulateTrainingPressureFromSubAction(
	const UStrategicAISubAction* SubAction,
	const float GameTimeSeconds)
{
	if (not IsValid(SubAction))
	{
		return;
	}

	TArray<FEnemyStrategicTrainingPressureContribution> PressureContributions;
	SubAction->BuildTrainingPressureContributions(M_Blackboard, GameTimeSeconds, PressureContributions);
	for (const FEnemyStrategicTrainingPressureContribution& PressureContribution : PressureContributions)
	{
		M_TrainingState.AddTrainingPressureContribution(PressureContribution);
	}
}

void UEnemyStrategicAIComponent::TrainingRequirements_ThinkStep()
{
	if (not EnsureIsValidUnitManager())
	{
		return;
	}

	constexpr uint8 EnemyPlayerIndex = 2;
	FEnemyLevelTraining& EnemyLevelTraining = M_TrainingState.EnemyLevelTraining;
	// This accumulates all the buidling types needed for the various tech levels;
	const TArray<EBuildingExpansionType> RequiredBxpTypes = EnemyLevelTraining.GetUniqueBuildingTypesForTechLevels();
	const TMap<EBuildingExpansionType, int32> BxpCountsByType = M_GameUnitManager->GetPlayerBxpCountsByType(
		EnemyPlayerIndex,
		RequiredBxpTypes);

	TMap<EEnemyAITechLevel, bool>& TechLevelUnlockedMap = M_TrainingState.TechLevelUnlockedMap;
	TechLevelUnlockedMap.Empty();
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.BasicInfantryOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.LightTankOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.MediumTankOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.Tier2Options,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.AdvancedInfantryOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.Tier3Options,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.ExperimentalOptions,
		BxpCountsByType);
}

void UEnemyStrategicAIComponent::GetTrainingPoints_ThinkStep()
{
	const int32 MultiplierPerInterval = FMath::Max(
		1, EnemyAISettings::ThinkingTimers::UpdateEnemyTrainingPoints_Interval / 60);
	M_TrainingState.TrainingPoints += M_TrainingState.EnemyLevelTraining.TrainingPointsPerMinute *
		MultiplierPerInterval;
}

bool UEnemyStrategicAIComponent::EnsureEnemyControllerIsValid() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_EnemyController",
		"EnsureEnemyControllerIsValid",
		this);

	return false;
}


bool UEnemyStrategicAIComponent::EnsureStochasticDecisionTreeIsValid() const
{
	if (M_StochasticDecisionTree.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_StochasticDecisionTree",
		"EnsureStochasticDecisionTreeIsValid",
		this);

	return false;
}

void UEnemyStrategicAIComponent::CacheGenerationSeedFromGameInstance()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UGameInstance* const GameInstance = World->GetGameInstance();
	URTSGameInstance* const RTSGameInstance = Cast<URTSGameInstance>(GameInstance);
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError("Enemy strategic AI component could not cache generation seed.");
		return;
	}

	const FCampaignGenerationSettings CampaignGenerationSettings = RTSGameInstance->GetCampaignGenerationSettings();
	M_CachedGenerationSeed = CampaignGenerationSettings.GenerationSeed;
}

int32 UEnemyStrategicAIComponent::GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt) const
{
	if (OptionCount <= 0)
	{
		return INDEX_NONE;
	}

	const uint32 CombinedSeed = static_cast<uint32>(M_CachedGenerationSeed)
		+ static_cast<uint32>(DecisionSalt)
		+ static_cast<uint32>(M_SeedDecisionCounter);
	FRandomStream SeededRandomStream(static_cast<int32>(CombinedSeed));
	++M_SeedDecisionCounter;
	return SeededRandomStream.RandRange(0, OptionCount - 1);
}

void UEnemyStrategicAIComponent::StartStrategicAIThinkingTimer()
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
		M_StrategicAIThinkingTimerHandle,
		this,
		&UEnemyStrategicAIComponent::StrategicAiThinkingLoop,
		EnemyAISettings::EnemyStrategicAIThinkingSpeed,
		true);
}


void UEnemyStrategicAIComponent::StrategicAiThinkingLoop()
{
	const float Now = GetWorld()->GetTimeSeconds();
	ClearInvalidIdleUnitsFromBlackboard();
	for (auto EachThinkTimer : M_AIThinkTimers)
	{
		if (not EachThinkTimer || not EachThinkTimer->ThinkStepDelegate.IsBound())
		{
			continue;
		}
		// Sees if enough time has passed since last think step then calls delegate.
		EachThinkTimer->TryExecuteThinkStep(Now);
	}
	if (GetIsAllowedDirectControlUnits() && EnsureStochasticDecisionTreeIsValid())
	{
		M_StochasticDecisionTree->DecisionTree_ThinkStep(Now, M_Blackboard);
	}
	if (GetIsAllowedUnitTraining())
	{
		Training_ThinkStep();
	}
	ProcessStrategicAIRequests();
}

void UEnemyStrategicAIComponent::StopStrategicAIThinkingTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_StrategicAIThinkingTimerHandle);
	}
}

void UEnemyStrategicAIComponent::ProcessStrategicAIRequests()
{
	if (M_PendingRequests.FindClosestFlankableEnemyHeavyRequests.IsEmpty()
		&& M_PendingRequests.GetPlayerUnitCountsAndBaseRequests.IsEmpty()
		&& M_PendingRequests.FindAlliedTanksToRetreatRequests.IsEmpty()
		&& M_PendingRequests.FindEnemyBaseClustersRequests.IsEmpty()
		&& M_PendingRequests.FindLocationsUnderPlayerAttackRequests.IsEmpty()
		&& M_PendingRequests.FindPlayerUnitBulkLocationsRequests.IsEmpty()
		&& M_PendingRequests.FindConstructionLocationsRequests.IsEmpty())
	{
		return;
	}

	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("Enemy strategic AI requires a valid game unit manager.");
		return;
	}

	FStrategicAIRequestBatch RequestsToSend = MoveTemp(M_PendingRequests);
	M_PendingRequests = FStrategicAIRequestBatch();

	TWeakObjectPtr<UEnemyStrategicAIComponent> WeakThis(this);
	GameUnitManager->RequestStrategicAIRequests(
		RequestsToSend,
		[WeakThis](const FStrategicAIResultBatch& ResultBatch)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnStrategicAIResultsReceived(ResultBatch);
			}
		});
}

void UEnemyStrategicAIComponent::OnStrategicAIResultsReceived(const FStrategicAIResultBatch& ResultBatch)
{
	M_LatestResults = ResultBatch;
	ProcessEnemyBaseClusterResults(ResultBatch.EnemyBaseClustersResults);
	ProcessPlayerUnitCountsAndBaseResults(ResultBatch.PlayerUnitCountsResults);
	// IMPORTANT; after unit counts to quickly invalidate old flanking data.
	ProcessClosestFlankableEnemyHeavyResults(ResultBatch.FindClosestFlankableEnemyHeavyResults);
	ProcessAlliedTanksToRetreatResults(ResultBatch.AlliedTanksToRetreatResults);
	ProcessLocationsUnderPlayerAttackResults(ResultBatch.LocationsUnderPlayerAttackResults);
	ProcessPlayerUnitBulkLocationsResults(ResultBatch.PlayerUnitBulkLocationsResults);
	ProcessConstructionLocationsResults(ResultBatch.ConstructionLocationsResults);
}

void UEnemyStrategicAIComponent::ProcessClosestFlankableEnemyHeavyResults(
	const TArray<FResultClosestFlankableEnemyHeavy>& ClosestFlankableEnemyHeavyResults)
{
	if (M_Blackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks <= 0)
	{
		// Disregard data and clear all older flanking positions.
		M_Blackboard.AgreggatedHeavyTankFlankingResults.Empty();
		return;
	}

	if (ClosestFlankableEnemyHeavyResults.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	M_Blackboard.AgreggatedHeavyTankFlankingResults.Reset(ClosestFlankableEnemyHeavyResults.Num());
	for (const FResultClosestFlankableEnemyHeavy& Result : ClosestFlankableEnemyHeavyResults)
	{
		FResultClosestFlankableEnemyHeavy ResultWithTimestamp = Result;
		ResultWithTimestamp.ResultTimestampSeconds = CurrentTimeSeconds;
		M_Blackboard.AgreggatedHeavyTankFlankingResults.Add(MoveTemp(ResultWithTimestamp));
	}
	if (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::FlankPositionsDebugging)
	{
		DebugFlankingPositiions();
	}
}

void UEnemyStrategicAIComponent::ProcessEnemyBaseClusterResults(
	const TArray<FResultEnemyBaseClusters>& EnemyBaseClusterResults)
{
	if (EnemyBaseClusterResults.IsEmpty())
	{
		return;
	}

	for (const FResultEnemyBaseClusters& BaseClusterResult : EnemyBaseClusterResults)
	{
		M_Blackboard.EnemyBasePoints = BaseClusterResult.BasePoints;
		M_Blackboard.CurrentBaseDefensePositions = BaseClusterResult.DefensePointCandidates;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::BaseLocationDebugging)
	{
		DebugBlackboardBasePoints();
	}
}

void UEnemyStrategicAIComponent::ProcessAlliedTanksToRetreatResults(
	const TArray<FResultAlliedTanksToRetreat>& AlliedTanksToRetreatResults)
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	if (AlliedTanksToRetreatResults.IsEmpty())
	{
		return;
	}

	UEnemyDirectControlComponent* EnemyDirectControlComponent = M_EnemyController->GetEnemyDirectControlComponent();
	if (not GetIsValidEnemyDirectControlComponent(EnemyDirectControlComponent))
	{
		return;
	}

	for (const FResultAlliedTanksToRetreat& RetreatResult : AlliedTanksToRetreatResults)
	{
		EnemyDirectControlComponent->HandleAsyncRetreatGroupResult(RetreatResult);
	}
}

void UEnemyStrategicAIComponent::ProcessPlayerUnitCountsAndBaseResults(
	const TArray<FResultPlayerUnitCounts>& PlayerUnitCountsAndBaseResults)
{
	if (PlayerUnitCountsAndBaseResults.IsEmpty())
	{
		return;
	}
	// Always get the last entry; most up to date.
	M_Blackboard.CurrentPlayerUnitCounts = PlayerUnitCountsAndBaseResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::PlayerCountsDebugging)
	{
		DebugBlackboardUnitCounts();
	}
}

void UEnemyStrategicAIComponent::ProcessLocationsUnderPlayerAttackResults(
	const TArray<FResultLocationsUnderPlayerAttack>& LocationsUnderPlayerAttackResults)
{
	if (LocationsUnderPlayerAttackResults.IsEmpty())
	{
		return;
	}
	M_Blackboard.CurrentLocationsUnderPlayerAttack = LocationsUnderPlayerAttackResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::LocationsUnderPlayerAttackDebugging)
	{
		DebugBlackboardLocationsUnderAttack();
	}
}

void UEnemyStrategicAIComponent::ProcessPlayerUnitBulkLocationsResults(
	const TArray<FResultPlayerUnitBulkLocations>& PlayerUnitBulkLocationsResults)
{
	if (PlayerUnitBulkLocationsResults.IsEmpty())
	{
		return;
	}
	M_Blackboard.CurrentPlayerUnitBulkLocations = PlayerUnitBulkLocationsResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::PlayerUnitBulkDebugging)
	{
		DebugBlackboardBulkPlayerUnits();
	}
}

void UEnemyStrategicAIComponent::ProcessConstructionLocationsResults(
	const TArray<FResultConstructionLocations>& ConstructionLocationsResults)
{
	if (ConstructionLocationsResults.IsEmpty())
	{
		return;
	}

	M_Blackboard.CurrentConstructionLocations = ConstructionLocationsResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::ConstructionLocationsDebugging)
	{
		DebugConstructionLocations();
	}
}

bool UEnemyStrategicAIComponent::GetIsValidEnemyDirectControlComponent(
	UEnemyDirectControlComponent* EnemyDirectControlComponent) const
{
	if (IsValid(EnemyDirectControlComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"EnemyDirectControlComponent",
		"GetIsValidEnemyDirectControlComponent",
		this);

	return false;
}

void UEnemyStrategicAIComponent::FillRetreatRequestExcludedUnits(FFindAlliedTanksToRetreat& RequestToFill) const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UEnemyDirectControlComponent* EnemyDirectControlComponent = M_EnemyController->GetEnemyDirectControlComponent();
	if (not GetIsValidEnemyDirectControlComponent(EnemyDirectControlComponent))
	{
		return;
	}

	RequestToFill.ExcludedRetreatUnitActors = EnemyDirectControlComponent->GetRetreatGroupUnitsToExclude();
}

void UEnemyStrategicAIComponent::ClearInvalidIdleUnitsFromBlackboard()
{
	TArray<FBlackboardIdleUnitEntry> ValidIdleUnits;
	bool bWasAnyUnitInvalid = false;
	for (auto EachIdleUnit : M_Blackboard.IdleDirectControlUnits)
	{
		if (RTSFunctionLibrary::RTSIsValidWeak(EachIdleUnit.IdleUnit))
		{
			ValidIdleUnits.Add(EachIdleUnit);
		}
		else
		{
			bWasAnyUnitInvalid = true;
		}
	}
	if (bWasAnyUnitInvalid)
	{
		M_Blackboard.IdleDirectControlUnits = ValidIdleUnits;
	}
}

void UEnemyStrategicAIComponent::DebugBlackboardBasePoints() const
{
	using namespace EnemyAISettings::Debugging;
	for (const FEnemyBasePointCoreBuildings& BasePoint : M_Blackboard.EnemyBasePoints)
	{
		DebugPoint(BasePoint.BaseLocation, BaseLocationDebuggingRadius, EnemyLocationColor, BaseLocationDebugDuration,
		           "Enemy Base Point");
	}
}

void UEnemyStrategicAIComponent::DebugBlackboardLocationsUnderAttack() const
{
	using namespace EnemyAISettings::Debugging;
	for (const auto& EachUnderAttackLocation : M_Blackboard.CurrentLocationsUnderPlayerAttack.
	                                                        LocationsUnderAttack)
	{
		DebugPoint(EachUnderAttackLocation.LocationUnderAttack, LocationUnderAttackDebuggingRadius, AttackLocationColor,
		           LocationsUnderAttackDuration, "Location Under Player Attack");
		DebugPoint(EachUnderAttackLocation.AverageAttackerLocation, LocationUnderAttackDebuggingRadius,
		           PlayerLocationColor, LocationsUnderAttackDuration, "avg Loc: attack location Units");
	}
}

void UEnemyStrategicAIComponent::DebugBlackboardBulkPlayerUnits() const
{
	using namespace EnemyAISettings::Debugging;
	for (const auto& EachBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
	{
		FString DebugText = FString::Printf(TEXT("Player Unit Bulk: %d units"), EachBulk.UnitsInBulk.Num());
		DebugPoint(EachBulk.BulkLocation, PlayerUnitBulkLocationDebuggingRadius, PlayerLocationColor,
		           PlayerUnitBulkLocationDebugDuration, DebugText);
	}
}

void UEnemyStrategicAIComponent::DebugConstructionLocations() const
{
	using namespace EnemyAISettings::Debugging;
	for (const FVector& ConstructionLocation : M_Blackboard.CurrentConstructionLocations.ConstructionLocations)
	{
		DebugPoint(
			ConstructionLocation,
			ConstructionLocationDebuggingRadius,
			ConstructionLocationColor,
			ConstructionLocationDebugDuration,
			TEXT("Construction Location"));
	}
}

void UEnemyStrategicAIComponent::DebugPoint(const FVector& Point, const float Radius,
                                            const FColor& Color, const float Duration, const FString& Text) const
{
	DrawDebugSphere(GetWorld(), Point, Radius, 20,
	                Color, false, Duration, 0, 5.f);
	DrawDebugString(
		GetWorld(), Point + FVector(0, 0, Radius),
		Text, nullptr, Color, Duration,
		false);
}

void UEnemyStrategicAIComponent::DebugBlackboardUnitCounts() const
{
	FString DebugString = "\n-------- Player Counts --------";
	DebugString += FString::Printf(
		TEXT("\n Armored Cars: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerArmoredCars);
	DebugString += FString::Printf(
		TEXT("\n Light Tanks: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerLightTanks);
	DebugString += FString::Printf(
		TEXT("\n Medium Tanks: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerMediumTanks);
	DebugString += FString::Printf(
		TEXT("\n Heavy Tanks: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks);
	DebugString += FString::Printf(
		TEXT("\n Squads: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerSquads);
	DebugString += FString::Printf(
		TEXT("\n Nomads: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerNomadicVehicles);
	const bool bFoundHQ = M_Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation != FVector::ZeroVector;
	DebugString += FString::Printf(
		TEXT("\n Found Player HQ: %d"), bFoundHQ ? 1 : 0);
	DebugString += FString::Printf(
		TEXT("\n Resource Buildings: %d"),
		M_Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings.Num());
	RTSFunctionLibrary::PrintString(
		DebugString, FColor::Green, EnemyAISettings::Debugging::PlayerCountsDuration);
}

void UEnemyStrategicAIComponent::DebugFlankingPositiions() const
{
	for (auto EachFlank : M_Blackboard.AgreggatedHeavyTankFlankingResults)
	{
		for (const auto& EachHeavyLocations : EachFlank.FlankLocationsAroundHeavyTank)
			for (const auto& EachLocation : EachHeavyLocations.Locations)
			{
				DrawDebugSphere(GetWorld(), EachLocation, 33.f, 20,
				                EnemyAISettings::Debugging::FlankLocationColor, false, 2.f, 0, 5.f);
			}
	}
}
