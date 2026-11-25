// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyItemSiteDirector.h"

#include "EngineUtils.h"
#include "EnemyConvoyBattleTracker.h"
#include "EnemyDivisionArmySpawner.h"
#include "EnemyItemSignatureSlot.h"
#include "EnemyItemStrengthSlot.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRouteActor/EnemyConvoyRoute.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace EnemyItemSiteDirectorConstants
{
	const FColor DirectorColor(255, 90, 30);
	constexpr float GroundLiftZ = 30.f;
	constexpr float FacingArrowLength = 1500.f;
	constexpr float RadiusLineThickness = 6.f;
	constexpr float RadiusBrightness = 0.6f;
	// Small wait so the enemy controller has run BeginPlay before its tree and training are replaced.
	constexpr float EnemyAIProfileDelaySeconds = 0.5f;
}

AEnemyItemSiteDirector::AEnemyItemSiteDirector()
{
	TestContext.ItemType = EMapEnemyItem::Airfield;
	M_ArmySpawner = CreateDefaultSubobject<UEnemyDivisionArmySpawner>(TEXT("DivisionArmySpawner"));
	M_ConvoyBattleTracker = CreateDefaultSubobject<UEnemyConvoyBattleTracker>(TEXT("ConvoyBattleTracker"));
}

bool AEnemyItemSiteDirector::GetIsValidArmySpawner() const
{
	if (IsValid(M_ArmySpawner))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_ArmySpawner", "GetIsValidArmySpawner", this);
	return false;
}

bool AEnemyItemSiteDirector::GetIsValidConvoyBattleTracker() const
{
	if (IsValid(M_ConvoyBattleTracker))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_ConvoyBattleTracker",
	                                                      "GetIsValidConvoyBattleTracker", this);
	return false;
}

void AEnemyItemSiteDirector::BeginPlay()
{
	Super::BeginPlay();

	// Next tick: every actor of the level has run BeginPlay, so the spawned buildings meet initialised managers.
	TWeakObjectPtr<AEnemyItemSiteDirector> WeakThis(this);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->BuildSiteFromMissionContext();
		}
	}));
	// The content does not wait for the enemy AI: without a controller the base must still stand.
	WaitForEnemyAI(EnemyItemSiteDirectorConstants::EnemyAIProfileDelaySeconds);
}

void AEnemyItemSiteDirector::BuildSiteFromMissionContext()
{
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	const FWorldMissionContext MissionContext = IsValid(GameInstance)
		                                            ? GameInstance->GetWorldMissionContext()
		                                            : FWorldMissionContext();
	BuildSite(EnemyItemSiteRules::BuildSiteContext(MissionContext, TestContext));
}

bool AEnemyItemSiteDirector::BuildSite(const FEnemyItemSiteContext& SiteContext)
{
	if (bM_HasStartedSite)
	{
		return false;
	}

	bM_HasStartedSite = true;
	M_SiteContext = SiteContext;
	if (M_SiteContext.bIsDivisionOperation)
	{
		// A field map has no signature slots; strength slots still switch on (a supporting division's dug-in guns).
		BuildSite_ActivateStrengthSlots();
		bM_IsConvoyRouteBattle = GetShouldDriveConvoyRoute();
		if (bM_IsConvoyRouteBattle)
		{
			BuildSite_StartConvoyBattle();
			return true;
		}

		BuildSite_SpawnDivisionArmy();
		return true;
	}

	BuildSite_FillSignatureSlots();
	BuildSite_ActivateStrengthSlots();
	BuildSite_ReportMissingSignatureTargets();
	FinishBuildingSite();
	return true;
}

