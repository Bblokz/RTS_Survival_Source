// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

#include "DrawDebugHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"

namespace
{
	FString GetGenerationStepName(ECampaignGenerationStep GenerationStep)
	{
		const UEnum* StepEnum = StaticEnum<ECampaignGenerationStep>();
		if (not StepEnum)
		{
			return TEXT("UnknownStep");
		}

		return StepEnum->GetNameStringByValue(static_cast<int64>(GenerationStep));
	}

	void LogMissingMapping(const FString& MappingName, const FString& EnumName, const AAnchorPoint* AnchorPoint,
	                       ECampaignGenerationStep GenerationStep)
	{
		const FString AnchorName = IsValid(AnchorPoint) ? AnchorPoint->GetName() : TEXT("InvalidAnchor");
		const FString AnchorKey = AnchorPoint ? AnchorPoint->GetAnchorKey().ToString() : TEXT("InvalidKey");
		const FString StepName = GetGenerationStepName(GenerationStep);
		const FString Message = FString::Printf(
			TEXT("WorldCampaign: Missing %s mapping for %s. AnchorKey=%s AnchorName=%s Step=%s"),
			*MappingName,
			*EnumName,
			*AnchorKey,
			*AnchorName,
			*StepName);
		RTSFunctionLibrary::PrintString(Message, FColor::Red);
	}
}

AAnchorPoint::AAnchorPoint()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAnchorPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	EnsureAnchorKeyIsInitialized();
}

void AAnchorPoint::PostLoad()
{
	Super::PostLoad();

	EnsureAnchorKeyIsInitialized();
}

int32 AAnchorPoint::GetConnectionCount() const
{
	return M_Connections.Num();
}

void AAnchorPoint::ClearConnections()
{
	M_Connections.Reset();
	M_NeighborAnchors.Reset();
}

void AAnchorPoint::AddConnection(AConnection* Connection, AAnchorPoint* NeighborAnchor)
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not M_Connections.Contains(Connection))
	{
		M_Connections.Add(Connection);
	}

	if (not IsValid(NeighborAnchor))
	{
		return;
	}

	if (not M_NeighborAnchors.Contains(NeighborAnchor))
	{
		M_NeighborAnchors.Add(NeighborAnchor);
	}
}

void AAnchorPoint::SortNeighborsByKey()
{
	M_NeighborAnchors.Sort([](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
	{
		if (not IsValid(Left))
		{
			return false;
		}

		if (not IsValid(Right))
		{
			return true;
		}

		return IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
	});
}

bool AAnchorPoint::IsAnchorKeyLess(const FGuid& Left, const FGuid& Right)
{
	if (Left.A != Right.A)
	{
		return Left.A < Right.A;
	}

	if (Left.B != Right.B)
	{
		return Left.B < Right.B;
	}

	if (Left.C != Right.C)
	{
		return Left.C < Right.C;
	}

	return Left.D < Right.D;
}

void AAnchorPoint::DebugDrawConnectionTo(const AAnchorPoint* OtherAnchor, const FColor& Color, float Duration) const
{
	if (not IsValid(OtherAnchor))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	constexpr float DebugLineThickness = 6.f;
	DrawDebugLine(World, GetActorLocation(), OtherAnchor->GetActorLocation(), Color, false, Duration, 0, DebugLineThickness);
}

void AAnchorPoint::InitializeCampaignSettings(const UWorldCampaignSettings* Settings)
{
	M_WorldCampaignSettings = Settings;
}

AWorldMapObject* AAnchorPoint::OnEnemyItemPromotion(EMapEnemyItem EnemyItemType, ECampaignGenerationStep GenerationStep)
{
	if (not GetIsValidWorldCampaignSettings())
	{
		return nullptr;
	}

	const UWorldCampaignSettings* WorldCampaignSettings = M_WorldCampaignSettings.Get();
	const TSubclassOf<AWorldEnemyObject>* EnemyClass = WorldCampaignSettings
		                                                   ? WorldCampaignSettings->EnemyObjectClassByType.Find(
			                                                   EnemyItemType)
		                                                   : nullptr;
	if (not EnemyClass || not IsValid(EnemyClass->Get()))
	{
		const UEnum* EnemyEnum = StaticEnum<EMapEnemyItem>();
		const FString EnumName = EnemyEnum
			                         ? EnemyEnum->GetNameStringByValue(static_cast<int64>(EnemyItemType))
			                         : TEXT("UnknownEnemyItem");
		LogMissingMapping(TEXT("EnemyObjectClassByType"), EnumName, this, GenerationStep);
		return nullptr;
	}

	RemovePromotedWorldObject();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy promotion failed: world was invalid."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	AWorldEnemyObject* SpawnedObject = World->SpawnActor<AWorldEnemyObject>(*EnemyClass, GetActorTransform(),
	                                                                        SpawnParameters);
	if (not IsValid(SpawnedObject))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy promotion failed: failed to spawn world object."));
		return nullptr;
	}

	SpawnedObject->InitializeForAnchor(this, EnemyItemType);
	M_PromotedWorldObject = SpawnedObject;
	return SpawnedObject;
}

