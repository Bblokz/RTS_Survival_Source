// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/GenerationStep/Enum_CampaignGenerationStep.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapPlayerItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "AnchorPoint.generated.h"

class AConnection;
class AWorldMapObject;
class UWorldCampaignSettings;

/**
 * @brief Anchor actors are placed on the campaign map and serve as stable nodes for generated connections.
 */
UCLASS()
class RTS_SURVIVAL_API AAnchorPoint : public AActor
{
	GENERATED_BODY()

public:
	AAnchorPoint();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;

	FGuid GetAnchorKey() const { return M_AnchorKey; }

	/**
	 * @brief Sets a deterministic key before construction so generated anchors sort consistently.
	 * @param InAnchorKey Key to assign for this anchor.
	 * @param bAllowOverwrite Whether to replace an existing key.
	 */
	void SetAnchorKey(const FGuid& InAnchorKey, bool bAllowOverwrite = false);
	int32 GetConnectionCount() const;

	const TArray<TObjectPtr<AAnchorPoint>>& GetNeighborAnchors() const { return M_NeighborAnchors; }
	const TArray<TObjectPtr<AConnection>>& GetConnections() const { return M_Connections; }

	void ClearConnections();
	void AddConnection(AConnection* Connection, AAnchorPoint* NeighborAnchor);
	void SortNeighborsByKey();

	static bool IsAnchorKeyLess(const FGuid& Left, const FGuid& Right);

	void DebugDrawConnectionTo(const AAnchorPoint* OtherAnchor, const FColor& Color, float Duration) const;

	void InitializeCampaignSettings(const UWorldCampaignSettings* Settings);

	AWorldMapObject* OnEnemyItemPromotion(EMapEnemyItem EnemyItemType, ECampaignGenerationStep GenerationStep);
	AWorldMapObject* OnNeutralItemPromotion(EMapNeutralObjectType NeutralObjectType, ECampaignGenerationStep GenerationStep);
	AWorldMapObject* OnMissionPromotion(EMapMission MissionType, ECampaignGenerationStep GenerationStep);
	AWorldMapObject* OnPlayerItemPromotion(EMapPlayerItem PlayerItemType, ECampaignGenerationStep GenerationStep);

	void RemovePromotedWorldObject();
	bool GetHasPromotedWorldObject() const;
	AWorldMapObject* GetPromotedWorldObject() const;

private:
	void EnsureAnchorKeyIsInitialized();
	bool GetIsValidWorldCampaignSettings() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	FGuid M_AnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AConnection>> M_Connections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AAnchorPoint>> M_NeighborAnchors;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<const UWorldCampaignSettings> M_WorldCampaignSettings;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AWorldMapObject> M_PromotedWorldObject;
};
