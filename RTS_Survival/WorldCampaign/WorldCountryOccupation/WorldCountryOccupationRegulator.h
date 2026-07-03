// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "UObject/ObjectKey.h"
#include "WorldCountryOccupationRegulator.generated.h"

class AAnchorPoint;
class AGeneratorWorldCampaign;
class AStaticMeshActor;
class UMaterialInterface;
class UStaticMeshComponent;
struct FWorldCampaignPlacementState;

/**
 * @brief Designer-authored country visuals used to connect generated anchors to world-map regions.
 */
USTRUCT(BlueprintType)
struct FWorldCountryOccupationActorPair
{
	GENERATED_BODY()

	/** Country fill mesh whose bounds drive anchor ownership and whose material slot 0 changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation")
	TWeakObjectPtr<AStaticMeshActor> M_CountryMeshActor;

	/** Border mesh kept with the country so later border material changes can use the same lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation")
	TWeakObjectPtr<AStaticMeshActor> M_BorderMeshActor;
};

/**
 * @brief Added to the world generator actor to cache country ownership and update country materials.
 *
 * @note The component is found by AGeneratorWorldCampaign::PostInitializeComponents.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldCountryOccupationRegulator : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * @brief Creates a non-ticking regulator; runtime state is built only after generation finishes.
	 */
	UWorldCountryOccupationRegulator();

	/**
	 * @brief Builds country-anchor lookup tables after generated map objects exist.
	 * @param WorldGenerator Generator that owns the authoritative placement state.
	 */
	void InitializeCountryOccupation(AGeneratorWorldCampaign* WorldGenerator);

	/**
	 * @brief Re-evaluates one country after a map item changes so visual ownership can update cheaply.
	 * @param AnchorPoint Anchor whose item type changed.
	 * @param MapItemType New high-level item type at the supplied anchor.
	 */
	void UpdateCountryOccupationForAnchor(AAnchorPoint* AnchorPoint, EMapItemType MapItemType);

private:
	/**
	 * @brief Internal visual result for one country after evaluating all mapped anchors.
	 */
	enum class EWorldCountryOccupationState : uint8
	{
		/** Empty and neutral anchors only. */
		Neutral,

		/** Enemy or mission anchors own the country under the Soviet-side rule. */
		SovietOccupied,

		/** Player anchors own the country under the resistance-side rule. */
		ResistanceOccupied,

		/** Multiple ownership families are present; keep the existing material. */
		Mixed
	};

	/**
	 * @brief Clears transient lookup data so regenerated or loaded worlds do not reuse stale mappings.
	 */
	void ResetRuntimeMappings();

	/**
	 * @brief Registers configured country actors that can participate in anchor ownership checks.
	 */
	void InitializeRuntimeCountryEntries();

	/**
	 * @brief Assigns generated anchors to configured countries before material evaluation begins.
	 * @param PlacementState Generated state that provides the current anchor set.
	 */
	void InitializeAnchorMappings(const FWorldCampaignPlacementState& PlacementState);

	/**
	 * @brief Captures current item ownership so later country checks do not need to inspect actors repeatedly.
	 * @param PlacementState Fallback data for anchors whose promoted actor is not available.
	 */
	void InitializeItemTypeCache(const FWorldCampaignPlacementState& PlacementState);

	/**
	 * @brief Applies the generation-time rule where any Soviet-side item claims the country visually.
	 */
	void ApplyInitialCountryMaterials();

	/**
	 * @brief Recomputes one country after a known anchor item type changed.
	 * @param CountryIndex Configured country index to re-evaluate.
	 */
	void ReevaluateCountryOccupation(int32 CountryIndex);

	/**
	 * @brief Stores both lookup directions once an anchor has been assigned to a country.
	 * @param AnchorPoint Anchor that belongs to the country.
	 * @param CountryIndex Configured country index containing the anchor.
	 */
	void CacheAnchorCountryMapping(AAnchorPoint* AnchorPoint, int32 CountryIndex);

