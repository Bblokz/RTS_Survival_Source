// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyStrongpoint.generated.h"

class AEnemyController;
class AEnemyStrongpoint;
class AEnemyWaveSpawner;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyStrongpointUnits, AEnemyStrongpoint*, Strongpoint,
                                             const TArray<AActor*>&, Units);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyStrongpointEvent, AEnemyStrongpoint*, Strongpoint);

/** One defender of the strongpoint: where it stands, which way it faces and what it is. */
USTRUCT(BlueprintType)
struct FEnemyStrongpointPost
{
	GENERATED_BODY()

	// Relative to the strongpoint; drag the widget to move the defender and rotate it to turn it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post", meta = (MakeEditWidget = "true"))
	FTransform Post;

	// The defender is one of these, picked at random.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post")
	TArray<FEnemyPlacementUnit> UnitOptions;

	// The post is manned on this difficulty and harder ones (Ironman counts as harder than Brutal).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post")
	ERTSGameDifficulty FromDifficulty = ERTSGameDifficulty::NewToRTS;

	// Vehicles dig in after spawning; units without the dig-in ability ignore it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post")
	bool bDigIn = true;

	// Squads only: on, an idle squad moves to cover it finds nearby (up to ~25 m, e.g. beside a dug-in vehicle);
	// off, it stays on the post.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post")
	bool bTakeNearbyCover = true;
};

USTRUCT(BlueprintType)
struct FEnemyStrongpointTiming
{
	GENERATED_BODY()

	// Off: the garrison waits until a mission calls SpawnGarrison (find it with FindEnemyStrongpoint).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	bool bSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float SpawnDelay = 1.f;

	// Seconds after a defender spawned before it starts digging in.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float DigInDelay = 3.f;
};

UENUM(BlueprintType)
enum class EEnemyStrongpointRelease : uint8
{
	// The defenders hold their posts for good (they still fight what comes in range).
	Never UMETA(DisplayName = "Never (hold posts)"),
	// The enemy AI may use the defenders from the start (counter-attacks, defending other bases).
	OnSpawn UMETA(DisplayName = "On Spawn"),
	// The survivors join the enemy AI once the garrison lost this fraction of its defenders.
	OnLosses UMETA(DisplayName = "On Losses")
};

/** When the defenders leave their posts and join the enemy AI's direct control. */
USTRUCT(BlueprintType)
struct FEnemyStrongpointRelease
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release")
	EEnemyStrongpointRelease Release = EEnemyStrongpointRelease::Never;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release",
		meta = (ClampMin = "0.01", ClampMax = "1.0",
			EditCondition = "Release == EEnemyStrongpointRelease::OnLosses", EditConditionHides))
	float LossFraction = 0.5f;

	// Off: dug-in defenders stay in their pits when the others are released.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release",
		meta = (EditCondition = "Release != EEnemyStrongpointRelease::Never", EditConditionHides))
	bool bReleaseDugInDefenders = false;
};

/** Placed wave spawners the strongpoint starts when it comes under pressure. */
USTRUCT(BlueprintType)
struct FEnemyStrongpointReinforcements
{
	GENERATED_BODY()

	// Set these spawners to not start on BeginPlay; their waypoints should end at the strongpoint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reinforcements")
	TArray<TObjectPtr<AEnemyWaveSpawner>> Spawners;

	// The spawners start once the garrison lost this fraction of its defenders.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reinforcements", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float LossFraction = 0.25f;
};

/** Lets the enemy AI's base-trench digging fortify this strongpoint (a designer base on the trench component). */
USTRUCT(BlueprintType)
struct FEnemyStrongpointTrenches
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trenches")
	bool bDigTrenches = false;

	// Radius of the trench arc around the strongpoint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trenches",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bDigTrenches", EditConditionHides))
	float PerimeterRadius = 3000.f;

	// On: the arc faces the threat point. Off: it faces the player's HQ once the enemy AI knows where it is.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trenches",
		meta = (EditCondition = "bDigTrenches", EditConditionHides))
	bool bFaceThreatPoint = true;

	// Relative to the strongpoint; drag the diamond to where the attack is expected from.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trenches", meta = (MakeEditWidget = "true",
		EditCondition = "bDigTrenches && bFaceThreatPoint", EditConditionHides))
	FVector ThreatPoint = FVector(5000.f, 0.f, 0.f);
};

USTRUCT(BlueprintType)
struct FEnemyStrongpointMinimap
{
	GENERATED_BODY()

	// A fixed icon on the strongpoint while any defender lives.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	bool bShowOnMinimap = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap",
		meta = (EditCondition = "bShowOnMinimap", EditConditionHides))
	EMinimapIconType Icon = EMinimapIconType::EnemyBoss;
};

/** One spawned defender and the post it holds. */
USTRUCT()
struct FEnemyStrongpointDefender
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Unit = nullptr;

	int32 PostIndex = INDEX_NONE;
	bool bDigsIn = false;
	bool bIsReleased = false;
};

