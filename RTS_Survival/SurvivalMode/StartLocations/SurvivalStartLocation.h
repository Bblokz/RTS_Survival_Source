// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalDefensePointFinder.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartContentPlanner.h"

#include "SurvivalStartLocation.generated.h"

class UBillboardComponent;
class UFowComp;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class USurvivalStartContentSettings;
struct FStreamableHandle;

/**
 * @brief A start location of a survival map: a large flat build area with ramps to defensible hills.
 * The picked one gets resource fields, wrecks and pits; the others get Soviet bases.
 * Place the actor on the flat ground where the HQ should land, set BuildAreaRadius, and either drag defense
 * points (DesignerDefensePoints widgets) onto hill tops / ramp heads or let the hill-top scan find them.
 * Selecting the actor in the editor draws the build area, defense points and (with PreviewContentSettings) the
 * resource / wreck bands; "Preview Start Content" draws a full planned layout without spawning anything.
 * @note Owned by the start-location design; the public signatures below are part of the contract.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASurvivalStartLocation : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalStartLocation();

	/** @return Where the HQ lands; the centre of the flat build area. */
	FVector GetDeployCenter() const;

	/** @return Radius of the flat build area around the deploy centre. */
	float GetBuildAreaRadius() const { return BuildAreaRadius; }

	FText GetDisplayName() const { return DisplayName; }

	/** @return World locations suited for defenses (hill tops, ramp heads). */
	const TArray<FVector>& GetDefensePoints() const { return M_DefensePoints; }

	/** @brief Generates resource fields, wrecks and capturable pits for the player around this location. */
	void GeneratePlayerStartContent(const USurvivalStartContentSettings* ContentSettings);

	/** @return Defense points with how they were found (designer / hill top / build-edge fallback), same order. */
	const TArray<FSurvivalDefensePoint>& GetDefensePointDetails() const { return M_DefensePointDetails; }

	/**
	 * @brief Re-resolves the defense points from the designer points and the landscape.
	 * Runs at BeginPlay for every start location (the Soviet bases use them too) and on editor changes.
	 */
	void RefreshDefensePoints();

	/** @return Furthest distance from the deploy centre start content can reach with these settings. */
	float GetContentOuterRadius(const USurvivalStartContentSettings* ContentSettings) const;

	bool GetHasGeneratedPlayerContent() const { return bM_HasGeneratedPlayerContent; }

	/** @return Actors spawned by GeneratePlayerStartContent that still exist or were destroyed (weak). */
	const TArray<TWeakObjectPtr<AActor>>& GetSpawnedStartContent() const { return M_SpawnedStartContent; }

	/**
	 * @brief Reveals the fog of war around the location while the player picks one; the director turns it off for
	 * every location once the HQ has landed.
	 */
	void SetRevealFogOfWar(bool bReveal);

