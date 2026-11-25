// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawnerTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyAirDrop.generated.h"

class AEnemyAirDrop;
class UEnemyAirDropSettings;
class UGlobalAbility;
class UGlobalAbilitiesManager;
class UGA_AircraftDrop;
class UGA_Paratroopers;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyAirDropDropEvent, AEnemyAirDrop*, AirDrop, int32, DropNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyAirDropFinished, AEnemyAirDrop*, AirDrop);
// A vehicle or squad of a cargo plane drop was released on the ground (squads: spawned at the landed aircraft).
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEnemyAirDropUnitDelivered, AEnemyAirDrop* /*AirDrop*/, AActor* /*Unit*/);
// Blueprint twin of FOnEnemyAirDropUnitDelivered, for mission stages.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyAirDropUnitLanded, AEnemyAirDrop*, AirDrop, AActor*, Unit);

/** How the units of a placed air drop arrive; also picks the shape drawn above each landing point. */
UENUM(BlueprintType)
enum class EEnemyAirDropDelivery : uint8
{
	// Squads and vehicles come in transport planes that land, unload and fly off (the enemy's tank drop ability).
	CargoPlane UMETA(DisplayName = "Cargo Plane (lands)"),
	// Squads jump from a transport over the landing point (the enemy's paratrooper ability). Vehicles cannot jump:
	// they come in a landing cargo plane at the same time.
	Parachute UMETA(DisplayName = "Parachute (squads jump)")
};

/** How a placed air drop depends on another actor of the map. */
UENUM(BlueprintType)
enum class EEnemyAirDropLinkRule : uint8
{
	None UMETA(DisplayName = "No Linked Actor"),
	// Drops come only while the linked actor stands (an airfield, a radio mast); once it is destroyed no new drop
	// starts (aircraft already flying still deliver).
	OnlyWhileAlive UMETA(DisplayName = "Only While Alive"),
	// The drops start once the linked actor is destroyed (the player blew up the bridge: paratroopers come).
	StartWhenDestroyed UMETA(DisplayName = "Start When Destroyed")
};

UENUM(BlueprintType)
enum class EEnemyAirDropLandingOrder : uint8
{
	// Drop 1 lands on point 1, drop 2 on point 2, ... and round again.
	InOrder UMETA(DisplayName = "In Order"),
	Random UMETA(DisplayName = "Random")
};

/** One kind of unit in a drop; each of the Count units picks one of the options at random. */
USTRUCT(BlueprintType)
struct FEnemyAirDropUnitEntry
{
	GENERATED_BODY()

	// Vehicles and squads can be mixed freely; each unit picks one of these at random.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
	TArray<FEnemyPlacementUnit> UnitOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/**
 * Units of one drop per difficulty, adding up like the wave spawner's: Hard drops Base + Normal extras + Hard extras.
 * Ironman drops the same as Brutal.
 */
USTRUCT(BlueprintType)
struct FEnemyAirDropUnits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Base Units (New To RTS +)")
	TArray<FEnemyAirDropUnitEntry> BaseUnits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Extra Units (Normal +)")
	TArray<FEnemyAirDropUnitEntry> ExtraUnitsNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Extra Units (Hard +)")
	TArray<FEnemyAirDropUnitEntry> ExtraUnitsHard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Extra Units (Brutal + Ironman)")
	TArray<FEnemyAirDropUnitEntry> ExtraUnitsBrutal;
};

/**
 * The global abilities that fly the drop: copies of their templates in the global ability data asset, so aircraft,
 * landing times, parachutes and sounds are the abilities' own; only the units are this actor's.
 */
USTRUCT(BlueprintType)
struct FEnemyAirDropDeliverySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	EEnemyAirDropDelivery Delivery = EEnemyAirDropDelivery::Parachute;

	// An aircraft drop ability (lands and unloads); carries the vehicles, and the squads of a cargo plane drop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	EGlobalAbility CargoPlaneAbility = EGlobalAbility::GA_SovietT34Drop;

	// A paratrooper ability; carries the squads of a parachute drop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery",
		meta = (EditCondition = "Delivery == EEnemyAirDropDelivery::Parachute", EditConditionHides))
	EGlobalAbility ParachuteAbility = EGlobalAbility::GA_SovietParatroopers;

	// Vehicles hang on ropes below the cargo plane and are lowered to the ground while it hovers, instead of riding
	// under it to a landing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	bool bRopeSlungVehicles = false;

	// The announcer warns the player as when the enemy AI sends the same ability (silent for abilities it has no
	// line for).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	bool bWarnPlayer = true;

	// Cargo plane drops: the landed units join the enemy AI's direct control. Off for a mission that orders them
	// itself (it listens to OnUnitDelivered). Parachuting squads always go to the enemy AI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	bool bHandUnitsToEnemyAI = true;
};

/** When the air drop starts and how often it comes; the same rules as the wave spawner and the convoy route. */
USTRUCT(BlueprintType)
struct FEnemyAirDropTiming
{
	GENERATED_BODY()

	// Off: the air drop waits until a mission calls StartAirDrop (find it with FindEnemyAirDrop).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	bool bStartOnBeginPlay = true;