/**
 * @brief Placed in a mission map to author an enemy strongpoint in the level: defenders on posts you drag and rotate
 * (magenta; filled = every difficulty, outline = harder difficulties only), optional dug-in vehicles, a trench arc
 * from the enemy AI's base-trench digging, linked wave spawners as reinforcements and a release of the survivors to
 * the enemy AI's direct control. Spawns on BeginPlay, or from a mission via FindEnemyStrongpoint + SpawnGarrison.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyStrongpoint : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AEnemyStrongpoint();

	/** @brief Spawns the defenders of the posts manned on this difficulty; does nothing when already spawned. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Strongpoint")
	void SpawnGarrison();

	/** @brief Hands the living defenders to the enemy AI's direct control now (dug-in ones per the release setting). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Strongpoint")
	void ReleaseGarrisonToEnemyAI();

	/** @brief Starts the linked reinforcement spawners now; does nothing when they were already called. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Strongpoint")
	void CallReinforcements();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Strongpoint")
	FName GetStrongpointId() const { return StrongpointId; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Strongpoint")
	int32 GetDefenderCount() const { return M_Defenders.Num(); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Strongpoint")
	int32 GetAliveDefenderCount() const;

	// True once every spawned defender is dead.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Strongpoint")
	bool GetIsCleared() const { return bM_IsCleared; }

	/** @return The placed strongpoint with this StrongpointId, or null (reports an error) when there is none. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Strongpoint",
		meta = (WorldContext = "WorldContextObject"))
	static AEnemyStrongpoint* FindEnemyStrongpoint(const UObject* WorldContextObject, const FName StrongpointIdToFind);

	// Every defender of this difficulty spawned (failed spawns are left out).
	UPROPERTY(BlueprintAssignable, Category = "Strongpoint")
	FOnEnemyStrongpointUnits OnGarrisonSpawned;

	// These defenders joined the enemy AI's direct control.
	UPROPERTY(BlueprintAssignable, Category = "Strongpoint")
	FOnEnemyStrongpointUnits OnGarrisonReleased;

	UPROPERTY(BlueprintAssignable, Category = "Strongpoint")
	FOnEnemyStrongpointEvent OnReinforcementsCalled;

	// The last defender died (a "take the strongpoint" objective is done).
	UPROPERTY(BlueprintAssignable, Category = "Strongpoint")
	FOnEnemyStrongpointEvent OnStrongpointCleared;

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override { return TEXT("STRONGPOINT"); }
	virtual FName GetPlacementId() const override { return StrongpointId; }
	virtual UTexture2D* GetEditorSpriteTexture() const override;
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty) override;

	// Name missions use to find this strongpoint (FindEnemyStrongpoint); also shown above it in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strongpoint")
	FName StrongpointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strongpoint|Posts")
	TArray<FEnemyStrongpointPost> Posts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strongpoint|Timing")
	FEnemyStrongpointTiming Timing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strongpoint|Release")
	FEnemyStrongpointRelease Release;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strongpoint|Reinforcements")
	FEnemyStrongpointReinforcements Reinforcements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strongpoint|Trenches")
	FEnemyStrongpointTrenches Trenches;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strongpoint|Minimap")
	FEnemyStrongpointMinimap Minimap;

private:
#if RTS_WITH_MAP_TESTS
	// -EnemyPlacementValidate configures strongpoints and reads their defenders and icon.
	friend class UEnemyPlacementTestSubsystem;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UTexture2D> M_StrongpointSprite = nullptr;
#endif

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	UPROPERTY()
	TArray<FEnemyStrongpointDefender> M_Defenders;

	FTimerHandle M_WatchTimerHandle;
	FName M_MinimapIconId = NAME_None;
	int32 M_PendingSpawns = 0;

	bool bM_HasStartedSpawning = false;
	// True while the spawn requests are being made; a callback inside a request must not finish the garrison early.
	bool bM_IsIssuingSpawns = false;
	bool bM_HasSpawnedGarrison = false;
	bool bM_IsReleased = false;
	bool bM_HasCalledReinforcements = false;
	bool bM_IsCleared = false;
	// AddFortifiedBase appends a base each call, so the strongpoint adds its own once.
	bool bM_HasAddedTrenchBase = false;

	// ----- Spawning -----
	bool GetIsValidEnemyController() const;
	void AddTrenchBaseOnce();
	void SpawnPostsForDifficulty(const ERTSGameDifficulty Difficulty);
	bool SpawnPostDefender(const int32 PostIndex);
	void HandleDefenderSpawned(const int32 PostIndex, AActor* SpawnedUnit);
	void ScheduleDigIn(AActor* Defender);
	void FinishSpawningGarrison();

	// ----- Watching losses -----
	void StartWatchingGarrison();
	void WatchGarrison();
	float GetLossFraction(const int32 AliveDefenders) const;
	void ClearStrongpoint();
	TArray<AActor*> GetAliveDefenders() const;
	void AddMinimapIcon();
	void RemoveMinimapIcon();

	// ----- Editor -----
	FVector GetPostWorldLocation(const FEnemyStrongpointPost& StrongpointPost) const;
	float GetPostWorldYaw(const FEnemyStrongpointPost& StrongpointPost) const;
	bool GetHasUnsetPost() const;
	FString GetReinforcementProblem() const;
	void AddPostShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const;
	void AddTrenchShapes(FEnemyPlacementShapes& OutDetail) const;
	void AddReinforcementLinkShapes(FEnemyPlacementShapes& OutSummary) const;
	float GetTrenchArcDegrees() const;
};
