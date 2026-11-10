// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/FieldTrench/FieldTrenchLayout.h"
#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorComponent.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyBaseTrenchComponent.generated.h"

class AEnemyController;
class ARTSFieldTrench;
struct FFieldTrenchPieceRuntime;

/**
 * @brief The caps and shape of the trenches the enemy AI digs around its bases; edit them on the map's enemy
 * controller (Base Trenches component). The trenches cost the AI nothing, so these caps are what limits them.
 */
USTRUCT(BlueprintType)
struct FEnemyBaseTrenchSettings
{
	GENERATED_BODY()

	// Off: the AI digs no trenches on this map.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches")
	bool bEnabled = true;

	// How often the AI looks for a trench to start.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.1", Units = "s"))
	float ThinkIntervalSeconds = 5.0f;

	// After a base starts a trench it waits this long before it starts the next one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "s"))
	float DigCooldownSeconds = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0"))
	int32 MaxTrenchesPerBase = 10;

	// Trench length one base may dig in all, in metres.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0"))
	float MaxTrenchMetresPerBase = 160.0f;

	// Trenches of all bases together.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0"))
	int32 MaxTrenches = 60;

	// Seconds of digging per metre; the player's trench digs at 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0"))
	float DigSecondsPerMetre = 1.5f;

	// A trench stops before the first piece that would have to lean further than this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches",
		meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float MaximumSlopeDegrees = 15.0f;

	// Each free trench anchor of a pit gets a straight trench this long, out of its port.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float PitTrenchLength = 1200.0f;

	// A pit belongs to the nearest base within this distance; a pit further from every base is fortified alone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float BaseGroupingRadius = 8000.0f;

	// Fire trenches on an arc around the base, facing the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches")
	bool bDigPerimeterTrenches = true;

	// How wide the arc is, centred on the direction of the player's HQ.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches",
		meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float PerimeterArcDegrees = 140.0f;

	// The arc is dug in sections of this length...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "300.0", Units = "cm"))
	float PerimeterSectionLength = 1800.0f;

	// ...with gaps of this length between them, so units and vehicles get through.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float PerimeterGapLength = 700.0f;

	// The arc lies this far beyond the base's farthest production building.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float PerimeterMargin = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumPerimeterRadius = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumPerimeterRadius = 9000.0f;

	// The AI's build radius: no trench piece lies further than this beyond the base's arc, or beyond the pit
	// trench of a pit fortified alone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float BuildRadiusMargin = 1500.0f;

	// No piece of a new trench comes closer than this to a piece of another trench of the AI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumTrenchSpacing = 450.0f;

	// The player does not see where the AI will dig: a piece shows once its digging starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Trenches")
	bool bHidePlannedPieces = true;
};

/** @brief One base the AI fortifies, with what it has dug so far. */
USTRUCT()
struct FEnemyTrenchBase
{
	GENERATED_BODY()

	FVector Centre = FVector::ZeroVector;
	// The radius of the perimeter arc; 0 for a pit fortified alone, which gets no arc.
	float PerimeterRadius = 0.0f;
	// Where the arc faces; zero faces the player's HQ once the strategic AI knows where it is.
	FVector2D ThreatDirection = FVector2D::ZeroVector;
	bool bIsAddedByDesigner = false;
	int32 TrenchCount = 0;
	float DugMetres = 0.0f;
	float NextDigTimeSeconds = 0.0f;
	// The arc's sections, in the order they are dug; planned once the arc's direction is known.
	TArray<TArray<FVector2D>> PerimeterSections;
	int32 NextPerimeterSection = 0;
};

/**
 * @brief The enemy AI's base fortification: on its own timer it digs Soviet field trenches out of the free trench
 * anchors of its pits and on an arc around each base, facing the player. The trenches dig themselves, cost the AI
 * nothing and are capped by FEnemyBaseTrenchSettings. Lives on AEnemyController and starts with the strategic AI's
 * thinking (UEnemyStrategicAIComponent::StartStrategicAIThinkingTimer), so in survival bases dig in during the grace
 * period. Bases come from the strategic AI's base clusters, from AddFortifiedBase, and from the pits themselves.
 */
