// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnDirector.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnPlanning.h"
#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnSettings.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldData/WorldDataComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace WorldEnemyTurnDirectorPrivate
{
	FVector2D GetXY(const FVector& Location)
	{
		return FVector2D(Location.X, Location.Y);
	}

	UWorldFortificationModificationsComponent* GetFortificationComponent(const AWorldMapObject& MapObject)
	{
		if (const AWorldEnemyObject* EnemyObject = Cast<AWorldEnemyObject>(&MapObject))
		{
			return EnemyObject->GetFortificationModificationsComponent();
		}
		if (const AWorldMissionObject* MissionObject = Cast<AWorldMissionObject>(&MapObject))
		{
			return MissionObject->GetFortificationModificationsComponent();
		}
		return nullptr;
	}

	int32 GetReactionCost(const EWorldFieldDivisions PieceType, const FWorldCampaignRuleNumbers& RuleNumbers)
	{
		return PieceType == EWorldFieldDivisions::SovietSupplyConvoy
			       ? RuleNumbers.ConvoyReactionCost
			       : RuleNumbers.DivisionReactionCost;
	}
}

UWorldEnemyTurnDirector::UWorldEnemyTurnDirector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldEnemyTurnDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorldDivisionManager* DivisionManager = M_DivisionManager.Get())
	{
		DivisionManager->OnDivisionRemoved().RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UWorldEnemyTurnDirector::InitializeEnemyTurnDirector(AWorldPlayerController* WorldPlayerController)
{
	if (not IsValid(WorldPlayerController))
	{
		return;
	}

	M_WorldPlayerController = WorldPlayerController;
	M_ProgressManager = WorldPlayerController->FindComponentByClass<UWorldCampaignProgressManager>();
	M_WorldGenerator = WorldPlayerController->GetWorldGenerator();
	if (UWorldDivisionManager* PreviousDivisionManager = M_DivisionManager.Get())
	{
		PreviousDivisionManager->OnDivisionRemoved().RemoveAll(this);
	}

	M_DivisionManager = WorldPlayerController->GetWorldDivisionManager();
	if (UWorldDivisionManager* DivisionManager = M_DivisionManager.Get())
	{
		DivisionManager->OnDivisionRemoved().AddUObject(this, &UWorldEnemyTurnDirector::OnDivisionRemoved);
	}
}

void UWorldEnemyTurnDirector::PlayEnemyTurn(const ERTSGameDifficulty GameDifficulty,
                                            const FSimpleDelegate& OnFinished)
{
	if (bM_IsEnemyTurnInProgress)
	{
		RTSFunctionLibrary::ReportError(TEXT("PlayEnemyTurn called while an enemy turn is still playing; ignored."));
		return;
	}

	FWorldEnemyTurnState* EnemyTurnState = EnsureInitialized() ? GetMutableEnemyTurnState() : nullptr;
	if (EnemyTurnState == nullptr)
	{
		OnFinished.ExecuteIfBound();
		return;
	}

	bM_IsEnemyTurnInProgress = true;
	M_OnTurnFinished = OnFinished;
	M_ActiveDifficulty = GameDifficulty;
	M_ActiveTurnSeed = WorldEnemyTurnPlanning::BuildEnemyTurnSeed(GetCampaignSeed(), EnemyTurnState->EnemyTurnsPlayed);
	EnemyTurnState->LastTurnConvoysSpawned = 0;
	EnemyTurnState->LastTurnDivisionsSpawned = 0;
	EnemyTurnState->LastTurnConvoysDelivered = 0;
	EnemyTurnState->LastTurnFreezeStagesGained = 0;

	UpdateAlertForTurn(*EnemyTurnState);
	const FWorldEnemyBudgetSplit BudgetSplit = WorldCampaignRules::SplitEnemyBudget(
		UWorldCampaignRulesSettings::GetNumbers(), EnemyTurnState->Alert);
	EnemyTurnState->LastTurnReactionPoints = BudgetSplit.ReactionPoints;
	EnemyTurnState->LastTurnFreezePoints = BudgetSplit.FreezePoints;

	RefreshExistingReactionOrders(*EnemyTurnState);
	SpendReactionPoints(*EnemyTurnState, BudgetSplit.ReactionPoints);
	PayFreeze(*EnemyTurnState, BudgetSplit.FreezePoints);
	StartEnemyMovement();
}

bool UWorldEnemyTurnDirector::RemoveEnemyPieceDefeatedByPlayer(const FGuid& DivisionKey)
{
	if (not EnsureInitialized())
	{
		return false;
	}

	const AWorldDivisionBase* DefeatedPiece = M_DivisionManager->FindDivisionByKey(DivisionKey);
	FWorldEnemyTurnState* EnemyTurnState = GetMutableEnemyTurnState();
	if (not IsValid(DefeatedPiece) || EnemyTurnState == nullptr)
	{
		return false;
	}

	EnemyTurnState->RecentlyStruckLocations.Add(DefeatedPiece->GetActorLocation());
	return M_DivisionManager->RemoveDivisionByKey(DivisionKey);
}

FWorldEnemyTurnSummary UWorldEnemyTurnDirector::GetLastEnemyTurnSummary() const
{
	FWorldEnemyTurnSummary Summary;
	// Quiet check on purpose: the UI may ask for the summary before the director was initialized.
	const UWorldCampaignProgressManager* ProgressManager = M_ProgressManager.Get();
	if (not IsValid(ProgressManager))
	{
		return Summary;
	}

	const FWorldEnemyTurnState& EnemyTurnState = ProgressManager->GetProgressState().EnemyTurn;
	Summary.EnemyTurnNumber = EnemyTurnState.EnemyTurnsPlayed;
	Summary.Alert = EnemyTurnState.Alert;
	Summary.PointsToReaction = EnemyTurnState.LastTurnReactionPoints;
	Summary.PointsToFreeze = EnemyTurnState.LastTurnFreezePoints;
	Summary.PointsBanked = EnemyTurnState.BankedReactionPoints;
	Summary.ConvoysSpawned = EnemyTurnState.LastTurnConvoysSpawned;
	Summary.DivisionsSpawned = EnemyTurnState.LastTurnDivisionsSpawned;
	Summary.PiecesSpawned = Summary.ConvoysSpawned + Summary.DivisionsSpawned;
	Summary.ConvoysDelivered = EnemyTurnState.LastTurnConvoysDelivered;
	Summary.FreezeStage = EnemyTurnState.FreezeStage;
	Summary.FreezeStagesGained = EnemyTurnState.LastTurnFreezeStagesGained;
	Summary.bIsFreezeComplete = WorldCampaignRules::GetIsFreezeComplete(UWorldCampaignRulesSettings::GetNumbers(),
	                                                                    EnemyTurnState.FreezeStage);
	return Summary;
}

bool UWorldEnemyTurnDirector::EnsureInitialized()
{
	if (not M_ProgressManager.IsValid() || not M_DivisionManager.IsValid() || not M_WorldGenerator.IsValid())
	{
		InitializeEnemyTurnDirector(Cast<AWorldPlayerController>(GetOwner()));
	}

	return GetIsValidWorldPlayerController()
		&& GetIsValidProgressManager()
		&& GetIsValidDivisionManager()
		&& GetIsValidWorldGenerator();
}

FWorldEnemyTurnState* UWorldEnemyTurnDirector::GetMutableEnemyTurnState() const
{
	UWorldCampaignProgressManager* ProgressManager = M_ProgressManager.Get();
	if (not IsValid(ProgressManager))
	{
		return nullptr;
	}
	return &ProgressManager->GetMutableProgressState().EnemyTurn;
}

int32 UWorldEnemyTurnDirector::GetCampaignSeed() const
{
	const AWorldPlayerController* WorldPlayerController = M_WorldPlayerController.Get();
	const UWorldStateAndSaveManager* SaveManager = IsValid(WorldPlayerController)
		                                               ? WorldPlayerController->GetWorldStateAndSaveManager()
		                                               : nullptr;
	return IsValid(SaveManager) ? SaveManager->GetWorldGenerationSeed() : 0;
}

void UWorldEnemyTurnDirector::UpdateAlertForTurn(FWorldEnemyTurnState& EnemyTurnState)
{
	const int32 MissionsCompleted = M_ProgressManager->GetProgressState().MissionsCompleted;
	const bool bPlayerStruck = WorldEnemyTurnPlanning::GetHasPlayerStruckSinceLastEnemyTurn(
		EnemyTurnState, MissionsCompleted);
	const float AlertBefore = EnemyTurnState.Alert;
	if (not bPlayerStruck)
	{
		EnemyTurnState.Alert = WorldCampaignRules::GetAlertAfterQuietTurn(UWorldCampaignRulesSettings::GetNumbers(),
		                                                                 AlertBefore);
	}

	// A strike raised the alert when its result came in; announce the value this turn plays with.
	if (bPlayerStruck || not FMath::IsNearlyEqual(AlertBefore, EnemyTurnState.Alert))
	{
		OnAlertChanged.Broadcast(EnemyTurnState.Alert);
	}
}

void UWorldEnemyTurnDirector::RefreshExistingReactionOrders(FWorldEnemyTurnState& EnemyTurnState)
{
	const TArray<FWorldEnemyTurnPlace> TargetCandidates = BuildTargetCandidates();
	TArray<FGuid> PieceKeys;
	EnemyTurnState.ReactionTargetAnchorByPieceKey.GetKeys(PieceKeys);
	// Sorted so retargeting order never depends on map storage order.
	PieceKeys.Sort();
	for (const FGuid& PieceKey : PieceKeys)
	{
		RefreshReactionOrder(EnemyTurnState, PieceKey, TargetCandidates);
	}
}

void UWorldEnemyTurnDirector::RefreshReactionOrder(FWorldEnemyTurnState& EnemyTurnState, const FGuid& PieceKey,
                                                   const TArray<FWorldEnemyTurnPlace>& TargetCandidates)
{
	AWorldDivisionBase* Piece = M_DivisionManager->FindDivisionByKey(PieceKey);
	if (not IsValid(Piece))
	{
		EnemyTurnState.ReactionTargetAnchorByPieceKey.Remove(PieceKey);
		return;
	}

	FGuid TargetAnchorKey = EnemyTurnState.ReactionTargetAnchorByPieceKey.FindRef(PieceKey);
	if (GetIsAnchorCleared(TargetAnchorKey) || not IsValid(FindMapObjectByAnchorKey(TargetAnchorKey)))
	{
		const int32 NewTargetIndex = WorldEnemyTurnPlanning::SelectNearestPlaceIndex(
			WorldEnemyTurnDirectorPrivate::GetXY(Piece->GetActorLocation()), TargetCandidates);
		if (not TargetCandidates.IsValidIndex(NewTargetIndex))
		{
			EnemyTurnState.ReactionTargetAnchorByPieceKey.Remove(PieceKey);
			return;
		}

		TargetAnchorKey = TargetCandidates[NewTargetIndex].Key;
		EnemyTurnState.ReactionTargetAnchorByPieceKey.Add(PieceKey, TargetAnchorKey);
	}

	const AAnchorPoint* TargetAnchor = FindAnchorByKey(TargetAnchorKey);
	if (not IsValid(TargetAnchor) || Piece->GetHasPendingMoveOrder())
	{
		return;
	}

	const float ArrivalRadius = UWorldEnemyTurnSettings::GetNumbers().ArrivalRadius;
	const double DistanceSquared = FVector2D::DistSquared(
		WorldEnemyTurnDirectorPrivate::GetXY(Piece->GetActorLocation()),
		WorldEnemyTurnDirectorPrivate::GetXY(TargetAnchor->GetActorLocation()));
	if (DistanceSquared > FMath::Square(ArrivalRadius))
	{
		(void)Piece->IssueMoveOrderToPoint(TargetAnchor->GetActorLocation());
	}
}

void UWorldEnemyTurnDirector::SpendReactionPoints(FWorldEnemyTurnState& EnemyTurnState, const int32 ReactionPoints)
{
	const FWorldCampaignRuleNumbers& RuleNumbers = UWorldCampaignRulesSettings::GetNumbers();
	const FWorldEnemyTurnNumbers& EnemyTurnNumbers = UWorldEnemyTurnSettings::GetNumbers();
	const int32 AvailablePoints = FMath::Max(0, ReactionPoints) + FMath::Max(0, EnemyTurnState.BankedReactionPoints);
	const FWorldEnemyReactionPurchase Purchase = WorldEnemyTurnPlanning::PlanReactionPurchase(
		AvailablePoints, RuleNumbers.ConvoyReactionCost, RuleNumbers.DivisionReactionCost,
		EnemyTurnNumbers.MaxReactionPiecesPerTurn);
	const TArray<EWorldFieldDivisions> PieceTypes = WorldEnemyTurnPlanning::PlanPieceTypes(
		Purchase, EnemyTurnNumbers.ReactionDivisionTypes, M_ActiveTurnSeed);

	const TArray<FWorldEnemyTurnPlace> TargetCandidates = BuildTargetCandidates();
	const TArray<int32> TargetIndices = WorldEnemyTurnPlanning::SelectReactionTargetIndices(
		BuildStruckLocations(EnemyTurnState), TargetCandidates, EnemyTurnNumbers.MaxReactionTargetsPerTurn);
	const TArray<FWorldEnemyTurnPlace> Sources = BuildSourceCandidates();

	// Points of pieces that could not be placed go back to the bank instead of being lost.
	// Divisions dropped because no division type is configured are refunded too.
	const int32 DroppedDivisionCount = FMath::Max(0, Purchase.GetPieceCount() - PieceTypes.Num());
	int32 UnspentPoints = Purchase.PointsLeft + DroppedDivisionCount * FMath::Max(1, RuleNumbers.DivisionReactionCost);
	TMap<FGuid, int32> SpawnCountBySource;
	for (int32 PieceIndex = 0; PieceIndex < PieceTypes.Num(); PieceIndex++)
	{
		const int32 TargetSlot = WorldEnemyTurnPlanning::GetTargetIndexForPiece(PieceIndex, TargetIndices.Num());
		const bool bWasSent = TargetIndices.IsValidIndex(TargetSlot)
			&& TrySendReactionPiece(EnemyTurnState, PieceTypes[PieceIndex], TargetCandidates[TargetIndices[TargetSlot]],
			                        PieceIndex, Sources, SpawnCountBySource);
		if (not bWasSent)
		{
			UnspentPoints += WorldEnemyTurnDirectorPrivate::GetReactionCost(PieceTypes[PieceIndex], RuleNumbers);
		}
	}

	EnemyTurnState.BankedReactionPoints = FMath::Clamp(UnspentPoints, 0,
	                                                   FMath::Max(0, EnemyTurnNumbers.MaxBankedReactionPoints));
}

bool UWorldEnemyTurnDirector::TrySendReactionPiece(FWorldEnemyTurnState& EnemyTurnState,
                                                   const EWorldFieldDivisions PieceType,
                                                   const FWorldEnemyTurnPlace& Target, const int32 PieceIndex,
                                                   const TArray<FWorldEnemyTurnPlace>& Sources,
                                                   TMap<FGuid, int32>& InOutSpawnCountBySource)
{
	const int32 SourceIndex = WorldEnemyTurnPlanning::SelectNearestPlaceIndex(Target.Location, Sources);
	const AAnchorPoint* SourceAnchor = Sources.IsValidIndex(SourceIndex) ? FindAnchorByKey(Sources[SourceIndex].Key)
		                                   : nullptr;
	const AAnchorPoint* TargetAnchor = FindAnchorByKey(Target.Key);
	const FGuid PieceKey = WorldEnemyTurnPlanning::BuildReactionPieceKey(
		GetCampaignSeed(), EnemyTurnState.EnemyTurnsPlayed, PieceIndex);
	if (not IsValid(SourceAnchor) || not IsValid(TargetAnchor) || IsValid(M_DivisionManager->FindDivisionByKey(PieceKey)))
	{
		return false;
	}

	int32& SpawnCountAtSource = InOutSpawnCountBySource.FindOrAdd(Sources[SourceIndex].Key);
	const FVector2D SpawnDirection = WorldEnemyTurnPlanning::BuildSpawnDirection(
		Sources[SourceIndex].Location, Target.Location, SpawnCountAtSource);
	SpawnCountAtSource++;
	const FVector2D SpawnXY = Sources[SourceIndex].Location
		+ SpawnDirection * UWorldEnemyTurnSettings::GetNumbers().SpawnDistanceFromSource;
	const FVector SpawnLocation(SpawnXY.X, SpawnXY.Y, SourceAnchor->GetActorLocation().Z);

	AWorldDivisionBase* Piece = M_DivisionManager->SpawnEnemyReactionPiece(
		PieceType, SpawnLocation, PieceKey, M_ActiveDifficulty);
	if (not IsValid(Piece))
	{
		return false;
	}

	if (not Piece->IssueMoveOrderToPoint(TargetAnchor->GetActorLocation()))
	{
		(void)M_DivisionManager->RemoveDivisionByKey(PieceKey);
		return false;
	}

	EnemyTurnState.ReactionTargetAnchorByPieceKey.Add(PieceKey, Target.Key);
	if (Piece->GetIsSupplyConvoy())
	{
		EnemyTurnState.LastTurnConvoysSpawned++;
	}
	else
	{
		EnemyTurnState.LastTurnDivisionsSpawned++;
	}

	OnEnemyReactionSpawned.Broadcast(Piece);
	return true;
}

void UWorldEnemyTurnDirector::PayFreeze(FWorldEnemyTurnState& EnemyTurnState, const int32 FreezePoints)
{
	const FWorldCampaignRuleNumbers& RuleNumbers = UWorldCampaignRulesSettings::GetNumbers();
	const bool bWasComplete = WorldCampaignRules::GetIsFreezeComplete(RuleNumbers, EnemyTurnState.FreezeStage);
	const int32 StageBefore = EnemyTurnState.FreezeStage;
	const FWorldFreezeAdvance FreezeAdvance = WorldCampaignRules::AdvanceFreeze(
		RuleNumbers, EnemyTurnState.FreezeProgressPoints, EnemyTurnState.FreezeStage, FreezePoints);
	EnemyTurnState.FreezeProgressPoints = FreezeAdvance.NewProgressPoints;
	EnemyTurnState.FreezeStage = FreezeAdvance.NewStage;
	EnemyTurnState.LastTurnFreezeStagesGained = FreezeAdvance.StagesGained;

	for (int32 ReachedStage = StageBefore + 1; ReachedStage <= FreezeAdvance.NewStage; ReachedStage++)
	{
		OnFreezeStageReached.Broadcast(ReachedStage);
	}

	const bool bIsComplete = WorldCampaignRules::GetIsFreezeComplete(RuleNumbers, FreezeAdvance.NewStage);
	const int32 PointsNeeded = bIsComplete
		                           ? 0
		                           : WorldCampaignRules::GetFreezePointsNeededForStage(
			                           RuleNumbers, FreezeAdvance.NewStage);
	OnFreezeProgress.Broadcast(FreezeAdvance.NewStage, FreezeAdvance.NewProgressPoints, PointsNeeded);
	if (bIsComplete && not bWasComplete)
	{
		OnFreezeComplete.Broadcast();
	}
}

void UWorldEnemyTurnDirector::StartEnemyMovement()
{
	if (not GetIsValidDivisionManager())
	{
		OnEnemyMovementFinished();
		return;
	}

	M_DivisionManager->MoveEnemyDivisionsThenNotify(
		M_ActiveDifficulty,
		FSimpleDelegate::CreateUObject(this, &UWorldEnemyTurnDirector::OnEnemyMovementFinished));
}

void UWorldEnemyTurnDirector::OnEnemyMovementFinished()
{
	FWorldEnemyTurnState* EnemyTurnState = GetMutableEnemyTurnState();
	if (EnemyTurnState == nullptr || not GetIsValidDivisionManager())
	{
		bM_IsEnemyTurnInProgress = false;
		const FSimpleDelegate OnTurnFinished = M_OnTurnFinished;
		M_OnTurnFinished.Unbind();
		OnTurnFinished.ExecuteIfBound();
		return;
	}

	ResolveConvoyArrivals(*EnemyTurnState);
	FinishEnemyTurn(*EnemyTurnState);
}

void UWorldEnemyTurnDirector::ResolveConvoyArrivals(FWorldEnemyTurnState& EnemyTurnState)
{
	const float ArrivalRadiusSquared = FMath::Square(UWorldEnemyTurnSettings::GetNumbers().ArrivalRadius);
	TArray<FGuid> PieceKeys;
	EnemyTurnState.ReactionTargetAnchorByPieceKey.GetKeys(PieceKeys);
	PieceKeys.Sort();

	// Collect first: delivering removes pieces, which changes the division list and the order map.
	TArray<FGuid> ArrivedConvoyKeys;
	for (const FGuid& PieceKey : PieceKeys)
	{
		const AWorldDivisionBase* Piece = M_DivisionManager->FindDivisionByKey(PieceKey);
		const AAnchorPoint* TargetAnchor = FindAnchorByKey(EnemyTurnState.ReactionTargetAnchorByPieceKey.FindRef(PieceKey));
		if (not IsValid(Piece) || not IsValid(TargetAnchor) || not Piece->GetIsSupplyConvoy()
			|| Piece->GetHasPendingMoveOrder())
		{
			continue;
		}

		const double DistanceSquared = FVector2D::DistSquared(
			WorldEnemyTurnDirectorPrivate::GetXY(Piece->GetActorLocation()),
			WorldEnemyTurnDirectorPrivate::GetXY(TargetAnchor->GetActorLocation()));
		if (DistanceSquared <= ArrivalRadiusSquared)
		{
			ArrivedConvoyKeys.Add(PieceKey);
		}
	}

	for (const FGuid& ConvoyKey : ArrivedConvoyKeys)
	{
		const FGuid TargetAnchorKey = EnemyTurnState.ReactionTargetAnchorByPieceKey.FindRef(ConvoyKey);
		AWorldMapObject* TargetObject = FindMapObjectByAnchorKey(TargetAnchorKey);
		if (IsValid(TargetObject) && not GetIsAnchorCleared(TargetAnchorKey))
		{
			const EWorldFortificationStrength AddedFortification = ApplyConvoyDelivery(*TargetObject);
			EnemyTurnState.LastTurnConvoysDelivered++;
			OnConvoyDelivered.Broadcast(TargetAnchorKey, AddedFortification);
		}

		EnemyTurnState.ReactionTargetAnchorByPieceKey.Remove(ConvoyKey);
		(void)M_DivisionManager->RemoveDivisionByKey(ConvoyKey);
	}
}

EWorldFortificationStrength UWorldEnemyTurnDirector::ApplyConvoyDelivery(AWorldMapObject& TargetObject) const
{
	UWorldFortificationModificationsComponent* FortificationComponent =
		WorldEnemyTurnDirectorPrivate::GetFortificationComponent(TargetObject);
	UWorldStrengthEstimationComponent* StrengthComponent = TargetObject.GetWorldStrengthEstimationComponent();
	const EWorldFortificationStrength AddedFortification = IsValid(FortificationComponent)
		                                                       ? WorldEnemyTurnPlanning::SelectConvoyDeliveryModifier(
			                                                       FortificationComponent->GetFortificationStrengths(),
			                                                       M_ActiveTurnSeed)
		                                                       : EWorldFortificationStrength::None;
	if (AddedFortification != EWorldFortificationStrength::None)
	{
		FortificationComponent->AddFortificationStrength(AddedFortification);
		if (IsValid(StrengthComponent))
		{
			RebuildFortificationModifierReport(*FortificationComponent, *StrengthComponent);
		}
		return AddedFortification;
	}

	if (IsValid(StrengthComponent))
	{
		StrengthComponent->AddBaseFortificationStrengthPercentage(
			UWorldEnemyTurnSettings::GetNumbers().ConvoyFallbackDeliveryStrengthPercentage);
	}
	return EWorldFortificationStrength::None;
}

void UWorldEnemyTurnDirector::RebuildFortificationModifierReport(
	const UWorldFortificationModificationsComponent& FortificationComponent,
	UWorldStrengthEstimationComponent& StrengthComponent) const
{
	const UWorldDataComponent* WorldDataComponent = GetIsValidWorldGenerator()
		                                                ? M_WorldGenerator->GetWorldDataComponent()
		                                                : nullptr;
	if (not IsValid(WorldDataComponent))
	{
		return;
	}

	StrengthComponent.ResetFortificationModifierReport();
	for (const EWorldFortificationStrength FortificationStrength : FortificationComponent.GetFortificationStrengths())
	{
		FWorldStrengthReason FortificationReason;
		if (WorldDataComponent->TryBuildFortificationStrengthReason(
			FortificationStrength, M_ActiveDifficulty, FortificationReason))
		{
			StrengthComponent.AddFortificationModifierReason(FortificationStrength, FortificationReason);
		}
	}
}

void UWorldEnemyTurnDirector::FinishEnemyTurn(FWorldEnemyTurnState& EnemyTurnState)
{
	EnemyTurnState.RecentlyStruckAnchorKeys.Reset();
	EnemyTurnState.RecentlyStruckLocations.Reset();
	EnemyTurnState.EnemyTurnsPlayed++;
	EnemyTurnState.MissionsCompletedAtLastEnemyTurn = M_ProgressManager->GetProgressState().MissionsCompleted;

	M_DivisionManager->RefreshDivisionInfluence(M_ActiveDifficulty);
	CacheTurnResultsForSave();

	bM_IsEnemyTurnInProgress = false;
	const FSimpleDelegate OnTurnFinished = M_OnTurnFinished;
	M_OnTurnFinished.Unbind();
	OnEnemyTurnFinished.Broadcast(GetLastEnemyTurnSummary());
	OnTurnFinished.ExecuteIfBound();
}

void UWorldEnemyTurnDirector::CacheTurnResultsForSave() const
{
	const AWorldPlayerController* WorldPlayerController = M_WorldPlayerController.Get();
	UWorldStateAndSaveManager* SaveManager = IsValid(WorldPlayerController)
		                                         ? WorldPlayerController->GetWorldStateAndSaveManager()
		                                         : nullptr;
	if (not IsValid(SaveManager) || not GetIsValidWorldGenerator() || not GetIsValidDivisionManager())
	{
		return;
	}

	// Rebuilding the world state picks up convoy fortifications but empties the division slice, so re-cache it.
	SaveManager->CacheCurrentWorldState(*M_WorldGenerator.Get());
	M_DivisionManager->CacheDivisionSaveState();
}

void UWorldEnemyTurnDirector::OnDivisionRemoved(const FGuid& DivisionKey, const FVector& LastLocation)
{
	FWorldEnemyTurnState* EnemyTurnState = GetMutableEnemyTurnState();
	if (EnemyTurnState == nullptr)
	{
		return;
	}

	EnemyTurnState->ReactionTargetAnchorByPieceKey.Remove(DivisionKey);
}

AAnchorPoint* UWorldEnemyTurnDirector::FindAnchorByKey(const FGuid& AnchorKey) const
{
	if (not AnchorKey.IsValid() || not GetIsValidWorldGenerator())
	{
		return nullptr;
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_WorldGenerator->GetPlacementState().CachedAnchors)
	{
		if (IsValid(AnchorPoint) && AnchorPoint->GetAnchorKey() == AnchorKey)
		{
			return AnchorPoint.Get();
		}
	}
	return nullptr;
}

AWorldMapObject* UWorldEnemyTurnDirector::FindMapObjectByAnchorKey(const FGuid& AnchorKey) const
{
	const AAnchorPoint* AnchorPoint = FindAnchorByKey(AnchorKey);
	return IsValid(AnchorPoint) ? AnchorPoint->GetPromotedWorldObject() : nullptr;
}

bool UWorldEnemyTurnDirector::GetIsAnchorCleared(const FGuid& AnchorKey) const
{
	if (GetIsValidProgressManager() && M_ProgressManager->GetIsAnchorCleared(AnchorKey))
	{
		return true;
	}

	const AWorldMapObject* MapObject = FindMapObjectByAnchorKey(AnchorKey);
	return IsValid(MapObject) && MapObject->GetIsClearedByPlayer();
}

TArray<FWorldEnemyTurnPlace> UWorldEnemyTurnDirector::BuildTargetCandidates() const
{
	TArray<FWorldEnemyTurnPlace> TargetCandidates;
	if (not GetIsValidWorldGenerator())
	{
		return TargetCandidates;
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_WorldGenerator->GetPlacementState().CachedAnchors)
	{
		const AWorldMapObject* MapObject = IsValid(AnchorPoint) ? AnchorPoint->GetPromotedWorldObject() : nullptr;
		const bool bIsEnemyTarget = IsValid(MapObject)
			&& (MapObject->IsA(AWorldEnemyObject::StaticClass()) || MapObject->IsA(AWorldMissionObject::StaticClass()));
		if (not bIsEnemyTarget || GetIsAnchorCleared(AnchorPoint->GetAnchorKey()))
		{
			continue;
		}

		FWorldEnemyTurnPlace TargetCandidate;
		TargetCandidate.Key = AnchorPoint->GetAnchorKey();
		TargetCandidate.Location = WorldEnemyTurnDirectorPrivate::GetXY(AnchorPoint->GetActorLocation());
		TargetCandidates.Add(TargetCandidate);
	}
	return TargetCandidates;
}

TArray<FWorldEnemyTurnPlace> UWorldEnemyTurnDirector::BuildSourceCandidates() const
{
	TArray<FWorldEnemyTurnPlace> SourceCandidates;
	if (not GetIsValidWorldGenerator())
	{
		return SourceCandidates;
	}

	const TArray<EMapEnemyItem>& SourceItems = UWorldEnemyTurnSettings::GetNumbers().ReactionSourceItems;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_WorldGenerator->GetPlacementState().CachedAnchors)
	{
		const AWorldEnemyObject* EnemyObject = IsValid(AnchorPoint)
			                                       ? Cast<AWorldEnemyObject>(AnchorPoint->GetPromotedWorldObject())
			                                       : nullptr;
		if (not IsValid(EnemyObject) || GetIsAnchorCleared(AnchorPoint->GetAnchorKey()))
		{
			continue;
		}

		const EMapEnemyItem EnemyItemType = EnemyObject->GetEnemyItemType();
		if (EnemyItemType != EMapEnemyItem::EnemyHQ && not SourceItems.Contains(EnemyItemType))
		{
			continue;
		}

		FWorldEnemyTurnPlace SourceCandidate;
		SourceCandidate.Key = AnchorPoint->GetAnchorKey();
		SourceCandidate.Location = WorldEnemyTurnDirectorPrivate::GetXY(AnchorPoint->GetActorLocation());
		SourceCandidates.Add(SourceCandidate);
	}
	return SourceCandidates;
}