	// Seconds from StartAirDrop (or BeginPlay, or the linked actor's destruction) to the first drop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float FirstDropDelay = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	EEnemyWaveSpawnerRepeat Repeat = EEnemyWaveSpawnerRepeat::Once;

	// Total drops, the first one included.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (ClampMin = "2", UIMin = "2", EditCondition = "Repeat == EEnemyWaveSpawnerRepeat::FixedCount",
			EditConditionHides))
	int32 DropCount = 3;

	// On: the next drop's countdown starts when the previous drop's aircraft have delivered and left (or were shot
	// down). Off: a drop comes every interval, whatever the previous one does.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (EditCondition = "Repeat != EEnemyWaveSpawnerRepeat::Once", EditConditionHides))
	bool bWaitForPreviousDrop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (ClampMin = "1.0", EditCondition = "Repeat != EEnemyWaveSpawnerRepeat::Once", EditConditionHides))
	float DropInterval = 180.f;
};

/** Makes the air drop depend on another actor of the map being alive or destroyed. */
USTRUCT(BlueprintType)
struct FEnemyAirDropLink
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linked Actor")
	EEnemyAirDropLinkRule Rule = EEnemyAirDropLinkRule::None;

	// Any actor of the map: a building, a unit, a bridge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linked Actor",
		meta = (EditCondition = "Rule != EEnemyAirDropLinkRule::None", EditConditionHides))
	TObjectPtr<AActor> LinkedActor = nullptr;
};

/** The ability copies of one started drop, kept alive until each has ended. */
USTRUCT()
struct FEnemyAirDropRun
{
	GENERATED_BODY()

	int32 DropNumber = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> Abilities;

	int32 PendingAbilityCount = 0;
};