	/**
	 * @brief Applies the resolved occupation state to the country fill mesh material.
	 * @param CountryIndex Configured country index to update.
	 * @param OccupationState State that chooses the material, or Mixed to preserve the current material.
	 */
	void ApplyCountryOccupationState(int32 CountryIndex, EWorldCountryOccupationState OccupationState);

	/**
	 * @brief Uses the startup occupation rule where any enemy or mission anchor marks Soviet control.
	 * @param CountryAnchors Anchors mapped to one country.
	 * @return Initial visual occupation for the country.
	 */
	EWorldCountryOccupationState DetermineInitialCountryOccupationState(
		const TArray<TWeakObjectPtr<AAnchorPoint>>& CountryAnchors) const;

	/**
	 * @brief Uses full-country rules for later occupation updates after an anchor changes.
	 * @param CountryAnchors Anchors mapped to one country.
	 * @return Resolved occupation, or Mixed when no requested material rule applies.
	 */
	EWorldCountryOccupationState DetermineCountryOccupationState(
		const TArray<TWeakObjectPtr<AAnchorPoint>>& CountryAnchors) const;

	/**
	 * @brief Reads promoted world objects first so save/load and generation paths share item detection.
	 * @param AnchorPoint Anchor whose current item type is needed.
	 * @param PlacementState Fallback placement maps when no promoted object is available.
	 * @return High-level map item category at the anchor.
	 */
	EMapItemType GetInitialMapItemTypeForAnchor(const AAnchorPoint* AnchorPoint,
	                                            const FWorldCampaignPlacementState& PlacementState) const;

	/**
	 * @brief Reads the cached item type used by country occupation checks.
	 * @param AnchorPoint Anchor whose cached type is needed.
	 * @return Cached type, or Empty when the anchor is not cached.
	 */
	EMapItemType GetCachedMapItemTypeForAnchor(const AAnchorPoint* AnchorPoint) const;

	/**
	 * @brief Converts a country occupation state into its configured material.
	 * @param OccupationState State to visualize.
	 * @return Material to apply, or nullptr when the state should not change the current material.
	 */
	UMaterialInterface* GetMaterialForOccupationState(EWorldCountryOccupationState OccupationState) const;

	/**
	 * @brief Returns the configured actor so actor-keyed lookup maps stay stable.
	 * @param CountryIndex Configured country index.
	 * @return Country actor, or nullptr when the index or actor is invalid.
	 */
	AStaticMeshActor* GetCountryMeshActorForIndex(int32 CountryIndex) const;

	/**
	 * @brief Returns the actor's mesh component for bounds and material operations.
	 * @param CountryIndex Configured country index.
	 * @return Static mesh component, or nullptr when the country actor is invalid.
	 */
	UStaticMeshComponent* GetCountryMeshForIndex(int32 CountryIndex) const;

	/**
	 * @brief Finds anchors already mapped to one country without rebuilding ownership.
	 * @param CountryIndex Configured country index.
	 * @return Anchor list for the country, or nullptr when the country has no runtime entry.
	 */
	const TArray<TWeakObjectPtr<AAnchorPoint>>* FindCountryAnchorsByIndex(int32 CountryIndex) const;

	/**
	 * @brief Finds the best country for an anchor using country bounds and deterministic overlap resolution.
	 * @param AnchorPoint Anchor to classify.
	 * @param OutCountryIndex Configured country index when a containing country is found.
	 * @return true when an owning country was found.
	 */
	bool TryFindCountryIndexForAnchor(const AAnchorPoint* AnchorPoint, int32& OutCountryIndex) const;

	/**
	 * @brief Tests anchor XY against an expanded country bounds without requiring collision.
	 * @param CountryMesh Mesh component whose world bounds are tested.
	 * @param AnchorPoint Anchor to classify.
	 * @return true when the anchor lies inside the bounds in XY.
	 */
	bool GetDoesCountryMeshBoundsContainAnchor(const UStaticMeshComponent* CountryMesh,
	                                           const AAnchorPoint* AnchorPoint) const;