void AEnemyItemSiteDirector::BuildSite_SpawnDivisionArmy()
{
	if (not GetIsValidArmySpawner())
	{
		FinishBuildingSite();
		return;
	}

	if (M_SiteContext.Army.GetUnitCount() <= 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Enemy site %s: the attacked division %s has no units in its roster, so nothing spawns and the ")
			TEXT("field map cannot be won. Opened directly? Fill TestContext.Army."),
			*GetName(), *GetSiteItemName()));
	}

	// Bound first: an empty army answers at once, inside StartSpawningArmy.
	M_ArmySpawner->OnArmySpawned.AddUObject(this, &AEnemyItemSiteDirector::OnDivisionArmySpawned);
	M_ArmySpawner->StartSpawningArmy(M_SiteContext.Army, ArmySettings, GetEnemyPlayer());
}

bool AEnemyItemSiteDirector::GetShouldDriveConvoyRoute() const
{
	if (M_SiteContext.Army.DivisionType != EWorldFieldDivisions::SovietSupplyConvoy)
	{
		return false;
	}

	if (IsValid(ArmySettings.ConvoyRoute))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Enemy site %s: a supply convoy is attacked but ArmySettings.ConvoyRoute is not set, so its trucks park ")
		TEXT("at the start instead of driving. Place an AEnemyConvoyRoute (Start On Begin Play off) and set it."),
		*GetName()));
	return false;
}

void AEnemyItemSiteDirector::BuildSite_StartConvoyBattle()
{
	if (not GetIsValidConvoyBattleTracker())
	{
		FinishBuildingSite();
		return;
	}

	if (M_SiteContext.Army.ConvoyTruckCount <= 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Enemy site %s: the attacked supply convoy has no trucks, so no convoy drives and the field map ")
			TEXT("cannot be won. Opened directly? Set TestContext.Army.ConvoyTruckCount."), *GetName()));
	}

	// Bound first: a route that cannot start answers at once, inside StartConvoyBattle.
	M_ConvoyBattleTracker->OnBattleReady.AddUObject(this, &AEnemyItemSiteDirector::FinishBuildingSite);
	M_ConvoyBattleTracker->StartConvoyBattle(ArmySettings.ConvoyRoute, M_SiteContext.Army.ConvoyTruckCount);
}

void AEnemyItemSiteDirector::OnDivisionArmySpawned(const int32 SpawnedUnitCount)
{
	if (SpawnedUnitCount <= 0 && M_SiteContext.Army.GetUnitCount() > 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Enemy site %s: none of the %d units of division %s could be spawned, so the field map cannot be ")
			TEXT("won."), *GetName(), M_SiteContext.Army.GetUnitCount(), *GetSiteItemName()));
	}

	FinishBuildingSite();
}

void AEnemyItemSiteDirector::FinishBuildingSite()
{
	if (bM_IsSiteBuilt)
	{
		return;
	}

	bM_IsSiteBuilt = true;
	UE_LOG(LogTemp, Display,
	       TEXT("RTS_ENEMY_ITEM_SITE built item=%s source=%s division=%d victoryTargets=%d strengthSlots=%d ")
	       TEXT("strength=%d seed=%d"),
	       *GetSiteItemName(), M_SiteContext.bFromMissionContext ? TEXT("worldmap") : TEXT("test"),
	       M_SiteContext.bIsDivisionOperation ? 1 : 0, GetSpawnedVictoryTargetCount(), M_ActivatedStrengthSlotCount,
	       M_SiteContext.GetTotalStrengthPercentage(), M_SiteContext.OperationSeed);
	OnSiteBuilt.Broadcast(this);
}

void AEnemyItemSiteDirector::BuildSite_FillSignatureSlots()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	// Collected first: filling spawns actors, which must not happen while the iterator walks the level.
	TArray<AEnemyItemSignatureSlot*> SlotsToFill;
	for (TActorIterator<AEnemyItemSignatureSlot> SlotIterator(World); SlotIterator; ++SlotIterator)
	{
		SlotsToFill.Add(*SlotIterator);
	}

	for (AEnemyItemSignatureSlot* SignatureSlot : SlotsToFill)
	{
		AActor* SpawnedContent = nullptr;
		bool bIsSignatureTarget = false;
		if (not IsValid(SignatureSlot)
			|| not SignatureSlot->TryFillForItem(M_SiteContext.ItemType, SpawnedContent, bIsSignatureTarget))
		{
			continue;
		}

		if (bIsSignatureTarget)
		{
			M_SignatureTargets.Add(SpawnedContent);
			M_SpawnedSignatureTargetCount++;
		}
	}
}