TArray<FVector2D> UWorldEnemyTurnDirector::BuildStruckLocations(const FWorldEnemyTurnState& EnemyTurnState) const
{
	TArray<FVector2D> StruckLocations;
	for (const FGuid& StruckAnchorKey : EnemyTurnState.RecentlyStruckAnchorKeys)
	{
		if (const AAnchorPoint* StruckAnchor = FindAnchorByKey(StruckAnchorKey))
		{
			StruckLocations.Add(WorldEnemyTurnDirectorPrivate::GetXY(StruckAnchor->GetActorLocation()));
		}
	}

	for (const FVector& StruckLocation : EnemyTurnState.RecentlyStruckLocations)
	{
		StruckLocations.Add(WorldEnemyTurnDirectorPrivate::GetXY(StruckLocation));
	}

	// No strike: reinforce the targets facing the player HQ, the front the player will push next.
	if (StruckLocations.Num() == 0 && GetIsValidWorldGenerator())
	{
		const AAnchorPoint* PlayerHQAnchor = M_WorldGenerator->GetPlacementState().PlayerHQAnchor;
		if (IsValid(PlayerHQAnchor))
		{
			StruckLocations.Add(WorldEnemyTurnDirectorPrivate::GetXY(PlayerHQAnchor->GetActorLocation()));
		}
	}
	return StruckLocations;
}

bool UWorldEnemyTurnDirector::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldPlayerController"),
		TEXT("UWorldEnemyTurnDirector::GetIsValidWorldPlayerController"),
		this);
	return false;
}

bool UWorldEnemyTurnDirector::GetIsValidProgressManager() const
{
	if (M_ProgressManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ProgressManager"),
		TEXT("UWorldEnemyTurnDirector::GetIsValidProgressManager"),
		this);
	return false;
}

bool UWorldEnemyTurnDirector::GetIsValidDivisionManager() const
{
	if (M_DivisionManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_DivisionManager"),
		TEXT("UWorldEnemyTurnDirector::GetIsValidDivisionManager"),
		this);
	return false;
}

bool UWorldEnemyTurnDirector::GetIsValidWorldGenerator() const
{
	if (M_WorldGenerator.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldGenerator"),
		TEXT("UWorldEnemyTurnDirector::GetIsValidWorldGenerator"),
		this);
	return false;
}