AWorldMapObject* AAnchorPoint::OnNeutralItemPromotion(EMapNeutralObjectType NeutralObjectType,
                                                      ECampaignGenerationStep GenerationStep)
{
	if (not GetIsValidWorldCampaignSettings())
	{
		return nullptr;
	}

	const UWorldCampaignSettings* WorldCampaignSettings = M_WorldCampaignSettings.Get();
	const TSubclassOf<AWorldNeutralObject>* NeutralClass = WorldCampaignSettings
		                                                       ? WorldCampaignSettings->NeutralObjectClassByType.Find(
			                                                       NeutralObjectType)
		                                                       : nullptr;
	if (not NeutralClass || not IsValid(NeutralClass->Get()))
	{
		const UEnum* NeutralEnum = StaticEnum<EMapNeutralObjectType>();
		const FString EnumName = NeutralEnum
			                         ? NeutralEnum->GetNameStringByValue(static_cast<int64>(NeutralObjectType))
			                         : TEXT("UnknownNeutralObject");
		LogMissingMapping(TEXT("NeutralObjectClassByType"), EnumName, this, GenerationStep);
		return nullptr;
	}

	RemovePromotedWorldObject();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Neutral promotion failed: world was invalid."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	AWorldNeutralObject* SpawnedObject = World->SpawnActor<AWorldNeutralObject>(*NeutralClass, GetActorTransform(),
	                                                                            SpawnParameters);
	if (not IsValid(SpawnedObject))
	{
		RTSFunctionLibrary::ReportError(TEXT("Neutral promotion failed: failed to spawn world object."));
		return nullptr;
	}

	SpawnedObject->InitializeForAnchor(this, NeutralObjectType);
	M_PromotedWorldObject = SpawnedObject;
	return SpawnedObject;
}

AWorldMapObject* AAnchorPoint::OnMissionPromotion(EMapMission MissionType, ECampaignGenerationStep GenerationStep)
{
	if (not GetIsValidWorldCampaignSettings())
	{
		return nullptr;
	}

	const UWorldCampaignSettings* WorldCampaignSettings = M_WorldCampaignSettings.Get();
	const TSubclassOf<AWorldMissionObject>* MissionClass = WorldCampaignSettings
		                                                       ? WorldCampaignSettings->MissionObjectClassByType.Find(
			                                                       MissionType)
		                                                       : nullptr;
	if (not MissionClass || not IsValid(MissionClass->Get()))
	{
		const UEnum* MissionEnum = StaticEnum<EMapMission>();
		const FString EnumName = MissionEnum
			                         ? MissionEnum->GetNameStringByValue(static_cast<int64>(MissionType))
			                         : TEXT("UnknownMission");
		LogMissingMapping(TEXT("MissionObjectClassByType"), EnumName, this, GenerationStep);
		return nullptr;
	}

	RemovePromotedWorldObject();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Mission promotion failed: world was invalid."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	AWorldMissionObject* SpawnedObject = World->SpawnActor<AWorldMissionObject>(*MissionClass, GetActorTransform(),
	                                                                            SpawnParameters);
	if (not IsValid(SpawnedObject))
	{
		RTSFunctionLibrary::ReportError(TEXT("Mission promotion failed: failed to spawn world object."));
		return nullptr;
	}

	SpawnedObject->InitializeForAnchor(this, MissionType);
	M_PromotedWorldObject = SpawnedObject;
	return SpawnedObject;
}

AWorldMapObject* AAnchorPoint::OnPlayerItemPromotion(EMapPlayerItem PlayerItemType,
                                                     ECampaignGenerationStep GenerationStep)
{
	if (not GetIsValidWorldCampaignSettings())
	{
		return nullptr;
	}

	const UWorldCampaignSettings* WorldCampaignSettings = M_WorldCampaignSettings.Get();
	const TSubclassOf<AWorldPlayerObject>* PlayerClass = WorldCampaignSettings
		                                                     ? WorldCampaignSettings->PlayerObjectClassByType.Find(
			                                                     PlayerItemType)
		                                                     : nullptr;
	if (not PlayerClass || not IsValid(PlayerClass->Get()))
	{
		const UEnum* PlayerEnum = StaticEnum<EMapPlayerItem>();
		const FString EnumName = PlayerEnum
			                         ? PlayerEnum->GetNameStringByValue(static_cast<int64>(PlayerItemType))
			                         : TEXT("UnknownPlayerItem");
		LogMissingMapping(TEXT("PlayerObjectClassByType"), EnumName, this, GenerationStep);
		return nullptr;
	}

	RemovePromotedWorldObject();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player promotion failed: world was invalid."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	AWorldPlayerObject* SpawnedObject = World->SpawnActor<AWorldPlayerObject>(*PlayerClass, GetActorTransform(),
	                                                                          SpawnParameters);
	if (not IsValid(SpawnedObject))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player promotion failed: failed to spawn world object."));
		return nullptr;
	}

	SpawnedObject->InitializeForAnchor(this, PlayerItemType);
	M_PromotedWorldObject = SpawnedObject;
	return SpawnedObject;
}

void AAnchorPoint::RemovePromotedWorldObject()
{
	AWorldMapObject* PromotedWorldObject = M_PromotedWorldObject.Get();
	if (not IsValid(PromotedWorldObject))
	{
		M_PromotedWorldObject.Reset();
		return;
	}

	PromotedWorldObject->Destroy();
	M_PromotedWorldObject.Reset();
}

bool AAnchorPoint::GetHasPromotedWorldObject() const
{
	AWorldMapObject* PromotedWorldObject = M_PromotedWorldObject.Get();
	return IsValid(PromotedWorldObject);
}

AWorldMapObject* AAnchorPoint::GetPromotedWorldObject() const
{
	return M_PromotedWorldObject.Get();
}

void AAnchorPoint::EnsureAnchorKeyIsInitialized()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (M_AnchorKey.IsValid())
	{
		return;
	}

	M_AnchorKey = FGuid::NewGuid();
}

bool AAnchorPoint::GetIsValidWorldCampaignSettings() const
{
	if (not IsValid(M_WorldCampaignSettings.Get()))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			TEXT("M_WorldCampaignSettings"),
			TEXT("GetIsValidWorldCampaignSettings"),
			this
		);

		return false;
	}

	return true;
}
