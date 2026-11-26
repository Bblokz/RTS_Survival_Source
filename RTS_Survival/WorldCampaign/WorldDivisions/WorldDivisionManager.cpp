// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"

#include "Engine/World.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnSettings.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldData/WorldDataComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldConvoyDivision.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionMovementComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionOwners.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldSquadDivision.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldTankDivision.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/StrengthEstimation/DataAndUtils/RTSStrengthEstimationTypes.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace WorldDivisionManagerConstants
{
	// A division handed out by ReleaseDivisionForDeferredDestroy is destroyed after this long at the latest.
	constexpr float ReleasedDivisionLifeSpanSeconds = 30.f;
}

namespace
{
	constexpr int32 PlayerOwner = WorldDivisionOwners::PlayerOwner;
	constexpr int32 EnemyOwner = WorldDivisionOwners::EnemyOwner;

	bool GetIsWithinXYRadius(const FVector& SourceLocation,
	                         const FVector& TargetLocation,
	                         const float XYRadius)
	{
		if (XYRadius <= 0.f)
		{
			return false;
		}

		const FVector2D SourceXY(SourceLocation.X, SourceLocation.Y);
		const FVector2D TargetXY(TargetLocation.X, TargetLocation.Y);
		return FVector2D::DistSquared(SourceXY, TargetXY) <= FMath::Square(XYRadius);
	}

	bool GetIsTankDivisionType(const EWorldFieldDivisions DivisionType)
	{
		switch (DivisionType)
		{
		case EWorldFieldDivisions::SovietLightArmorDivision:
		case EWorldFieldDivisions::SovietMediumArmorDivision:
		case EWorldFieldDivisions::SovietHeavyArmorDivision:
			return true;
		default:
			return false;
		}
	}

	TArray<AWorldMapObject*> BuildPromotedMapObjects(const FWorldCampaignPlacementState& PlacementState)
	{
		TArray<AWorldMapObject*> MapObjects;
		MapObjects.Reserve(PlacementState.CachedAnchors.Num());

		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			AWorldMapObject* PromotedWorldObject = AnchorPoint->GetPromotedWorldObject();
			if (IsValid(PromotedWorldObject))
			{
				MapObjects.Add(PromotedWorldObject);
			}
		}

		return MapObjects;
	}

	int32 GetOwnerForMapObject(const AWorldMapObject* MapObject)
	{
		if (not IsValid(MapObject) || MapObject->IsA(AWorldNeutralObject::StaticClass()))
		{
			return INDEX_NONE;
		}

		if (MapObject->IsA(AWorldPlayerObject::StaticClass()))
		{
			return PlayerOwner;
		}

		if (MapObject->IsA(AWorldEnemyObject::StaticClass())
			|| MapObject->IsA(AWorldMissionObject::StaticClass()))
		{
			return EnemyOwner;
		}

		return INDEX_NONE;
	}

	int32 GetDivisionInfluenceSign(const AWorldDivisionBase& WorldDivision, const AWorldMapObject& TargetObject)
	{
		/*
		 * Positive field-division strength means "harder" in the existing strength UI. Only the enemy has divisions
		 * and convoys: they make enemy items and missions in range harder and do nothing to player or neutral items.
		 * Cleared targets are no longer fought over, so they get nothing either.
		 */
		if (WorldDivision.GetOwningPlayer() != EnemyOwner || TargetObject.GetIsClearedByPlayer())
		{
			return 0;
		}

		const bool bIsMissionObject = TargetObject.IsA(AWorldMissionObject::StaticClass());
		const bool bIsEnemyObject = TargetObject.IsA(AWorldEnemyObject::StaticClass());
		return bIsMissionObject || bIsEnemyObject ? 1 : 0;
	}

	FText BuildFallbackDivisionReasonText(const EWorldFieldDivisions DivisionType)
	{
		const UEnum* DivisionEnum = StaticEnum<EWorldFieldDivisions>();
		if (not IsValid(DivisionEnum))
		{
			return FText::FromString(TEXT("Field Division"));
		}

		return DivisionEnum->GetDisplayNameTextByValue(static_cast<int64>(DivisionType));
	}
}

