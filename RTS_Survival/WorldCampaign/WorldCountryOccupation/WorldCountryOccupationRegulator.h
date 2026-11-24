// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryLiberationRules.h"
#include "UObject/ObjectKey.h"
#include "WorldCountryOccupationRegulator.generated.h"

class AAnchorPoint;
class AGeneratorWorldCampaign;
class AStaticMeshActor;
class UTexture2D;
class AWorldPlayerController;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UWorldCampaignProgressManager;
struct FWorldCampaignPlacementState;

/** A country's liberation value changed; build nodes and UI listen to this. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldCountryLiberationChanged, FName, CountryId, float, Liberation);

/**
 * @brief Runtime liberation state of one country. Rebuilt from the generated anchors and the saved cleared anchor
 * keys, so nothing here is saved.
 */
USTRUCT()
struct FWorldCountryLiberationData
{
	GENERATED_BODY()

	FName CountryId = NAME_None;

	/** Index into the regulator's runtime country list. */
	int32 CountryIndex = INDEX_NONE;

	FWorldCountryTargetTally TargetTally;

	// -1 until the first evaluation so the first real value is always broadcast.
	float Liberation = -1.f;

	/** One dynamic instance per country, parented to the Soviet material; created on first use. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LiberationMaterial = nullptr;

	/** Staff-map fill of this country; used instead of LiberationMaterial when the staff-map look is on. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StaffMapMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BorderMaterial = nullptr;

	/** Outline mesh that frames this country in the level; found once by mesh name. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AStaticMeshActor> BorderMeshActor;

	bool bHasSearchedBorderMeshActor = false;

	/** Distance of every point of the country to its edge; the fill material draws the border band from it. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> EdgeDistanceTexture = nullptr;

	bool bHasBuiltEdgeDistanceTexture = false;

	// False until the country was drawn once: its saved state appears at once, later clears grow outward.
	bool bHasShownFrontOnce = false;

	bool bIsFrontGrowing = false;

	// World time at which each cleared anchor's free ground started to spread.
	TMap<FGuid, double> FrontGrowStartSecondsByAnchorKey;
};

/**
 * @brief One cleared target of a country as the fill material draws it: free ground spreading from a point.
 */
struct FWorldCountryFrontPoint
{
	FGuid AnchorKey;
	FVector2D MapLocation = FVector2D::ZeroVector;
	float Radius = 0.f;
};

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
 * @brief Added to the world generator actor to map anchors to countries, derive each country's liberation value from
 * the cleared anchors and blend the country material from the Soviet look toward the Resistance look.
 * Call InitializeCampaignProgress once with the world player controller so cleared anchors update the countries.
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

	/**
	 * @brief Reads cleared anchors from the controller's progress manager and re-evaluates a country whenever one of
	 * its anchors is cleared. Safe to call before or after InitializeCountryOccupation, and more than once.
	 * @param WorldPlayerController Controller that owns the UWorldCampaignProgressManager.
	 */
	void InitializeCampaignProgress(AWorldPlayerController* WorldPlayerController);

	/**
	 * @brief Liberation of a country: cleared targets over all targets, story missions weighted.
	 * @return 0..1; 1 for a country without targets; 0 for an unknown id.
	 */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Country Occupation")
	float GetCountryLiberation(FName CountryId) const;

	/** @return Country of the anchor, or NAME_None when the anchor lies outside every configured country. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Country Occupation")
	FName GetCountryIdForAnchorKey(const FGuid& AnchorKey) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Country Occupation")
	TArray<FName> GetCountryIds() const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Country Occupation")
	int32 GetClearedTargetCount(FName CountryId) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Country Occupation")
	int32 GetTargetCount(FName CountryId) const;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Country Occupation")
	FOnWorldCountryLiberationChanged OnCountryLiberationChanged;

	/** @return Country whose shape contains the map location, or NAME_None over sea and unlisted countries. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Country Occupation")
	FName GetCountryIdAtMapLocation(FVector2D MapLocation) const;

	/**
	 * @brief Brightens the country under the cursor and its border; the controller calls it every frame.
	 * @param MapLocation Cursor position on the map plane.
	 * @param bIsCursorOverMap false clears the highlight (cursor over UI or off the map).
	 */
	void SetHoveredCountryAtMapLocation(FVector2D MapLocation, bool bIsCursorOverMap);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	 * @return true when the index can be used with M_RuntimeCountryMeshActors.
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

