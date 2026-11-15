// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawnerTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyConvoyRoute.generated.h"

class AConvoy;
class AEnemyConvoyRoute;
class UConvoyDefinition;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyConvoyRouteConvoyStarted, AEnemyConvoyRoute*, Route,
                                             AConvoy*, Convoy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyConvoyRouteConvoyFinished, AEnemyConvoyRoute*, Route,
                                               AConvoy*, Convoy, EConvoyOutcome, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyConvoyRouteFinished, AEnemyConvoyRoute*, Route);

/**
 * Escort strength per difficulty as an index into the convoy definition's guard tiers (clamped to the last tier).
 * Ironman uses the Brutal tier.
 */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteEscort
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escort", DisplayName = "Guard Tier (New To RTS)",
		meta = (ClampMin = "0"))
	int32 GuardTierNewToRTS = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escort", DisplayName = "Guard Tier (Normal)",
		meta = (ClampMin = "0"))
	int32 GuardTierNormal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escort", DisplayName = "Guard Tier (Hard)",
		meta = (ClampMin = "0"))
	int32 GuardTierHard = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escort", DisplayName = "Guard Tier (Brutal + Ironman)",
		meta = (ClampMin = "0"))
	int32 GuardTierBrutal = 2;

	// Each later convoy of this route uses this many tiers more (escorts grow light -> armoured -> tanks); 0 = alike.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escort", meta = (ClampMin = "0"))
	int32 GuardTierStepPerConvoy = 0;

	// Multiplies the cargo the caches pay (the definition scales it with the escort's cost already).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escort", meta = (ClampMin = "0.0"))
	float CargoMultiplier = 1.f;
};

/** When the route starts and how many convoys drive it. */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteTiming
{
	GENERATED_BODY()

	// Off: the route waits until a mission calls StartRoute (find it with FindEnemyConvoyRoute).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	bool bStartOnBeginPlay = true;

	// Seconds from StartRoute (or BeginPlay) to the first convoy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float FirstConvoyDelay = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	EEnemyWaveSpawnerRepeat Repeat = EEnemyWaveSpawnerRepeat::Once;

	// Total convoys, the first one included.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (ClampMin = "2", UIMin = "2", EditCondition = "Repeat == EEnemyWaveSpawnerRepeat::FixedCount",
			EditConditionHides))
	int32 ConvoyCount = 3;

	// On: the next convoy's countdown starts when the previous one escaped or was destroyed, so only one drives at a
	// time. Off: a convoy starts every interval, whatever the previous one does.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (EditCondition = "Repeat != EEnemyWaveSpawnerRepeat::Once", EditConditionHides))
	bool bWaitForPreviousConvoy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (ClampMin = "1.0", EditCondition = "Repeat != EEnemyWaveSpawnerRepeat::Once", EditConditionHides))
	float ConvoyInterval = 240.f;
};

/** Ties the route to an enemy building: once it is destroyed no new convoy starts (driving ones carry on). */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteOwnership
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ownership")
	bool bHasOwningBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ownership",
		meta = (EditCondition = "bHasOwningBuilding", EditConditionHides))
	TObjectPtr<AActor> OwningBuilding = nullptr;
};

/** Trucks of one started convoy, counted for the route's totals. */
USTRUCT()
struct FEnemyConvoyRouteConvoyTally
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AConvoy> Convoy = nullptr;

	int32 ConvoyId = INDEX_NONE;
	int32 TruckCount = 0;
	int32 DestroyedTruckCount = 0;
	FDelegateHandle EventHandle;
};