UWorldDivisionManager::UWorldDivisionManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldDivisionManager::InitializeNewCampaignDivisions(AWorldPlayerController* WorldPlayerController,
                                                           AGeneratorWorldCampaign* WorldGenerator,
                                                           const ERTSGameDifficulty GameDifficulty)
{
	if (not CacheRuntimeReferences(WorldPlayerController, WorldGenerator))
	{
		return;
	}

	DestroyRegisteredDivisions();
	SpawnInitialDivisions(GameDifficulty);
	RefreshDivisionInfluence(GameDifficulty);
	CacheDivisionSaveState();
}

void UWorldDivisionManager::RestoreSavedCampaignDivisions(AWorldPlayerController* WorldPlayerController,
                                                          AGeneratorWorldCampaign* WorldGenerator,
                                                          const ERTSGameDifficulty GameDifficulty,
                                                          const TArray<FWorldDivisionSaveData>& DivisionSaveData)
{
	if (not CacheRuntimeReferences(WorldPlayerController, WorldGenerator))
	{
		return;
	}

	DestroyRegisteredDivisions();
	SpawnSavedDivisions(GameDifficulty, DivisionSaveData);
	RefreshDivisionInfluence(GameDifficulty);
	CacheDivisionSaveState();
}

void UWorldDivisionManager::MovePlayerDivisions(const ERTSGameDifficulty GameDifficulty)
{
	MoveDivisionsForOwner(PlayerOwner, EWorldTurnType::Player, GameDifficulty, true, FSimpleDelegate());
}

void UWorldDivisionManager::MoveEnemyDivisions(const ERTSGameDifficulty GameDifficulty)
{
	MoveDivisionsForOwner(EnemyOwner, EWorldTurnType::Enemy, GameDifficulty, true, FSimpleDelegate());
}

void UWorldDivisionManager::MoveEnemyDivisionsThenNotify(const ERTSGameDifficulty GameDifficulty,
                                                         const FSimpleDelegate& OnMovementFinished)
{
	MoveDivisionsForOwner(EnemyOwner, EWorldTurnType::Enemy, GameDifficulty, false, OnMovementFinished);
}

AWorldDivisionBase* UWorldDivisionManager::SpawnEnemyReactionPiece(const EWorldFieldDivisions PieceType,
                                                                   const FVector& SpawnLocation,
                                                                   const FGuid& DivisionKey,
                                                                   const ERTSGameDifficulty GameDifficulty)
{
	if (PieceType == EWorldFieldDivisions::None || not GetIsValidWorldGenerator())
	{
		return nullptr;
	}

	FTransform SpawnTransform = FTransform::Identity;
	SpawnTransform.SetLocation(SpawnLocation);
	AWorldDivisionBase* WorldDivision = SpawnDivision(PieceType, SpawnTransform);
	if (not IsValid(WorldDivision))
	{
		return nullptr;
	}

	// The C++ tank and squad fallback classes have no scene root: such a piece would sit at the origin and never move.
	if (not IsValid(WorldDivision->GetRootComponent()))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Enemy reaction piece class has no root component; configure a Blueprint for this type in ")
			TEXT("UWorldCampaignSettings::WorldDivisionClassByType. The piece was not spawned."));
		WorldDivision->Destroy();
		return nullptr;
	}

	InitializeDivisionFromWorldData(*WorldDivision, DivisionKey, PieceType, EnemyOwner, GameDifficulty);
	ApplyReactionPieceRoster(*WorldDivision, UWorldEnemyTurnSettings::FindPieceSetup(PieceType));
	RegisterDivision(WorldDivision);
	return WorldDivision;
}

AWorldDivisionBase* UWorldDivisionManager::FindDivisionByKey(const FGuid& DivisionKey) const
{
	if (not DivisionKey.IsValid())
	{
		return nullptr;
	}

	for (const TObjectPtr<AWorldDivisionBase>& WorldDivision : M_WorldDivisions)
	{
		if (IsValid(WorldDivision) && WorldDivision->GetDivisionKey() == DivisionKey)
		{
			return WorldDivision.Get();
		}
	}
	return nullptr;
}