	bool GetIsValidWorldCampaignProgressManager() const;

	/** Native OnAnchorCleared handler: re-evaluates only the country that holds the anchor. */
	void OnAnchorCleared(const FGuid& AnchorKey);

	void UnbindFromCampaignProgress();

	/**
	 * @brief Creates one liberation entry per valid country with its stable id.
	 */
	void InitializeCountryLiberationData();

	/**
	 * @brief Stable id: the Countries config entry whose CountryMeshActorName matches the actor, else the actor FName.
	 * @param CountryIndex Configured country index.
	 * @return Country id, or NAME_None when the country actor is invalid.
	 */
	FName ResolveCountryId(int32 CountryIndex) const;

	void RecomputeAllCountryLiberation();

	/**
	 * @brief Recounts one country's targets, updates its liberation value and look, and broadcasts a change.
	 * @param LiberationDataIndex Index into M_CountryLiberationData.
	 */
	void RecomputeCountryLiberation(int32 LiberationDataIndex);

	/**
	 * @brief Classifies one anchor: missions and enemy items are targets, and so is any anchor the player cleared,
	 * so a cleared target still counts after its object was hidden or removed.
	 * @param AnchorPoint Anchor mapped to the country.
	 * @param ProgressManager Source of cleared anchors, or nullptr before InitializeCampaignProgress.
	 * @return Target flags for the liberation tally.
	 */
	FWorldCountryAnchorTargetInput BuildAnchorTargetInput(const AAnchorPoint* AnchorPoint,
	                                                      const UWorldCampaignProgressManager* ProgressManager) const;

	/** Reads the blendable parameters of the Soviet and Resistance instances once. */
	void CacheLiberationLookParameters();

	/**
	 * @brief Reads the configured parameter names from a material; parameters the material lacks are skipped.
	 * @param Material Material instance to read.
	 * @param OutParameterSet Receives the values found.
	 */
	void ReadMaterialParameterSet(const UMaterialInterface* Material,
	                              FWorldCountryMaterialParameterSet& OutParameterSet) const;

	/**
	 * @brief Neutral material without targets, the blended dynamic instance otherwise, and the old whole-material
	 * swap as fallback when no dynamic instance can be made.
	 * @param LiberationData Country to update.
	 */
	void ApplyCountryLiberationLook(FWorldCountryLiberationData& LiberationData);

	/**
	 * @brief Pushes the liberation blend into the country's dynamic instance.
	 * @param LiberationData Country to update; receives its dynamic instance on first use.
	 * @return false when the blend cannot be shown (no parameters read or no instance could be made).
	 */
	bool TryApplyBlendedLiberationLook(FWorldCountryLiberationData& LiberationData);

	/**
	 * @brief Creates the country's dynamic instance once and makes sure the mesh uses it.
	 * @param LiberationData Country that owns the instance.
	 * @return The instance, or nullptr when the mesh or Soviet material is missing.
	 */
	UMaterialInstanceDynamic* GetOrCreateLiberationMaterial(FWorldCountryLiberationData& LiberationData);

	const FWorldCountryLiberationData* FindLiberationDataByCountryId(FName CountryId) const;

	/** Outcome of testing an anchor against a country's real shape. */
	enum class EWorldCountryShapeTestResult : uint8
	{
		Inside,
		Outside,
		/** No triangle data and no query collision: only the bounds rule can decide for this country. */
		Untestable
	};

