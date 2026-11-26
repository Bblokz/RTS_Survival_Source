// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowTypes.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/EnemyMapItemUIData/FEnemyMapItemUIData.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"
#include "WorldMissionObject.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UWorldUnknownMissionMarkerComponent;

/** What the unknown marker changed on one component of the mission piece, so it can be put back. */
USTRUCT()
struct FWorldMissionFogHiddenComponent
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> Component = nullptr;

	bool bWasVisible = false;

	// Only primitives have collision; lights and scene components keep this false.
	bool bHadCollision = false;

	TEnumAsByte<ECollisionEnabled::Type> CollisionBeforeFog = ECollisionEnabled::NoCollision;
};

/**
 * @brief Spawned marker for mission items placed on anchors. While the fog of war fully hides it, the fog manager
 * gives it a presentation (UWorldFogHintSettings): its real piece seen through an opening in the fog, or a plain
 * unknown marker above the fog instead of its piece. Either way it stays unidentified: no name, no click.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldMissionObject : public AWorldMapObject
{
	GENERATED_BODY()

public:
	AWorldMissionObject();

	/**
	 * @brief Called by the fog manager on every fog pass: switches between the piece and the plain marker.
	 * @param NewPresentation What the fog hint rules decided for this pass.
	 */
	void ApplyFogPresentation(EWorldMissionFogPresentation NewPresentation);

	EWorldMissionFogPresentation GetFogPresentation() const { return M_FogPresentation; }

	/** @return true while the piece is hidden and the plain unknown marker stands in for it. */
	bool GetIsShownAsUnknownMarker() const { return M_FogPresentation == EWorldMissionFogPresentation::PlainMarker; }

	/** @return true while the fog hides what this mission is: hovering it shows the unidentified tag only. */
	bool GetIsUnidentifiedUnderFog() const { return M_FogPresentation != EWorldMissionFogPresentation::AsFogState; }

	/** @return The unknown marker, or nullptr when it was never needed. */
	UWorldUnknownMissionMarkerComponent* GetUnknownMarker() const { return M_UnknownMarker; }

	void InitializeForAnchorWithMissionType(AAnchorPoint* AnchorPoint, EMapMission MissionType);

	EMapMission GetMissionType() const { return M_MissionType; }

	/** @return The mission's name on the campaign map; empty for a mission that has none configured. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Map Item")
	FText GetMissionDisplayName() const;
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

protected:
	virtual void OnClearedByPlayer() override;

private:
	/** @return The marker, created on first use; nullptr when its mesh is missing. */
	UWorldUnknownMissionMarkerComponent* GetOrCreateUnknownMarker();

	void ShowUnknownMarker();
	void ShowRealPiece();
	void HidePieceComponentForFog(USceneComponent* PieceComponent);

	// Stands the tall landmark of this mission type beside the piece, from UWorldFogHintSettings; none for most types.
	void CreateLandmark();

	UPROPERTY()
	TObjectPtr<UWorldUnknownMissionMarkerComponent> M_UnknownMarker;

	// Tagged WorldLandmark, so the fog never hides it with the piece.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_Landmark;

	// Every piece component the marker hid or made cursor-proof; put back when the real piece shows again.
	UPROPERTY()
	TArray<FWorldMissionFogHiddenComponent> M_ComponentsHiddenForFog;

	EWorldMissionFogPresentation M_FogPresentation = EWorldMissionFogPresentation::AsFogState;

	// The marker mesh is missing: reported once, then the piece stays as it was.
	bool bM_HasUnknownMarkerFailed = false;

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