bool UWorldDivisionManager::RemoveDivisionByKey(const FGuid& DivisionKey)
{
	// Not IsValid: the unregistered actor was just destroyed.
	return UnregisterDivisionByKey(DivisionKey, EWorldDivisionUnregisterMode::DestroyActor) != nullptr;
}

AWorldDivisionBase* UWorldDivisionManager::ReleaseDivisionForDeferredDestroy(const FGuid& DivisionKey)
{
	AWorldDivisionBase* WorldDivision = UnregisterDivisionByKey(DivisionKey, EWorldDivisionUnregisterMode::KeepActor);
	if (not IsValid(WorldDivision))
	{
		return nullptr;
	}

	// The husk is only scenery now: it takes no cursor trace, and it cannot outlive a caller that forgets it.
	WorldDivision->SetActorEnableCollision(false);
	WorldDivision->SetLifeSpan(WorldDivisionManagerConstants::ReleasedDivisionLifeSpanSeconds);
	return WorldDivision;
}

AWorldDivisionBase* UWorldDivisionManager::UnregisterDivisionByKey(const FGuid& DivisionKey,
                                                                   const EWorldDivisionUnregisterMode Mode)
{
	AWorldDivisionBase* WorldDivision = FindDivisionByKey(DivisionKey);
	if (not IsValid(WorldDivision))
	{
		return nullptr;
	}

	/*
	 * Unregister first so callbacks fired by StopTurnMovement (which may end a movement pass and run its waiting
	 * callers) already see the map without this piece.
	 */
	const FVector LastLocation = WorldDivision->GetActorLocation();
	WorldDivision->OnDivisionStrengthChanged().RemoveAll(this);
	M_WorldDivisions.Remove(WorldDivision);
	M_DivisionKeysMovingThisPass.Remove(DivisionKey);
	WorldDivision->StopTurnMovement();
	if (UWorldDivisionMovementComponent* MovementComponent = WorldDivision->GetWorldDivisionMovementComponent())
	{
		MovementComponent->OnMovementFinished().RemoveAll(this);
	}
	if (Mode == EWorldDivisionUnregisterMode::DestroyActor)
	{
		WorldDivision->Destroy();
	}

	RefreshDivisionInfluence(M_CurrentGameDifficulty);
	CacheDivisionSaveState();
	M_OnDivisionRemoved.Broadcast(DivisionKey, LastLocation);
	TryFinishMovementPassIfIdle();
	return WorldDivision;
}

int32 UWorldDivisionManager::ApplyDamageToEnemyDivisionsInRadius(const FVector& Center,
                                                                 const float Radius,
                                                                 const int32 DamageEntryCount)
{
	if (DamageEntryCount <= 0 || Radius <= 0.f)
	{
		return 0;
	}

	// Damage can remove a piece and change M_WorldDivisions, so iterate a weak snapshot.
	TArray<TWeakObjectPtr<AWorldDivisionBase>> DivisionsInRadius;
	for (const TObjectPtr<AWorldDivisionBase>& WorldDivision : M_WorldDivisions)
	{
		if (IsValid(WorldDivision)
			&& WorldDivision->GetOwningPlayer() == EnemyOwner
			&& GetIsWithinXYRadius(Center, WorldDivision->GetActorLocation(), Radius))
		{
			DivisionsInRadius.Add(WorldDivision.Get());
		}
	}

	int32 HitCount = 0;
	for (const TWeakObjectPtr<AWorldDivisionBase>& WeakDivision : DivisionsInRadius)
	{
		AWorldDivisionBase* WorldDivision = WeakDivision.Get();
		if (not IsValid(WorldDivision))
		{
			continue;
		}

		WorldDivision->ApplyWorldDivisionDamage(DamageEntryCount);
		HitCount++;
	}
	return HitCount;
}