	/**
	 * @brief Tests the anchor's XY against the country's flattened mesh triangles; without triangle data a downward
	 * trace is used when the mesh has query collision.
	 * @param CountryIndex Runtime country index (triangle cache).
	 * @param CountryMesh Country fill mesh.
	 * @param AnchorPoint Anchor to classify.
	 * @return Inside, Outside, or Untestable when neither test can run.
	 */
	EWorldCountryShapeTestResult TestCountryShapeForAnchor(int32 CountryIndex, UStaticMeshComponent* CountryMesh,
	                                                       const AAnchorPoint* AnchorPoint) const;

	/**
	 * @brief Country list from the Countries config, matched to level actors by FName; empty config falls back to
	 * the valid entries of M_CountryMeshActorPairs.
	 * @return false when the config has no countries.
	 */
	bool InitializeRuntimeCountryEntries_FromConfig();
	void InitializeRuntimeCountryEntries_FromActorPairs();

	/**
	 * @brief Adds one country actor to the runtime list; a second entry for the same actor is refused.
	 * @param CountryMeshActor Country fill mesh actor in the level.
	 * @param CountryId Stable id of the country.
	 */
	void AddRuntimeCountry(AStaticMeshActor* CountryMeshActor, FName CountryId);

	/**
	 * @brief Reads LOD0 render triangles of the country mesh once and flattens them to world XY. Needs CPU-side mesh
	 * data: always there in the editor, in a cooked build only with bAllowCPUAccess on the mesh.
	 * @param CountryMesh Country fill mesh.
	 * @param OutTriangles Receives the triangles; left empty when the data cannot be read.
	 */
	void BuildCountryTriangles(const UStaticMeshComponent* CountryMesh,
	                           TArray<FWorldCountryTriangle2D>& OutTriangles) const;

	/**
	 * @brief Keeps the candidate with the smaller XY bounds area, so tighter countries win overlaps.
	 * @param CandidateCountryIndex Country that contains the anchor.
	 * @param InOutBestCountryIndex Best country so far; INDEX_NONE when none was found yet.
	 */
	void KeepSmallerBoundsCountry(int32 CandidateCountryIndex, int32& InOutBestCountryIndex) const;

	/**
	 * Designer-provided country actors and paired border actors placed on the campaign map.
	 * Fallback only: the country list comes from UWorldCampaignMapContentSettings::Countries when that is set.
	 * Empty entries are skipped silently.
	 */
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

	/** Vector parameters of M_StripeMaterial blended from the Soviet values to the Resistance values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation|Liberation Look",
		meta = (AllowPrivateAccess = "true", DisplayName = "Blended Vector Parameters"))
	TArray<FName> M_LiberationVectorParameterNames = {
		TEXT("BaseColor"), TEXT("BaseColorB"), TEXT("StripeColor"), TEXT("WideStripeColor")
	};

	/** Scalar parameters of M_StripeMaterial blended from the Soviet values to the Resistance values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation|Liberation Look",
		meta = (AllowPrivateAccess = "true", DisplayName = "Blended Scalar Parameters"))
	TArray<FName> M_LiberationScalarParameterNames = {
		TEXT("StripeIntensity"), TEXT("StripeSpacing"), TEXT("StripeSpeedWorld"), TEXT("StripeWidth01")
	};

	/** Off = the old stripe materials are blended; on = hatched staff-map fill, liberation front and state borders. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation|Staff Map Look",
		meta = (AllowPrivateAccess = "true", DisplayName = "Use Staff Map Look"))
	bool bM_UseStaffMapLook = true;

	/** M_CountryStaffMap: built by Source/Tools/WorldCampaign/create_country_look_materials.py. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation|Staff Map Look",
		meta = (AllowPrivateAccess = "true", DisplayName = "Staff Map Fill Material"))
	TSoftObjectPtr<UMaterialInterface> M_StaffMapFillMaterial;

	/** M_CountryBorderState: one colour for one owner, two alternating colours while contested. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation|Staff Map Look",
		meta = (AllowPrivateAccess = "true", DisplayName = "Border State Material"))
	TSoftObjectPtr<UMaterialInterface> M_BorderStateMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> M_LoadedStaffMapFillMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> M_LoadedBorderStateMaterial = nullptr;

	FTimerHandle M_FrontGrowthTimerHandle;

	// Index into M_CountryLiberationData of the country under the cursor; INDEX_NONE when none.
	int32 M_HoveredLiberationDataIndex = INDEX_NONE;

	void LoadStaffMapLookMaterials();

	/**
	 * @brief Draws the country with the staff-map fill and updates its border.
	 * @return false when the staff-map look is off or its material is missing; the caller then uses the old look.
	 */
	bool TryApplyStaffMapLook(FWorldCountryLiberationData& LiberationData);