protected:
	virtual void BeginPlay() override;

	// Vision radius used while the location is revealed for the start location picker.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Start Location", meta = (ClampMin = "0.0"))
	float PickerVisionRadius = 16000.f;

	UPROPERTY(VisibleAnywhere, Category = "Survival|Start Location")
	TObjectPtr<UFowComp> M_FowComponent = nullptr;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	// Editor only: marks the debug view dirty; the (throttled) editor tick re-scans the landscape.
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Start Location", meta = (ClampMin = "1000.0"))
	float BuildAreaRadius = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Start Location")
	FText DisplayName;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Survival|Defense Points")
	TArray<FVector> M_DefensePoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Defense Points")
	ESurvivalDefensePointSource DefensePointSource = ESurvivalDefensePointSource::DesignerThenAuto;

	// Relative to the actor; drag the diamond widgets onto hill tops and ramp heads. Snapped to the landscape.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Defense Points", meta = (MakeEditWidget = "true"))
	TArray<FVector> DesignerDefensePoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Defense Points")
	FSurvivalDefensePointSearchSettings DefensePointSearch;

	// Optional: draws the resource / wreck bands and enables "Preview Start Content". Not used at runtime.
	UPROPERTY(EditAnywhere, Category = "Survival|Debug")
	TObjectPtr<USurvivalStartContentSettings> PreviewContentSettings = nullptr;

	UPROPERTY(EditAnywhere, Category = "Survival|Debug")
	ERTSGameDifficulty PreviewDifficulty = ERTSGameDifficulty::Normal;

	// Draw the editor visualisation even when the actor is not selected.
	UPROPERTY(EditAnywhere, Category = "Survival|Debug")
	bool bAlwaysDrawDebug = false;

	// Keep drawing build area, defense points and the generated layout while playing (PIE / development).
	UPROPERTY(EditAnywhere, Category = "Survival|Debug")
	bool bDrawDebugInGame = false;

	/** @brief Re-runs the hill-top scan and the band preview (also happens on move / property change). */
	UFUNCTION(CallInEditor, Category = "Survival|Debug")
	void RefreshEditorPreview();

	/** @brief Plans the start content with PreviewContentSettings and PreviewDifficulty and draws it. */
	UFUNCTION(CallInEditor, Category = "Survival|Debug")
	void PreviewStartContent();

	UFUNCTION(CallInEditor, Category = "Survival|Debug")
	void ClearStartContentPreview();

	/** @brief Copies the resolved defense points (hill tops included) into DesignerDefensePoints for hand edits. */
	UFUNCTION(CallInEditor, Category = "Survival|Defense Points")
	void BakeDefensePointsIntoDesignerPoints();

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> M_EditorSprite = nullptr;
#endif

	// Same order as M_DefensePoints; plain data for the planner and the debug colours.
	TArray<FSurvivalDefensePoint> M_DefensePointDetails;

	// The plan waiting for its classes to finish loading; spawned in OnStartContentLoaded.
	FSurvivalStartContentPlan M_PendingContentPlan;

	TSharedPtr<FStreamableHandle> M_ContentLoadHandle;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedStartContent;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> M_FoliageComponents;

	bool bM_HasGeneratedPlayerContent = false;

	// Last plan drawn by the debug view (editor preview or the generated runtime layout).
	FSurvivalStartContentPlan M_DebugPlan;
	bool bM_HasDebugPlan = false;

	// Editor preview state: re-planned with the same seed whenever the location or its settings change.
	bool bM_PreviewRequested = false;
	int32 M_PreviewSeed = 0;

	// Set by OnConstruction; the editor tick refreshes at most every DebugRefreshIntervalSeconds while dragging.
	bool bM_DebugViewDirty = true;
	double M_LastDebugRefreshSeconds = 0.0;

	// Landscape-following rings drawn by the debug view: build edge, resource band, wreck band.
	TArray<FVector> M_DebugBuildRing;
	TArray<FVector> M_DebugResourceInnerRing;
	TArray<FVector> M_DebugResourceOuterRing;
	TArray<FVector> M_DebugWreckOuterRing;

	ERTSGameDifficulty GetContentDifficulty() const;
	int32 MakeContentSeed(const USurvivalStartContentSettings& ContentSettings) const;
	void GetDesignerDefensePointsWorld(TArray<FVector>& OutPoints) const;
	void GatherOtherStartAreas(const USurvivalStartContentSettings& ContentSettings,
	                           TArray<FSurvivalReservedCircle>& OutAreas) const;

	/**
	 * @brief Builds the plan input from this location, its defense points and the other start locations.
	 * @param ContentSettings Settings the plan is built with.
	 * @param Difficulty Difficulty the counts are scaled for.
	 * @param Seed Random seed of the layout.
	 * @param OutInput Receives the filled input.
	 */
	void BuildPlanInput(
		const USurvivalStartContentSettings& ContentSettings,
		const ERTSGameDifficulty Difficulty,
		const int32 Seed,
		FSurvivalStartContentPlanInput& OutInput) const;

	void RequestStartContentLoad();
	void OnStartContentLoaded();
	void SpawnPendingContent();
	AActor* SpawnPlannedActor(const FSurvivalPlannedActor& PlannedActor, TSet<FSoftObjectPath>& OutMissingClasses);
	void SpawnFoliageInstances(const TArray<FSurvivalPlannedMeshInstance>& Instances, const float CullDistance);
	UInstancedStaticMeshComponent* CreateFoliageComponent(UStaticMesh* Mesh, const float CullDistance);
	void ReportPlanShortfalls(const FSurvivalStartContentPlan& Plan) const;

	void BuildPreviewPlan();
	bool GetIsValidPreviewContentSettings() const;
	void TickEditorDebugRefresh();

	void RefreshDebugRings();
	void BuildGroundRing(const float Radius, TArray<FVector>& OutRing) const;
	void DrawDebugView() const;
	void DrawDebugRing(const TArray<FVector>& Ring, const FColor& Color, const float Thickness) const;
	void DrawDebugDefensePoints() const;
	void DrawDebugPlan() const;
	bool GetShouldDrawDebugView() const;
};