TArray<FWorldStrengthContribution> UWorldDivisionManager::BuildOperationStrengthContributionsForDivision(
	const FGuid& DivisionKey) const
{
	TArray<FWorldStrengthContribution> StrengthContributions;
	const AWorldDivisionBase* TargetDivision = FindDivisionByKey(DivisionKey);
	if (not IsValid(TargetDivision))
	{
		return StrengthContributions;
	}

	if (TargetDivision->GetCurrentStrengthPercentage() != 0)
	{
		StrengthContributions.Add(TargetDivision->BuildOwnStrengthContribution());
	}

	for (const TObjectPtr<AWorldDivisionBase>& SupportingDivision : M_WorldDivisions)
	{
		if (not IsValid(SupportingDivision)
			|| SupportingDivision.Get() == TargetDivision
			|| SupportingDivision->GetOwningPlayer() != EnemyOwner
			|| SupportingDivision->GetCurrentStrengthPercentage() == 0
			|| not GetIsWithinXYRadius(
				SupportingDivision->GetActorLocation(),
				TargetDivision->GetActorLocation(),
				SupportingDivision->GetInfluenceAreaRadius()))
		{
			continue;
		}

		StrengthContributions.Add(SupportingDivision->BuildOwnStrengthContribution());
	}
	return StrengthContributions;
}

void UWorldDivisionManager::RefreshDivisionInfluence(const ERTSGameDifficulty GameDifficulty)
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	/*
	 * Field-division reasons are fully rebuilt each time so movement and damage cannot leave stale strength entries on
	 * map objects that were previously in range.
	 */
	M_CurrentGameDifficulty = GameDifficulty;
	const TArray<AWorldMapObject*> MapObjects = BuildPromotedMapObjects(M_WorldGenerator->GetPlacementState());
	for (AWorldMapObject* MapObject : MapObjects)
	{
		if (not IsValid(MapObject))
		{
			continue;
		}

		UWorldStrengthEstimationComponent* StrengthEstimationComponent =
			MapObject->GetWorldStrengthEstimationComponent();
		if (IsValid(StrengthEstimationComponent))
		{
			StrengthEstimationComponent->ResetFieldDivisionReport();
		}
	}

	for (const TObjectPtr<AWorldDivisionBase>& WorldDivision : M_WorldDivisions)
	{
		if (not IsValid(WorldDivision) || WorldDivision->GetCurrentStrengthPercentage() == 0)
		{
			continue;
		}

		for (AWorldMapObject* MapObject : MapObjects)
		{
			if (not IsValid(MapObject))
			{
				continue;
			}

			if (not GetIsWithinXYRadius(
				WorldDivision->GetActorLocation(),
				MapObject->GetActorLocation(),
				WorldDivision->GetInfluenceAreaRadius()))
			{
				continue;
			}

			const int32 InfluenceSign = GetDivisionInfluenceSign(*WorldDivision, *MapObject);
			if (InfluenceSign == 0)
			{
				continue;
			}

			UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				MapObject->GetWorldStrengthEstimationComponent();
			if (not IsValid(StrengthEstimationComponent))
			{
				continue;
			}

			const FWorldStrengthReason StrengthReason(
				WorldDivision->GetStrengthReasonText(),
				WorldDivision->GetCurrentStrengthPercentage() * InfluenceSign);
			StrengthEstimationComponent->AddFieldDivisionReason(
				WorldDivision->GetDivisionType(),
				WorldDivision->GetDivisionKey(),
				WorldDivision->GetOwningPlayer(),
				StrengthReason);
		}
	}
}

void UWorldDivisionManager::CacheDivisionSaveState()
{
	if (not GetIsValidWorldPlayerController())
	{
		return;
	}

	UWorldStateAndSaveManager* WorldStateAndSaveManager =
		M_WorldPlayerController->GetWorldStateAndSaveManager();
	if (not IsValid(WorldStateAndSaveManager))
	{
		return;
	}

	WorldStateAndSaveManager->CacheWorldDivisionSaveData(BuildWorldDivisionSaveData());
}

bool UWorldDivisionManager::IssueMoveOrderToDivision(AWorldDivisionBase* WorldDivision,
                                                     const FVector& TargetLocation)
{
	if (not IsValid(WorldDivision))
	{
		return false;
	}

	const bool bIssuedMoveOrder = WorldDivision->IssueMoveOrderToPoint(TargetLocation);
	if (bIssuedMoveOrder)
	{
		CacheDivisionSaveState();
	}

	return bIssuedMoveOrder;
}