/**
 * @brief Placed in a mission map to author enemy air drops in the level: drag the LandingPoints diamonds (teal).
 * Each drop is flown by copies of the enemy's own drop abilities from the global ability data asset (the aircraft drop
 * for cargo planes and vehicles, the paratrooper ability for parachuting squads), owned by the enemy and started from
 * the enemy's aircraft edge, so the units join the enemy AI's direct control once landed, as the strategic AI's drops
 * do. Repeats once, a fixed count or forever, and can depend on another actor being alive or destroyed.
 * Starts on BeginPlay, or from a mission via FindEnemyAirDrop + StartAirDrop.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyAirDrop : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AEnemyAirDrop();

	/**
	 * @brief Starts the countdown to the first drop (with Start When Destroyed: once the linked actor is destroyed).
	 * Does nothing while running; after it finished it runs again.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Air Drop")
	void StartAirDrop();

	/**
	 * @brief Sets up an air drop a mission spawns at runtime (SpawnActorDeferred, call before FinishSpawning): one
	 * drop on the actor's own location, FirstDropDelay seconds after the enemy AI is ready.
	 */
	void InitRuntimeAirDrop(const FName InAirDropId, const FEnemyAirDropUnits& InUnits,
	                        const FEnemyAirDropDeliverySettings& InDelivery, const float FirstDropDelay);

	/** @brief No new drop starts; flying aircraft still deliver and the air drop finishes once they are gone. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Air Drop")
	void StopAirDrop();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Drop")
	bool GetIsAirDropRunning() const { return bM_IsRunning; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Drop")
	FName GetAirDropId() const { return AirDropId; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Drop")
	int32 GetStartedDropCount() const { return M_StartedDropCount; }

	// Drops whose aircraft have all delivered and left or were shot down.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Drop")
	int32 GetFinishedDropCount() const { return M_FinishedDropCount; }

	/** @return The placed air drop with this AirDropId, or null (reports an error) when there is none. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Air Drop",
		meta = (WorldContext = "WorldContextObject"))
	static AEnemyAirDrop* FindEnemyAirDrop(const UObject* WorldContextObject, const FName AirDropIdToFind);

	// The aircraft of a drop took off towards the landing point; DropNumber counts from 1 over the actor's life.
	UPROPERTY(BlueprintAssignable, Category = "Air Drop")
	FOnEnemyAirDropDropEvent OnDropStarted;

	// Every aircraft of the drop delivered and left, or was shot down.
	UPROPERTY(BlueprintAssignable, Category = "Air Drop")
	FOnEnemyAirDropDropEvent OnDropFinished;

	// No drop will start any more and every started one is done.
	UPROPERTY(BlueprintAssignable, Category = "Air Drop")
	FOnEnemyAirDropFinished OnAirDropFinished;

	// Each vehicle and squad a cargo plane of this air drop releases (not parachuted squads).
	FOnEnemyAirDropUnitDelivered OnUnitDelivered;

	// The same moment for Blueprints: a mission can order the landed unit, track it, or show it.
	UPROPERTY(BlueprintAssignable, Category = "Air Drop")
	FOnEnemyAirDropUnitLanded OnUnitLanded;

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override;
	virtual FName GetPlacementId() const override { return AirDropId; }
	virtual UTexture2D* GetEditorSpriteTexture() const override;
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty) override;
	virtual void OnEnemyAINeverReady() override;

	// Name missions use to find this air drop (FindEnemyAirDrop); also shown above it in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop")
	FName AirDropId = NAME_None;

	// Relative to the actor: where the units land; drag the diamonds. Several points spread repeated drops.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop|Landing", meta = (MakeEditWidget = "true"))
	TArray<FVector> LandingPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop|Landing")
	EEnemyAirDropLandingOrder LandingOrder = EEnemyAirDropLandingOrder::InOrder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop|Units")
	FEnemyAirDropUnits Units;

	// Optional: when set, Units and Timing.FirstDropDelay come from this asset at BeginPlay (tune the drop in the
	// asset, next to the map's other data assets, instead of on the placed actor).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop|Units")
	TObjectPtr<UEnemyAirDropSettings> SettingsAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop|Delivery")
	FEnemyAirDropDeliverySettings Delivery;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop|Timing")
	FEnemyAirDropTiming Timing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Drop|Linked Actor")
	FEnemyAirDropLink Link;

private:
#if RTS_WITH_MAP_TESTS
	// -EnemyAirDropValidate configures air drops and reads their runs.
	friend class UEnemyAirDropTestSubsystem;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UTexture2D> M_AirDropSprite = nullptr;
#endif

	UPROPERTY()
	TArray<FEnemyAirDropRun> M_Runs;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	ERTSGameDifficulty M_Difficulty = ERTSGameDifficulty::Normal;
	FTimerHandle M_NextDropTimerHandle;
	FTimerHandle M_LinkWatchTimerHandle;

	int32 M_StartedDropCount = 0;
	int32 M_FinishedDropCount = 0;
	// Drops started since the last StartAirDrop; ends a fixed count.
	int32 M_DropsThisRun = 0;
	int32 M_NextLandingPointIndex = 0;

	bool bM_IsRunning = false;
	// Set once no further drop will start this run; the air drop finishes when the started ones are done too.
	bool bM_HasLaunchedLastDrop = false;
	// True while a drop's abilities execute: one that ends at once must not finish the half-started drop.
	bool bM_IsLaunching = false;

	// ----- Linked actor -----
	bool GetIsLinkedActorAlive() const;
	void StartWatchingLinkedActor();
	void CheckLinkedActorDestroyed();

	// ----- Launch -----
	void LaunchDrop();
	bool TryBuildPayload(TArray<ETankSubtype>& OutVehicles, TArray<ESquadSubtype>& OutSquads) const;
	/** @brief Adds the entry's Count units, each one of its options at random; an unpicked entry adds none. */
	static void AppendEntryPayload(const FEnemyAirDropUnitEntry& Entry, TArray<ETankSubtype>& OutVehicles,
	                               TArray<ESquadSubtype>& OutSquads);
	TArray<const TArray<FEnemyAirDropUnitEntry>*> GetUnitListsForDifficulty(const ERTSGameDifficulty Difficulty) const;
	FVector PickLandingPoint();
	bool TryCreateDropAbilities(const TArray<ETankSubtype>& Vehicles, const TArray<ESquadSubtype>& Squads,
	                            TArray<UGlobalAbility*>& OutAbilities, EGlobalAbility& OutWarningAbility) const;
	UGA_AircraftDrop* CreateCargoPlaneCopy(const TArray<ETankSubtype>& Vehicles,
	                                       const TArray<ESquadSubtype>& Squads) const;
	UGA_Paratroopers* CreateParachuteCopy(const TArray<ESquadSubtype>& Squads) const;
	void ExecuteDropAbilities(const int32 DropNumber, const TArray<UGlobalAbility*>& Abilities,
	                          UGlobalAbilitiesManager& Manager, const FVector& LandingPoint);
	void WarnPlayer(const EGlobalAbility WarningAbility) const;
	bool GetHasMoreDropsToStart() const;
	void ScheduleNextDrop();
	void MarkLastDropLaunched();
	void TryFinishAirDrop();

	// ----- Ability callbacks -----
	void HandleAbilityEnded(UGlobalAbility* Ability);
	void HandleCargoUnitReleased(UGA_AircraftDrop* Ability, AActor* Unit);
	void FinishRunIfDone(const int32 DropNumber);
	void UnbindAllRuns();
	void UnbindRun(const FEnemyAirDropRun& Run);

	// ----- Editor -----
	bool GetHasUnsetUnitEntry() const;
	bool GetHasAnyUnitEntry() const;
	FString GetAbilityTemplateProblem() const;
	void AddLandingPointShapes(const FVector& LandingPoint, const bool bIsFirst, FEnemyPlacementShapes& OutSummary,
	                           FEnemyPlacementShapes& OutDetail) const;
	void AddUnitGlyphShapes(const FVector& LandingPoint, FEnemyPlacementShapes& OutDetail) const;
	void AddEntryGlyphShapes(const FEnemyAirDropUnitEntry& Entry, const FVector& LandingPoint,
	                         const bool bEveryDifficulty, int32& InOutUseIndex, FEnemyPlacementShapes& OutDetail) const;
	void AddLinkShapes(FEnemyPlacementShapes& OutSummary) const;
};
