// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "AnchorPoint.generated.h"

class AConnection;

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
	int32 GetConnectionCount() const;

	const TArray<TObjectPtr<AAnchorPoint>>& GetNeighborAnchors() const { return M_NeighborAnchors; }
	const TArray<TObjectPtr<AConnection>>& GetConnections() const { return M_Connections; }

	void ClearConnections();
	void AddConnection(AConnection* Connection, AAnchorPoint* NeighborAnchor);
	void SortNeighborsByKey();

	static bool IsAnchorKeyLess(const FGuid& Left, const FGuid& Right);

	void DebugDrawAnchorState(const FString& Label, const FColor& Color, float Duration) const;
	void DebugDrawConnectionTo(const AAnchorPoint* OtherAnchor, const FColor& Color, float Duration) const;

	UFUNCTION(BlueprintCallable, Category = "World Campaign|Anchor")
	void OnEnemyItemPromotion(EMapEnemyItem EnemyItemType);

	UFUNCTION(BlueprintCallable, Category = "World Campaign|Anchor")
	void OnNeutralItemPromotion(EMapNeutralObjectType NeutralObjectType);

	UFUNCTION(BlueprintCallable, Category = "World Campaign|Anchor")
	void OnMissionPromotion(EMapMission MissionType);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Anchor")
	void BP_OnEnemyItemPromotion(EMapEnemyItem EnemyItemType);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Anchor")
	void BP_OnNeutralItemPromotion(EMapNeutralObjectType NeutralObjectType);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Anchor")
	void BP_OnMissionPromotion(EMapMission MissionType);

private:
	void EnsureAnchorKeyIsInitialized();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	FGuid M_AnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AConnection>> M_Connections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AAnchorPoint>> M_NeighborAnchors;
};
