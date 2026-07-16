// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"

#include "Engine/World.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldData/WorldDataComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionMovementComponent.h"
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

namespace
{
	constexpr int32 PlayerOwner = 1;
	constexpr int32 EnemyOwner = 2;

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
		 * Positive field-division strength means "harder" in the existing strength UI. Player divisions therefore add
		 * positive strength to friendly map objects, but negative strength to enemy missions they are suppressing.
		 */
		const int32 DivisionOwner = WorldDivision.GetOwningPlayer();
		const bool bIsMissionObject =TargetObject.IsA(AWorldMissionObject::StaticClass()); 
		const bool bIsEnemyObject = TargetObject.IsA(AWorldEnemyObject::StaticClass());
		const bool bIsPlayerObject = TargetObject.IsA(AWorldPlayerObject::StaticClass());

		if (bIsMissionObject || bIsEnemyObject)
		{
			if (DivisionOwner == PlayerOwner)
			{
				// Player division suppresses difficulty in mission / enemy base siege.
				return -1;
			}
			// Enemy division makes mission / enemy base siege harder.
			return 1;
		}
		if (bIsPlayerObject)
		{
			return DivisionOwner == EnemyOwner ? 1 : -1;
		}
		return 0;
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
	MoveDivisionsForOwner(PlayerOwner, EWorldTurnType::Player, GameDifficulty);
}

void UWorldDivisionManager::MoveEnemyDivisions(const ERTSGameDifficulty GameDifficulty)
{
	MoveDivisionsForOwner(EnemyOwner, EWorldTurnType::Enemy, GameDifficulty);
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
}

void UWorldDivisionManager::MoveDivisionsForOwner(const int32 OwningPlayer,
                                                  const EWorldTurnType TurnType,
                                                  const ERTSGameDifficulty GameDifficulty)
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	M_ActiveMovingTurn = TurnType;
	M_ActiveMovementDifficulty = GameDifficulty;
	M_ActiveTurnMovementCount = 0;

	/*
	 * Movement is allowed only for the active owner, but completion is asynchronous because the movement component
	 * interpolates visually. The manager counts started movements and performs save/influence work after all callbacks.
	 */
	for (const TObjectPtr<AWorldDivisionBase>& WorldDivision : M_WorldDivisions)
	{
		if (not IsValid(WorldDivision)
			|| WorldDivision->GetOwningPlayer() != OwningPlayer
			|| not WorldDivision->GetHasPendingMoveOrder())
		{
			continue;
		}

		UWorldDivisionMovementComponent* MovementComponent =
			WorldDivision->GetWorldDivisionMovementComponent();
		if (not IsValid(MovementComponent))
		{
			continue;
		}

		MovementComponent->OnMovementFinished().RemoveAll(this);
		MovementComponent->OnMovementFinished().AddUObject(
			this,
			&UWorldDivisionManager::OnDivisionMovementFinished);
		if (WorldDivision->MoveForTurnDistance())
		{
			M_ActiveTurnMovementCount++;
		}
	}

	if (M_ActiveTurnMovementCount == 0)
	{
		FinishActiveTurnMovement();
	}
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
	}

	M_ActiveTurnMovementCount = FMath::Max(0, M_ActiveTurnMovementCount - 1);
	if (M_ActiveTurnMovementCount > 0)
	{
		return;
	}

	FinishActiveTurnMovement();
}

void UWorldDivisionManager::OnDivisionStrengthChanged(AWorldDivisionBase* WorldDivision)
{
	if (not IsValid(WorldDivision))
	{
		return;
	}

	RefreshDivisionInfluence(M_CurrentGameDifficulty);
	CacheDivisionSaveState();

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
	 * state into the world save payload, then commit the campaign save.
	 */
	RefreshDivisionInfluence(M_ActiveMovementDifficulty);
	CacheDivisionSaveState();

	if (GetIsValidWorldPlayerController())
	{
		UWorldStateAndSaveManager* WorldStateAndSaveManager =
			M_WorldPlayerController->GetWorldStateAndSaveManager();
		if (IsValid(WorldStateAndSaveManager))
		{
			WorldStateAndSaveManager->SaveCampaignState();
		}
	}

	M_ActiveMovingTurn = EWorldTurnType::None;
	M_ActiveTurnMovementCount = 0;
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