	/**
	 * @brief Scores overlapping country bounds so tighter bounds win ambiguous anchor ownership.
	 * @param CountryMesh Mesh component to score.
	 * @return XY bounds area, or max float for invalid meshes.
	 */
	float GetCountryMeshBoundsArea2D(const UStaticMeshComponent* CountryMesh) const;

	/**
	 * @brief Guards configured country index lookups before array access.
	 * @param CountryIndex Configured country index to test.
	 * @return true when the index can be used with M_CountryMeshActorPairs.
	 */
	bool GetIsCountryIndexValid(int32 CountryIndex) const;

	/**
	 * @brief Validates designer setup for the country actor in one configured pair.
	 * @param ActorPair Configured country/border pair.
	 * @param CountryIndex Index used in logs.
	 * @param FunctionName Calling function used in error reports.
	 * @return true when the country actor can be used.
	 */
	bool GetIsValidCountryMeshActor(const FWorldCountryOccupationActorPair& ActorPair, int32 CountryIndex,
	                                const FString& FunctionName) const;

	/**
	 * @brief Validates that a configured country actor exposes a mesh component.
	 * @param CountryMesh Mesh component pulled from the country actor.
	 * @param CountryIndex Index used in logs.
	 * @param FunctionName Calling function used in error reports.
	 * @return true when bounds/material operations can use the component.
	 */
	bool GetIsValidCountryMeshComponent(const UStaticMeshComponent* CountryMesh, int32 CountryIndex,
	                                    const FString& FunctionName) const;

	/**
	 * @brief Validates the material used for Soviet-side country control.
	 * @return true when the material can be applied to a country mesh.
	 */
	bool GetIsValidSovietOccupiedMaterial() const;

	/**
	 * @brief Validates the material used for resistance-side country control.
	 * @return true when the material can be applied to a country mesh.
	 */
	bool GetIsValidResistanceMaterial() const;

	/**
	 * @brief Validates the material used for empty or neutral country control.
	 * @return true when the material can be applied to a country mesh.
	 */
	bool GetIsValidNeutralMaterial() const;

	/** Designer-provided country actors and paired border actors placed on the campaign map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation",
		meta = (AllowPrivateAccess = "true", DisplayName = "Country Mesh Actor Pairs"))
	TArray<FWorldCountryOccupationActorPair> M_CountryMeshActorPairs;

	/** Material used when a country is visually controlled by enemy or mission anchors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation",
		meta = (AllowPrivateAccess = "true", DisplayName = "Soviet Occupied Material"))
	TObjectPtr<UMaterialInterface> M_SovietOccupiedMaterial = nullptr;

	/** Material used when a country is visually controlled by player anchors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation",
		meta = (AllowPrivateAccess = "true", DisplayName = "Resistance Material"))
	TObjectPtr<UMaterialInterface> M_ResistanceMaterial = nullptr;

	/** Material used when a country is empty or contains only neutral anchors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation",
		meta = (AllowPrivateAccess = "true", DisplayName = "Neutral Material"))
	TObjectPtr<UMaterialInterface> M_NeutralMaterial = nullptr;

	/** Extra XY tolerance for anchors that sit exactly on a country actor's bounds edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Country Bounds Padding XY"))
	float M_CountryBoundsPaddingXY;

	/** Anchor key lookup used by later single-anchor update calls. */
	TMap<FGuid, int32> M_CountryIndexByAnchorKey;

	/** Reverse lookup requested by gameplay: country actor to anchors inside that country. */
	TMap<TObjectKey<AStaticMeshActor>, TArray<TWeakObjectPtr<AAnchorPoint>>> M_AnchorsByCountryMeshActor;

	/** Current high-level anchor item state used to re-evaluate one country cheaply. */
	TMap<FGuid, EMapItemType> M_CurrentItemTypeByAnchorKey;

	/** Valid configured country indices; invalid actor pairs are omitted once during initialization. */
	TArray<int32> M_TraceableCountryIndices;
};