	/** Sends the cleared targets of the country to its fill material and starts the growth of new ones. */
	void ApplyCountryFrontPoints(FWorldCountryLiberationData& LiberationData);

	/**
	 * @brief Collects the cleared targets of a country; each frees ground toward the nearest enemy still standing.
	 * @param CountryIndex Runtime country index.
	 * @param OutFrontPoints Receives at most as many points as the fill material can draw, in a stable order.
	 */
	void BuildCountryFrontPoints(int32 CountryIndex, TArray<FWorldCountryFrontPoint>& OutFrontPoints) const;

	void StartFrontGrowthTimer();
	void TickFrontGrowth();

	/** Enemy red, free blue, or cream and red dashes while contested; brighter for the hovered country. */
	void ApplyCountryBorderState(FWorldCountryLiberationData& LiberationData);

	AStaticMeshActor* FindBorderMeshActorForCountry(int32 CountryIndex) const;

	/**
	 * @brief Measures the country's edge distances once and hands them to its fill material, which then draws the
	 * border as a band inside the country. Without triangle data the band stays off and only the outline mesh shows.
	 */
	void ApplyCountryEdgeDistance(FWorldCountryLiberationData& LiberationData);

	/** Rectangle around every anchor of the map; parts of a country outside it (overseas land) are not measured. */
	FBox2D GetCampaignMapRect() const;

	/** Width of the border band inside each country, in map units; 0 turns the band off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation|Staff Map Look",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "60.0", DisplayName = "Border Band Width"))
	float M_BorderBandWidth = 14.f;
	int32 FindLiberationDataIndexAtMapLocation(const FVector2D& MapLocation) const;
	void ApplyHoverToCountry(int32 LiberationDataIndex, bool bIsHovered);

	/** Off = anchors are matched by bounding box only, as before the shape test existed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Country Occupation",
		meta = (AllowPrivateAccess = "true", DisplayName = "Use Shape Containment Test"))
	bool bM_UseShapeContainmentTest = true;

	// Generator whose placement state and story-mission pins classify the anchors.
	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;

	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_WorldCampaignProgressManager;

	FDelegateHandle M_AnchorClearedHandle;

	// Set by InitializeCampaignProgress; before that a missing progress manager is expected and not an error.
	bool bM_IsCampaignProgressBound = false;

	// Set when all three materials are valid; mappings and liberation are built either way.
	bool bM_CanApplyCountryLook = false;

	UPROPERTY(Transient)
	TArray<FWorldCountryLiberationData> M_CountryLiberationData;

	TMap<FName, int32> M_LiberationDataIndexByCountryId;
	TMap<int32, int32> M_LiberationDataIndexByCountryIndex;

	FWorldCountryMaterialParameterSet M_OccupiedLookParameters;
	FWorldCountryMaterialParameterSet M_LiberatedLookParameters;

	// Runtime country list; every "CountryIndex" in this class indexes these three parallel arrays.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AStaticMeshActor>> M_RuntimeCountryMeshActors;

	TArray<FName> M_RuntimeCountryIds;

	// World-XY triangles of each country mesh; empty when the mesh data could not be read (bounds/trace fallback).
	TArray<TArray<FWorldCountryTriangle2D>> M_CountryTrianglesByIndex;
};