void AEnemyItemSiteDirector::BuildSite_ActivateStrengthSlots()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	// Collected first: Activate spawns actors, which must not happen while the iterator walks the level.
	TArray<AEnemyItemStrengthSlot*> SlotsToActivate;
	for (TActorIterator<AEnemyItemStrengthSlot> SlotIterator(World); SlotIterator; ++SlotIterator)
	{
		if (SlotIterator->GetShouldActivateForSite(M_SiteContext))
		{
			SlotsToActivate.Add(*SlotIterator);
		}
	}

	for (AEnemyItemStrengthSlot* StrengthSlot : SlotsToActivate)
	{
		if (IsValid(StrengthSlot) && IsValid(StrengthSlot->Activate()))
		{
			M_ActivatedStrengthSlotCount++;
		}
	}
}

void AEnemyItemSiteDirector::BuildSite_ReportMissingSignatureTargets() const
{
	if (M_SpawnedSignatureTargetCount > 0)
	{
		return;
	}

	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Enemy item site %s spawned no signature target for %s: no AEnemyItemSignatureSlot on this map has a ")
		TEXT("victory target for this item type, so the operation cannot be won by destroying it."),
		*GetName(), *GetSiteItemName()));
}

TArray<AActor*> AEnemyItemSiteDirector::GetSignatureTargets() const
{
	TArray<AActor*> LivingTargets;
	for (const TWeakObjectPtr<AActor>& SignatureTarget : M_SignatureTargets)
	{
		// A dying building or wreck still exists as an actor; RTSIsValid treats it as dead.
		if (RTSFunctionLibrary::RTSIsValidWeak(SignatureTarget))
		{
			LivingTargets.Add(SignatureTarget.Get());
		}
	}

	return LivingTargets;
}

TArray<AActor*> AEnemyItemSiteDirector::GetVictoryTargets() const
{
	if (not M_SiteContext.bIsDivisionOperation)
	{
		return GetSignatureTargets();
	}

	if (bM_IsConvoyRouteBattle)
	{
		return GetIsValidConvoyBattleTracker() ? M_ConvoyBattleTracker->GetLivingTrucks() : TArray<AActor*>();
	}

	return GetIsValidArmySpawner() ? M_ArmySpawner->GetLivingUnits() : TArray<AActor*>();
}

int32 AEnemyItemSiteDirector::GetSpawnedVictoryTargetCount() const
{
	if (not M_SiteContext.bIsDivisionOperation)
	{
		return M_SpawnedSignatureTargetCount;
	}

	if (bM_IsConvoyRouteBattle)
	{
		return GetIsValidConvoyBattleTracker() ? M_ConvoyBattleTracker->GetSpawnedTruckCount() : 0;
	}

	return GetIsValidArmySpawner() ? M_ArmySpawner->GetSpawnedUnitCount() : 0;
}

EEnemyItemSiteObjectiveState AEnemyItemSiteDirector::GetObjectiveState() const
{
	if (bM_IsConvoyRouteBattle)
	{
		// Decided from truck fates: an escaped truck is removed, so the living count alone would read it as a win.
		return GetIsValidConvoyBattleTracker()
			       ? EnemyItemSiteRules::GetConvoyBattleObjectiveState(M_ConvoyBattleTracker->GetBattleState())
			       : EEnemyItemSiteObjectiveState::NoTargets;
	}

	return EnemyItemSiteRules::GetObjectiveState(bM_IsSiteBuilt, GetSpawnedVictoryTargetCount(),
	                                             GetVictoryTargets().Num());
}