/**
 * @brief Placed in a mission map to author enemy convoys in the level: drag the RoutePoints diamonds (start, via
 * points, exit; green). Each convoy is an AConvoy from the chosen convoy definition on the drivable route
 * UConvoyRouteLibrary builds through the points, so cargo, escorts, combat and the minimap mark are the convoy
 * system's own. Starts on BeginPlay, or from a mission via FindEnemyConvoyRoute + StartRoute.
 * Use "Preview Drive Path" in the details panel to see the road the trucks will take (needs built navigation).
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyConvoyRoute : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AEnemyConvoyRoute();

	/** @brief Starts the countdown to the first convoy; does nothing while running. After it finished it runs again. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Route")
	void StartRoute();

	/** @brief No new convoys start; driving convoys carry on and the route finishes once they are done. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Route")
	void StopRoute();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	bool GetIsRouteRunning() const { return bM_IsRunning; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	FName GetRouteId() const { return RouteId; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	int32 GetStartedConvoyCount() const { return M_StartedConvoyCount; }

	// Convoys whose trucks reached the exit with at least one truck.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	int32 GetEscapedConvoyCount() const { return M_EscapedConvoyCount; }

	// Convoys that lost every truck.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	int32 GetDestroyedConvoyCount() const { return M_DestroyedConvoyCount; }

	// Trucks that drove off the map at the exit (a "supply delivered" meter counts these).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	int32 GetEscapedTruckCount() const { return M_EscapedTruckCount; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	int32 GetDestroyedTruckCount() const { return M_DestroyedTruckCount; }

	/** @return The placed route with this RouteId, or null (reports an error) when there is none. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Route",
		meta = (WorldContext = "WorldContextObject"))
	static AEnemyConvoyRoute* FindEnemyConvoyRoute(const UObject* WorldContextObject, const FName RouteIdToFind);

	// A convoy spawned its column on the route.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route")
	FOnEnemyConvoyRouteConvoyStarted OnConvoyStarted;

	// A convoy escaped, lost all its trucks or was ended from code.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route")
	FOnEnemyConvoyRouteConvoyFinished OnConvoyFinished;

	// No convoy will start any more and every started one is done.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route")
	FOnEnemyConvoyRouteFinished OnRouteFinished;

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

#if WITH_EDITOR
	/** @brief Builds the drivable route through the points in the editor world and draws it while selected. */
	UFUNCTION(CallInEditor, Category = "Convoy Route")
	void PreviewDrivePath();

	UFUNCTION(CallInEditor, Category = "Convoy Route")
	void ClearDrivePathPreview();
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override { return TEXT("CONVOY"); }
	virtual FName GetPlacementId() const override { return RouteId; }
	virtual UTexture2D* GetEditorSpriteTexture() const override;
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty) override;
	virtual bool GetNeedsEnemyController() const override { return false; }

	// Name missions use to find this route (FindEnemyConvoyRoute); also shown above it in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route")
	FName RouteId = NAME_None;

	// Trucks, cargo, guard tiers, spacing, pace and combat reactions of every convoy on this route.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route")
	TObjectPtr<UConvoyDefinition> Definition = nullptr;

	// Relative to the actor: the first point is where the column forms, the last is the exit; drag the diamonds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Points", meta = (MakeEditWidget = "true"))
	TArray<FVector> RoutePoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Escort")
	FEnemyConvoyRouteEscort Escort;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Timing")
	FEnemyConvoyRouteTiming Timing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Owning Building")
	FEnemyConvoyRouteOwnership Ownership;

	// The convoy system's moving mark on the lead truck and marks on dropped cargo.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Minimap")
	bool bShowOnMinimap = true;

private:
#if RTS_WITH_MAP_TESTS
	// -EnemyPlacementValidate configures routes and reads their built route and tallies.
	friend class UEnemyPlacementTestSubsystem;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UTexture2D> M_RouteSprite = nullptr;

	// Drive path built by PreviewDrivePath, with the route points it was built from (it is drawn while they match).
	UPROPERTY(Transient)
	TArray<FVector> M_PreviewDrivePath;

	UPROPERTY(Transient)
	TArray<FVector> M_PreviewControlPoints;

	UPROPERTY(Transient)
	bool bM_PreviewReachedExit = true;
#endif

	UPROPERTY()
	TArray<FEnemyConvoyRouteConvoyTally> M_Convoys;

	// Built once per run from the route points; every convoy of the run drives it.
	FConvoyRoute M_BuiltRoute;
	bool bM_HasBuiltRoute = false;

	ERTSGameDifficulty M_Difficulty = ERTSGameDifficulty::Normal;
	FTimerHandle M_NextConvoyTimerHandle;

	int32 M_StartedConvoyCount = 0;
	// Convoys started since the last StartRoute; picks the guard tier step and ends a fixed count.
	int32 M_ConvoysThisRun = 0;
	int32 M_EscapedConvoyCount = 0;
	int32 M_DestroyedConvoyCount = 0;
	int32 M_EscapedTruckCount = 0;
	int32 M_DestroyedTruckCount = 0;

	bool bM_IsRunning = false;
	// Set once no further convoy will start this run; the route finishes when the started ones are done too.
	bool bM_HasLaunchedLastConvoy = false;

	// ----- Launch -----
	void LaunchConvoy();
	bool GetIsOwningBuildingAlive() const;
	bool TryGetBuiltRoute(FConvoyRoute& OutRoute);
	int32 GetGuardTierIndex(const int32 ConvoyIndex) const;
	int32 GetBaseGuardTierForDifficulty() const;
	FConvoyStartParams BuildStartParams(const int32 ConvoyIndex) const;
	void TrackStartedConvoy(AConvoy* Convoy, const int32 GuardTierIndex);
	bool GetHasMoreConvoysToStart() const;
	void ScheduleNextConvoy();
	void MarkLastConvoyLaunched();
	void TryFinishRoute();

	// ----- Convoy callbacks -----
	UFUNCTION()
	void HandleConvoyFinished(AConvoy* Convoy, EConvoyOutcome Outcome);

	void HandleConvoyEvent(const FConvoyEvent& Event);
	FEnemyConvoyRouteConvoyTally* FindTally(const int32 ConvoyId);
	void CountFinishedConvoy(const FEnemyConvoyRouteConvoyTally& Tally, const EConvoyOutcome Outcome);
	void UnbindAllConvoys();

	// ----- Editor -----
	float GetExitRadius() const;
	bool GetIsPreviewCurrent() const;
	void AddRoutePointShapes(const TArray<FVector>& WorldRoutePoints, FEnemyPlacementShapes& OutSummary,
	                         FEnemyPlacementShapes& OutDetail) const;
	void AddPreviewShapes(FEnemyPlacementShapes& OutDetail) const;
};
