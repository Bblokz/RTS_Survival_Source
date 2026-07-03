// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/EnemyMapItemUIData/FEnemyMapItemUIData.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"
#include "WorldMissionObject.generated.h"

/**
 * @brief Spawned marker for mission items placed on anchors.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldMissionObject : public AWorldMapObject
{
	GENERATED_BODY()

public:
	AWorldMissionObject();

	void InitializeForAnchorWithMissionType(AAnchorPoint* AnchorPoint, EMapMission MissionType);

	EMapMission GetMissionType() const { return M_MissionType; }
	FEnemyOrMissionMapItemUIData GetMapItemUIData() const;
	const FPrimaryReward& GetPrimaryReward() const { return M_PrimaryReward; }
	const FSecondaryReward& GetSecondaryReward() const { return M_SecondaryReward; }

	/**
	 * @brief Gets the runtime/save-game fortification modifier component for this mission object.
	 * @return Component that stores EWorldFortificationStrength modifiers.
	 */
	UWorldFortificationModificationsComponent* GetFortificationModificationsComponent() const
	{
		return M_FortificationModificationsComponent;
	}

	/**
	 * @brief Gets the base fortification strength percentage cached on the shared strength component.
	 * @return Base fortification strength percentage for this mission object.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Difficulty")
	int32 GetBaseDifficultyPercentage() const;

	/**
	 * @brief Resets the strategic support category on the shared strength component.
	 */
	void ResetStrategicReport();

	/**
	 * @brief Adds one strategic support reason to the shared strength component.
	 * @param StrengthReason Strategic support reason to add.
	 */
	void AddStrategicSupportReason(const FWorldStrengthReason& StrengthReason);

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EMapMission M_MissionType = EMapMission::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FEnemyOrMissionMapItemUIData M_MapItemUIData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Fortification Modifications",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldFortificationModificationsComponent> M_FortificationModificationsComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FPrimaryReward M_PrimaryReward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FSecondaryReward M_SecondaryReward;

	/**
	 * @brief Builds a UI data copy with the current strength report projected from the shared component.
	 * @return Mission map item UI data for the current object state.
	 */
	FEnemyOrMissionMapItemUIData BuildMapItemUIData() const;
};