void AEnemyItemSiteDirector::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	Super::OnEnemyAIReady(EnemyController, Difficulty);
	if (not IsValid(EnemyController))
	{
		return;
	}

	// The profile depends on the item or division type, which is only known once the site is started.
	if (not bM_HasStartedSite)
	{
		BuildSiteFromMissionContext();
	}

	ApplyProfileToEnemyAI(*EnemyController);
	if (M_SiteContext.bIsDivisionOperation && not bM_IsConvoyRouteBattle && GetIsValidArmySpawner())
	{
		M_ArmySpawner->SetEnemyController(EnemyController);
	}
}

void AEnemyItemSiteDirector::ApplyProfileToEnemyAI(AEnemyController& EnemyController) const
{
	if (const FEnemyItemSiteProfile* Profile = FindProfileForSite())
	{
		if (IsValid(Profile->DecisionTree))
		{
			EnemyController.OverrideStrategicDecisionTree(Profile->DecisionTree);
		}

		if (Profile->bOverrideTraining)
		{
			EnemyController.ApplyMissionTraining(Profile->Training);
		}
	}

	if (GetNeedsEnemyAIDecisionTree() && not IsValid(EnemyController.GetStrategicDecisionTree()))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Enemy item site %s: enemy controller %s has no strategic decision tree, so the enemy trains and ")
			TEXT("orders nothing. Give the controller a tree or set one in the Profiles asset for %s."),
			*GetName(), *EnemyController.GetName(), *GetSiteItemName()));
	}

	if (bStartEnemyAIThinking)
	{
		EnemyController.StartStrategicAIThinkingFromMission();
	}
}

bool AEnemyItemSiteDirector::GetNeedsEnemyAIDecisionTree() const
{
	// An army that only holds its start needs no tree; an item base and an army under AI control do.
	if (not M_SiteContext.bIsDivisionOperation)
	{
		return true;
	}

	// A convoy on a route drives and fights by its definition, not by the decision tree.
	if (bM_IsConvoyRouteBattle)
	{
		return false;
	}

	return ArmySettings.GetOrdersForDivision(M_SiteContext.Army.DivisionType)
		== EEnemyDivisionArmyOrders::EnemyAIControl;
}

const FEnemyItemSiteProfile* AEnemyItemSiteDirector::FindProfileForSite() const
{
	// Read after BuildSite on the next tick; before that the site context is still empty.
	if (not bM_HasStartedSite || not IsValid(Profiles))
	{
		return nullptr;
	}

	if (M_SiteContext.bIsDivisionOperation)
	{
		return Profiles->ProfileByDivision.Find(M_SiteContext.Army.DivisionType);
	}

	return Profiles->ProfileByItem.Find(M_SiteContext.ItemType);
}

FString AEnemyItemSiteDirector::GetSiteItemName() const
{
	return M_SiteContext.bIsDivisionOperation
		       ? UEnum::GetValueAsString(M_SiteContext.Army.DivisionType)
		       : UEnum::GetValueAsString(M_SiteContext.ItemType);
}

FColor AEnemyItemSiteDirector::GetPlacementColor() const
{
	return EnemyItemSiteDirectorConstants::DirectorColor;
}

FName AEnemyItemSiteDirector::GetPlacementId() const
{
	return FName(UEnum::GetDisplayValueAsText(TestContext.ItemType).ToString());
}

void AEnemyItemSiteDirector::BuildEditorShapes(FEnemyPlacementShapes& OutSummary,
                                               FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyItemSiteDirectorConstants;
	const FVector Ground = GetActorLocation() + FVector(0.f, 0.f, GroundLiftZ);
	OutSummary.AddCircle(Ground, BaseRadius, EnemyPlacementColors::GetDimmed(DirectorColor, RadiusBrightness),
	                     RadiusLineThickness);
	const FVector Forward = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
	OutDetail.ArrowLines.Emplace(Ground, Ground + Forward * FacingArrowLength, EnemyPlacementColors::Facing);
}