UCLASS(ClassGroup = (Custom))
class RTS_SURVIVAL_API UEnemyBaseTrenchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyBaseTrenchComponent();

	/** @brief Starts the think timer; does nothing when it already runs or when the settings switch trenches off. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Base Trenches")
	void StartBaseTrenchDigging();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Base Trenches")
	void StopBaseTrenchDigging();

	/**
	 * @brief Lets a map or mission fortify a spot that the strategic AI's base search does not find.
	 * @param Centre The middle of the base.
	 * @param PerimeterRadius The radius of its arc; 0 digs only out of its pits.
	 * @param ThreatDirection Where the arc faces; zero faces the player's HQ.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Base Trenches")
	void AddFortifiedBase(const FVector& Centre, const float PerimeterRadius, const FVector2D ThreatDirection);

	bool GetIsDigging() const { return bM_IsDigging; }
	const TArray<TWeakObjectPtr<ARTSFieldTrench>>& GetDugTrenches() const { return M_Trenches; }
	const TArray<FEnemyTrenchBase>& GetBases() const { return M_Bases; }

#if RTS_WITH_MAP_TESTS
	void MapTest_SetSettings(const FEnemyBaseTrenchSettings& NewSettings) { Settings = NewSettings; }
	const FEnemyBaseTrenchSettings& MapTest_GetSettings() const { return Settings; }
#endif

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Trenches")
	FEnemyBaseTrenchSettings Settings;

private:
	FTimerHandle M_ThinkTimerHandle;
	bool bM_IsDigging = false;

	UPROPERTY()
	TArray<FEnemyTrenchBase> M_Bases;

	UPROPERTY()
	TArray<TWeakObjectPtr<ARTSFieldTrench>> M_Trenches;

	// Anchors whose trench did not fit (a wall, a slope); they are not tried again.
	TArray<FFieldTrenchAnchorHandle> M_FailedAnchors;

	void ThinkStep();
	void UpdateBasesFromStrategicAI();
	/** @return Index of the base nearest the location within the grouping radius; INDEX_NONE when none is. */
	int32 FindBaseNear(const FVector& Location, const float Radius) const;
	/**
	 * @brief Merges the pit-only bases near this base into it, with what they dug, so pits found before their base
	 * count towards the base's caps and the base's arc is planned once.
	 */
	void AbsorbPitOnlyBases(const int32 BaseIndex);
	/** @brief Pits far from every base become a base of their own, which only digs out of its pits. */
	void AddBasesForLonelyPits(const TArray<FFieldTrenchAnchorHandle>& FreeAnchors);
	/** @return True when the base started a trench. */
	bool TryDigForBase(const int32 BaseIndex, const TArray<FFieldTrenchAnchorHandle>& FreeAnchors);
	bool GetCanBaseDig(const FEnemyTrenchBase& Base, const float Now) const;
	bool TryDigFromAnchor(FEnemyTrenchBase& Base, const FFieldTrenchAnchorHandle& Anchor);
	bool TryDigPerimeterSection(FEnemyTrenchBase& Base);
	/** @brief Plans the arc's sections once the direction it faces is known. */
	bool TryPlanPerimeterSections(FEnemyTrenchBase& Base) const;
	FVector2D GetThreatDirection(const FEnemyTrenchBase& Base) const;

	/**
	 * @brief Spawns a Soviet trench, lays it along the path as far as it fits and starts digging it.
	 * @param Base The base it is dug for: its build radius, and what it counts towards.
	 * @param Path Ground-plane points the trench follows.
	 * @param Anchor The trench starts on this anchor's port; unset starts it with an End cap.
	 * @return True when at least one piece of real trench fitted and digging started.
	 */
	bool TryLayTrench(FEnemyTrenchBase& Base, const TArray<FVector2D>& Path, const FFieldTrenchAnchorHandle& Anchor);
	/**
	 * @brief The player placement's PlanBuildableTrench for the AI: replans shorter until every piece fits.
	 * @return The pieces that fit; empty when not even the first one does.
	 */
	TArray<FFieldTrenchPiecePlacement> PlanBuildablePieces(
		ARTSFieldTrench& Trench,
		const FEnemyTrenchBase& Base,
		const TArray<FVector2D>& Path,
		const FFieldTrenchPlanOptions& Options,
		const float MaximumLengthCm,
		const float GroundHeight,
		const AActor* IgnoredActor) const;
	bool GetIsPieceBuildable(const FFieldTrenchPieceRuntime& Piece, const FEnemyTrenchBase& Base,
	                         const ARTSFieldTrench& Trench, const AActor* IgnoredActor) const;
	bool GetIsPieceBlocked(const FFieldTrenchPieceRuntime& Piece, const ARTSFieldTrench& Trench,
	                       const AActor* IgnoredActor) const;
	bool GetIsNearOtherTrench(const FVector& Location, const ARTSFieldTrench& Trench) const;
	float GetBuildRadius(const FEnemyTrenchBase& Base) const;
	int32 CountLiveTrenches() const;
	bool GetIsAnchorFailed(const FFieldTrenchAnchorHandle& Anchor) const;
	ARTSFieldTrench* SpawnSovietTrench(const FVector& Location) const;
};