TArray<FWorldDivisionSaveData> UWorldDivisionManager::BuildWorldDivisionSaveData() const
{
	TArray<FWorldDivisionSaveData> DivisionSaveData;
	DivisionSaveData.Reserve(M_WorldDivisions.Num());

	for (const TObjectPtr<AWorldDivisionBase>& WorldDivision : M_WorldDivisions)
	{
		if (IsValid(WorldDivision))
		{
			DivisionSaveData.Add(WorldDivision->BuildWorldDivisionSaveData());
		}
	}

	return DivisionSaveData;
}

bool UWorldDivisionManager::CacheRuntimeReferences(AWorldPlayerController* WorldPlayerController,
                                                   AGeneratorWorldCampaign* WorldGenerator)
{
	if (not IsValid(WorldPlayerController) || not IsValid(WorldGenerator))
	{
		return false;
	}

	M_WorldPlayerController = WorldPlayerController;
	M_WorldGenerator = WorldGenerator;
	M_WorldDataComponent = const_cast<UWorldDataComponent*>(WorldGenerator->GetWorldDataComponent());
	return GetIsValidWorldDataComponent();
}

void UWorldDivisionManager::DestroyRegisteredDivisions()
{
	for (int32 DivisionIndex = M_WorldDivisions.Num() - 1; DivisionIndex >= 0; DivisionIndex--)
	{
		AWorldDivisionBase* WorldDivision = M_WorldDivisions[DivisionIndex];
		if (IsValid(WorldDivision))
		{
			WorldDivision->OnDivisionStrengthChanged().RemoveAll(this);
			WorldDivision->Destroy();
		}
	}

	M_WorldDivisions.Reset();
	M_DivisionKeysMovingThisPass.Reset();
}

void UWorldDivisionManager::SpawnInitialDivisions(const ERTSGameDifficulty GameDifficulty)
{
	const UWorldCampaignSettings* Settings = UWorldCampaignSettings::Get();
	if (not IsValid(Settings))
	{
		return;
	}

	for (const FWorldDivisionInitialSpawnData& SpawnData : Settings->InitialWorldDivisions)
	{
		AWorldDivisionBase* WorldDivision = SpawnDivision(SpawnData.DivisionType, SpawnData.Transform);
		if (not IsValid(WorldDivision))
		{
			continue;
		}

		InitializeDivisionFromWorldData(
			*WorldDivision,
			FGuid::NewGuid(),
			SpawnData.DivisionType,
			SpawnData.OwningPlayer,
			GameDifficulty);

		if (AWorldTankDivision* TankDivision = Cast<AWorldTankDivision>(WorldDivision))
		{
			const TArray<ETankSubtype> TankSubtypes = SpawnData.TankSubtypes.Num() > 0
				                                          ? SpawnData.TankSubtypes
				                                          : TankDivision->GetTankSubtypes();
			TankDivision->SetTankSubtypes(TankSubtypes);
		}

		if (AWorldSquadDivision* SquadDivision = Cast<AWorldSquadDivision>(WorldDivision))
		{
			const TArray<ESquadSubtype> SquadSubtypes = SpawnData.SquadSubtypes.Num() > 0
				                                            ? SpawnData.SquadSubtypes
				                                            : SquadDivision->GetSquadSubtypes();
			SquadDivision->SetSquadSubtypes(SquadSubtypes);
		}

		RegisterDivision(WorldDivision);
	}
}

void UWorldDivisionManager::SpawnSavedDivisions(const ERTSGameDifficulty GameDifficulty,
                                                const TArray<FWorldDivisionSaveData>& DivisionSaveData)
{
	for (const FWorldDivisionSaveData& SavedDivision : DivisionSaveData)
	{
		FTransform SpawnTransform = FTransform::Identity;
		SpawnTransform.SetLocation(SavedDivision.Location);
		AWorldDivisionBase* WorldDivision = SpawnDivision(SavedDivision.DivisionType, SpawnTransform);
		if (not IsValid(WorldDivision))
		{
			continue;
		}

		InitializeDivisionFromWorldData(
			*WorldDivision,
			SavedDivision.DivisionKey,
			SavedDivision.DivisionType,
			SavedDivision.OwningPlayer,
			GameDifficulty);
		WorldDivision->RestoreWorldDivisionSaveData(SavedDivision);
		RegisterDivision(WorldDivision);
	}
}

AWorldDivisionBase* UWorldDivisionManager::SpawnDivision(const EWorldFieldDivisions DivisionType,
                                                         const FTransform& SpawnTransform) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	const TSubclassOf<AWorldDivisionBase> DivisionClass = ResolveDivisionClass(DivisionType);
	if (not IsValid(DivisionClass.Get()))
	{
		RTSFunctionLibrary::ReportError(TEXT("No valid world division class found for division type."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = M_WorldPlayerController.Get();
	return World->SpawnActor<AWorldDivisionBase>(DivisionClass, SpawnTransform, SpawnParameters);
}

void UWorldDivisionManager::RegisterDivision(AWorldDivisionBase* WorldDivision)
{
	if (not IsValid(WorldDivision))
	{
		return;
	}

	WorldDivision->OnDivisionStrengthChanged().RemoveAll(this);
	WorldDivision->OnDivisionStrengthChanged().AddUObject(
		this,
		&UWorldDivisionManager::OnDivisionStrengthChanged);
	M_WorldDivisions.Add(WorldDivision);
}

TSubclassOf<AWorldDivisionBase> UWorldDivisionManager::ResolveDivisionClass(
	const EWorldFieldDivisions DivisionType) const
{
	const UWorldCampaignSettings* Settings = UWorldCampaignSettings::Get();
	if (IsValid(Settings))
	{
		const TSubclassOf<AWorldDivisionBase>* DivisionClass =
			Settings->WorldDivisionClassByType.Find(DivisionType);
		if (DivisionClass != nullptr && IsValid(DivisionClass->Get()))
		{
			return *DivisionClass;
		}
	}

	if (GetIsTankDivisionType(DivisionType))
	{
		return AWorldTankDivision::StaticClass();
	}

	if (DivisionType == EWorldFieldDivisions::SovietInfantryDivision)
	{
		return AWorldSquadDivision::StaticClass();
	}

	if (DivisionType == EWorldFieldDivisions::SovietSupplyConvoy)
	{
		return AWorldConvoyDivision::StaticClass();
	}

	return nullptr;
}

void UWorldDivisionManager::InitializeDivisionFromWorldData(AWorldDivisionBase& WorldDivision,
                                                            const FGuid& DivisionKey,
                                                            const EWorldFieldDivisions DivisionType,
                                                            const int32 OwningPlayer,
                                                            const ERTSGameDifficulty GameDifficulty) const
{
	FWorldStrengthReason StrengthReason;
	if (GetIsValidWorldDataComponent())
	{
		M_WorldDataComponent->TryBuildFieldDivisionReason(DivisionType, GameDifficulty, StrengthReason);
	}

	const FText StrengthReasonText = StrengthReason.StrengthReason.IsEmpty()
		                                 ? BuildFallbackDivisionReasonText(DivisionType)
		                                 : StrengthReason.StrengthReason;
	WorldDivision.InitializeWorldDivision(
		DivisionKey,
		DivisionType,
		OwningPlayer,
		StrengthReason.StrengthPercent,
		StrengthReasonText);

	/*
	 * Without a WorldData entry a piece would have zero strength and be skipped by influence. The enemy turn settings
	 * hold a fallback per type; it is applied on every spawn and restore so saved damage ratios stay consistent.
	 */
	const FWorldEnemyReactionPieceSetup* PieceSetup = UWorldEnemyTurnSettings::FindPieceSetup(DivisionType);
	if (WorldDivision.GetMaxStrengthPercentage() <= 0 && PieceSetup != nullptr
		&& PieceSetup->FallbackMaxStrengthPercentage > 0)
	{
		WorldDivision.SetMaxStrengthPercentage(PieceSetup->FallbackMaxStrengthPercentage);
		WorldDivision.SetCurrentStrengthPercentage(PieceSetup->FallbackMaxStrengthPercentage);
	}
}

void UWorldDivisionManager::ApplyReactionPieceRoster(AWorldDivisionBase& WorldDivision,
                                                     const FWorldEnemyReactionPieceSetup* PieceSetup) const
{
	if (PieceSetup == nullptr)
	{
		return;
	}

	if (AWorldTankDivision* TankDivision = Cast<AWorldTankDivision>(&WorldDivision))
	{
		if (PieceSetup->TankSubtypes.Num() > 0)
		{
			TankDivision->SetTankSubtypes(PieceSetup->TankSubtypes);
		}
		return;
	}

	AWorldSquadDivision* SquadDivision = Cast<AWorldSquadDivision>(&WorldDivision);
	if (IsValid(SquadDivision) && PieceSetup->SquadSubtypes.Num() > 0)
	{
		SquadDivision->SetSquadSubtypes(PieceSetup->SquadSubtypes);
	}
}

void UWorldDivisionManager::MoveDivisionsForOwner(const int32 OwningPlayer,
                                                  const EWorldTurnType TurnType,
                                                  const ERTSGameDifficulty GameDifficulty,
                                                  const bool bSaveWhenFinished,
                                                  const FSimpleDelegate& OnMovementFinished)
{
	if (not GetIsValidWorldGenerator())
	{
		OnMovementFinished.ExecuteIfBound();
		return;
	}

	/*
	 * Movement is allowed only for the active owner, but completion is asynchronous because the movement component
	 * interpolates visually. Movers are tracked by key across passes: a pass started while an earlier pass still
	 * moves joins it instead of resetting it, and every waiting caller runs once the last mover reported back.
	 */
	M_ActiveMovingTurn = TurnType;
	M_ActiveMovementDifficulty = GameDifficulty;
	bM_ShouldSaveWhenMovementFinished = bM_ShouldSaveWhenMovementFinished || bSaveWhenFinished;
	if (OnMovementFinished.IsBound())
	{
		M_MovementFinishedCallbacks.Add(OnMovementFinished);
	}

	bM_IsStartingMovementPass = true;
	const TArray<TObjectPtr<AWorldDivisionBase>> DivisionsSnapshot = M_WorldDivisions;
	for (const TObjectPtr<AWorldDivisionBase>& WorldDivision : DivisionsSnapshot)
	{
		if (not IsValid(WorldDivision)
			|| WorldDivision->GetOwningPlayer() != OwningPlayer
			|| not WorldDivision->GetHasPendingMoveOrder())
		{
			continue;
		}

		(void)TryStartTurnMovement(*WorldDivision);
	}

	bM_IsStartingMovementPass = false;
	TryFinishMovementPassIfIdle();
}

bool UWorldDivisionManager::TryStartTurnMovement(AWorldDivisionBase& WorldDivision)
{
	// A division still playing an earlier pass keeps that pass's binding and key.
	if (WorldDivision.GetIsMoving())
	{
		return false;
	}

	UWorldDivisionMovementComponent* MovementComponent = WorldDivision.GetWorldDivisionMovementComponent();
	if (not IsValid(MovementComponent))
	{
		return false;
	}

	const FGuid DivisionKey = WorldDivision.GetDivisionKey();
	MovementComponent->OnMovementFinished().RemoveAll(this);
	MovementComponent->OnMovementFinished().AddUObject(this, &UWorldDivisionManager::OnDivisionMovementFinished);
	M_DivisionKeysMovingThisPass.Add(DivisionKey);
	if (WorldDivision.MoveForTurnDistance())
	{
		return true;
	}

	MovementComponent->OnMovementFinished().RemoveAll(this);
	M_DivisionKeysMovingThisPass.Remove(DivisionKey);
	return false;
}

void UWorldDivisionManager::PruneStaleMovingDivisionKeys()
{
	TArray<FGuid> StaleDivisionKeys;
	for (const FGuid& DivisionKey : M_DivisionKeysMovingThisPass)
	{
		const AWorldDivisionBase* WorldDivision = FindDivisionByKey(DivisionKey);
		if (not IsValid(WorldDivision) || not WorldDivision->GetIsMoving())
		{
			StaleDivisionKeys.Add(DivisionKey);
		}
	}

	for (const FGuid& StaleDivisionKey : StaleDivisionKeys)
	{
		M_DivisionKeysMovingThisPass.Remove(StaleDivisionKey);
	}
}

void UWorldDivisionManager::TryFinishMovementPassIfIdle()
{
	if (bM_IsStartingMovementPass || M_ActiveMovingTurn == EWorldTurnType::None)
	{
		return;
	}

	PruneStaleMovingDivisionKeys();
	if (M_DivisionKeysMovingThisPass.Num() > 0)
	{
		return;
	}

	FinishActiveTurnMovement();
}

void UWorldDivisionManager::OnDivisionMovementFinished(AWorldDivisionBase* WorldDivision)
{
	if (IsValid(WorldDivision))
	{
		if (UWorldDivisionMovementComponent* MovementComponent =
			WorldDivision->GetWorldDivisionMovementComponent())
		{
			MovementComponent->OnMovementFinished().RemoveAll(this);
		}

		M_DivisionKeysMovingThisPass.Remove(WorldDivision->GetDivisionKey());
	}

	TryFinishMovementPassIfIdle();
}

void UWorldDivisionManager::OnDivisionStrengthChanged(AWorldDivisionBase* WorldDivision)
{
	if (not IsValid(WorldDivision))
	{
		return;
	}

	// A piece that lost its last tank, squad or truck leaves the map; removal refreshes influence and the cache.
	if (WorldDivision->GetRemainingEntryCount() == 0)
	{
		(void)RemoveDivisionByKey(WorldDivision->GetDivisionKey());
	}
	else
	{
		RefreshDivisionInfluence(M_CurrentGameDifficulty);
		CacheDivisionSaveState();
	}

	if (GetIsValidWorldPlayerController())
	{
		UWorldStateAndSaveManager* WorldStateAndSaveManager =
			M_WorldPlayerController->GetWorldStateAndSaveManager();
		if (IsValid(WorldStateAndSaveManager))
		{
			WorldStateAndSaveManager->SaveCampaignState();
		}
	}
}

void UWorldDivisionManager::FinishActiveTurnMovement()
{
	/*
	 * This is the one post-movement choke point for both sides: recalc strength from final positions, cache division
	 * state into the world save payload, commit the campaign save when a pass asked for it, then run the callers that
	 * waited. State is reset before the callers run so they can start the next pass right away.
	 */
	RefreshDivisionInfluence(M_ActiveMovementDifficulty);
	CacheDivisionSaveState();

	if (bM_ShouldSaveWhenMovementFinished && GetIsValidWorldPlayerController())
	{
		UWorldStateAndSaveManager* WorldStateAndSaveManager =
			M_WorldPlayerController->GetWorldStateAndSaveManager();
		if (IsValid(WorldStateAndSaveManager))
		{
			WorldStateAndSaveManager->SaveCampaignState();
		}
	}

	M_ActiveMovingTurn = EWorldTurnType::None;
	bM_ShouldSaveWhenMovementFinished = false;
	M_DivisionKeysMovingThisPass.Reset();
	const TArray<FSimpleDelegate> WaitingCallbacks = MoveTemp(M_MovementFinishedCallbacks);
	M_MovementFinishedCallbacks.Reset();
	for (const FSimpleDelegate& WaitingCallback : WaitingCallbacks)
	{
		WaitingCallback.ExecuteIfBound();
	}
}

bool UWorldDivisionManager::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldPlayerController"),
		TEXT("UWorldDivisionManager::GetIsValidWorldPlayerController"),
		this);
	return false;
}

bool UWorldDivisionManager::GetIsValidWorldGenerator() const
{
	if (M_WorldGenerator.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldGenerator"),
		TEXT("UWorldDivisionManager::GetIsValidWorldGenerator"),
		this);
	return false;
}

bool UWorldDivisionManager::GetIsValidWorldDataComponent() const
{
	if (M_WorldDataComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldDataComponent"),
		TEXT("UWorldDivisionManager::GetIsValidWorldDataComponent"),
		this);
	return false;
}
