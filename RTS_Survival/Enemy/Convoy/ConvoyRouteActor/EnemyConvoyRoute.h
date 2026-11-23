// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyBehaviourTypes.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawnerTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyConvoyRoute.generated.h"

class AConvoy;
class AEnemyController;
class AEnemyConvoyRoute;
class UConvoyDefinition;
class UTexture2D;
struct FAttackWaveElement;
struct FConvoyRoutePolyline;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyConvoyRouteConvoyStarted, AEnemyConvoyRoute*, Route,
                                             AConvoy*, Convoy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyConvoyRouteConvoyFinished, AEnemyConvoyRoute*, Route,
                                               AConvoy*, Convoy, EConvoyOutcome, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyConvoyRouteFinished, AEnemyConvoyRoute*, Route);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyConvoyRouteTruckCaptured, AEnemyConvoyRoute*, Route,
                                               AConvoy*, Convoy, AActor*, Truck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyConvoyRouteStopStarted, AEnemyConvoyRoute*, Route,
                                               AConvoy*, Convoy, int32, ConvoyIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnEnemyConvoyRouteStopEnded, AEnemyConvoyRoute*, Route,
                                              AConvoy*, Convoy, int32, ConvoyIndex, EConvoyStopEndReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnEnemyConvoyRouteBranchTruck, AEnemyConvoyRoute*, Route,
                                              AActor*, Truck, EConvoyCargoType, Cargo, int32, ConvoyIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnEnemyConvoyRouteAmbushZoneEntered, AEnemyConvoyRoute*, Route,
                                              AConvoy*, Convoy, int32, ConvoyIndex, int32, ZoneIndex,
                                              bool, bRadioPostAlive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnEnemyConvoyRouteHuntersDismounted, AEnemyConvoyRoute*, Route,
                                              AConvoy*, Convoy, int32, ConvoyIndex, const TArray<AActor*>&, Squads);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyConvoyRouteCargoOutcome, AEnemyConvoyRoute*, Route,
                                               AConvoy*, Convoy, const FConvoyCargoOutcome&, Outcome);

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

/**
 * A timed halt of the whole column (trucks and escorts) at StopPoint, e.g. a checkpoint inspection. Attacked before
 * or during the stop, the convoy skips or ends it and floors it.
 */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteStop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	bool bEnabled = false;

	// Convoy indices of this route that stop (0 = the first convoy it started, counted over every StartRoute);
	// empty = every convoy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	TArray<int32> OnlyConvoyIndices;

	// Optional: stop at this actor instead of the StopPoint diamond (projected onto the drive path either way).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	TObjectPtr<AActor> StopActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop", meta = (ClampMin = "0.0", Units = "s"))
	float StopSeconds = 60.f;

	// The stop starts once the front truck is this close (along the road) to the stop point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop", meta = (ClampMin = "50.0", Units = "cm"))
	float ArriveTolerance = 400.f;

	// Attacked before or during the stop: the trucks' pace is multiplied by this (capped at their top speed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop", meta = (ClampMin = "1.0"))
	float FloorItSpeedMultiplier = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop", meta = (ClampMin = "0.0", Units = "s"))
	float FloorItSeconds = 30.f;

	// On: while flooring it the trucks keep driving even when the definition halts them behind fighting escorts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	bool bFloorItIgnoresHalt = true;
};

/**
 * The trucks of one cargo type leave the column at the fork (first BranchPoints diamond) and drive the branch path to
 * its last point (or DestinationActor); the convoy finishes only when both parts are done.
 */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch")
	bool bEnabled = false;

	// Convoy indices of this route that split (see FEnemyConvoyRouteStop); empty = every convoy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch")
	TArray<int32> OnlyConvoyIndices;

	// Trucks carrying this cargo turn off (FConvoyTruckSpec::bOverrideCargoType makes a mixed column).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch")
	EConvoyCargoType CargoType = EConvoyCargoType::Ammo;

	// Optional: appended to the BranchPoints as the destination (e.g. the artillery battery).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch")
	TObjectPtr<AActor> DestinationActor = nullptr;

	// A branch truck this close (2D) to the destination has delivered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch", meta = (ClampMin = "200.0", Units = "cm"))
	float ArrivalRadius = 1500.f;

	// Applied to the convoy pace while a truck drives the branch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch", meta = (ClampMin = "0.1"))
	float SpeedMultiplier = 1.f;

	// On: a delivered truck is removed like an escaped one; off: it is released alive where it stands.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch")
	bool bDespawnOnArrival = true;

	// The column waits at the exit this long for branch trucks still driving; those then count as escaped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Branch", meta = (ClampMin = "0.0", Units = "s"))
	float MaxWaitAtExitSeconds = 120.f;
};

/**
 * Ambush zones along the road: the route reports each convoy entering one, and while RadioPostActor stands the
 * trucks speed up inside them.
 */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteAmbush
{
	GENERATED_BODY()

	// Convoy indices of this route that use the zones (see FEnemyConvoyRouteStop); empty = every convoy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambush")
	TArray<int32> OnlyConvoyIndices;

	// Optional: while it stands, convoys drive SpeedMultiplier faster inside the zones. Unset = no speed-up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambush")
	TObjectPtr<AActor> RadioPostActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambush", meta = (ClampMin = "1.0"))
	float SpeedMultiplier = 1.35f;

	// Radius of every zone (the AmbushZonePoints diamonds, ZoneActors and tagged actors).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambush", meta = (ClampMin = "100.0", Units = "cm"))
	float ZoneRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambush")
	TArray<TObjectPtr<AActor>> ZoneActors;

	// Optional: every actor in the map with this tag is a zone too (e.g. ORE_AmbushZone). None = off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambush")
	FName ZoneActorTag = NAME_None;
};

/** Squads one hunter truck carries. */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteHunterTruckLoad
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	TArray<FEnemyPlacementUnit> Squads;
};

/**
 * The trucks carry infantry that gets out at the DismountPoint (or where the convoy is attacked first) and
 * attack-moves through the HunterWaypoints to AttackTarget, as one attack wave of the enemy controller.
 */
USTRUCT(BlueprintType)
struct FEnemyConvoyRouteHunters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	bool bEnabled = false;

	// Convoy indices of this route that carry hunters (see FEnemyConvoyRouteStop); empty = every convoy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	TArray<int32> OnlyConvoyIndices;

	// Squads every living truck unloads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	TArray<FEnemyPlacementUnit> SquadsPerTruck;

	// Optional per truck (index = position in the truck column, 0 = front); a non-empty entry replaces SquadsPerTruck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	TArray<FEnemyConvoyRouteHunterTruckLoad> PerTruckSquads;

	// Optional: dismount at this actor instead of the DismountPoint diamond.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	TObjectPtr<AActor> DismountActor = nullptr;

	// The front truck dismounts this far (along the road) before the dismount point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters", meta = (ClampMin = "0.0", Units = "cm"))
	float DismountRadius = 2500.f;

	// On: an attack before the dismount point unloads the squads where the trucks are.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	bool bDismountWhenAttacked = true;

	// The column stands this long while the squads get out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters", meta = (ClampMin = "0.0", Units = "s"))
	float DismountHaltSeconds = 8.f;

	// Final waypoint of the hunters (the player's ridge or HQ); without it the last HunterWaypoints diamond is.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	TObjectPtr<AActor> AttackTarget = nullptr;

	// Squads appear this far beside their truck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters", meta = (ClampMin = "0.0", Units = "cm"))
	float UnloadSideOffset = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hunters")
	FEnemyWaveSpawnerAttackSettings Attack;
};

/** Trucks of one started convoy, counted for the route's totals. */
USTRUCT()
struct FEnemyConvoyRouteConvoyTally
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AConvoy> Convoy = nullptr;

	int32 ConvoyId = INDEX_NONE;
	// The route's convoy index (0 = the first convoy it started, counted over every StartRoute).
	int32 ConvoyIndex = INDEX_NONE;
	int32 TruckCount = 0;
	int32 DestroyedTruckCount = 0;
	// Trucks that surrendered: neither destroyed nor escaped.
	int32 CapturedTruckCount = 0;
	// Branch trucks that reached the branch destination: neither destroyed nor escaped.
	int32 BranchDeliveredTruckCount = 0;
	FDelegateHandle EventHandle;
	FDelegateHandle BehaviourHandle;
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

	// Trucks that surrendered and left their convoy alive (they count as stopped, not as destroyed or escaped).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	int32 GetCapturedTruckCount() const { return M_CapturedTruckCount; }

	// Convoys that lost every truck with at least one of them captured (all-destroyed ones are in GetDestroyedConvoyCount).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route")
	int32 GetStoppedConvoyCount() const { return M_StoppedConvoyCount; }

	// Branch trucks that reached the branch destination (not in GetEscapedTruckCount).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route|Behaviours")
	int32 GetBranchDeliveredTruckCount() const { return M_BranchDeliveredTruckCount; }

	/** @return The route's index of a convoy it started and that still runs (0 = first), or INDEX_NONE. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route|Behaviours")
	int32 GetConvoyIndexOf(const AConvoy* Convoy) const;

	/** @return Convoys of this route that have not finished yet. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Route|Behaviours")
	TArray<AConvoy*> GetActiveConvoys() const;

	/** @brief Convoys started from now on use this definition (one route can run differently loaded convoys). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Route|Behaviours")
	void SetConvoyDefinition(UConvoyDefinition* NewDefinition);

	/** @brief Sets the radio post of the ambush speed-up; convoys already driving keep the one they started with. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Route|Behaviours")
	void SetRadioPostActor(AActor* RadioPost) { Ambush.RadioPostActor = RadioPost; }

	/** @brief Sets where hunter squads attack; read when they dismount. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Route|Behaviours")
	void SetHunterAttackTarget(AActor* Target) { Hunters.AttackTarget = Target; }

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

	// A truck of a convoy on this route surrendered; it now belongs to its captor.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route")
	FOnEnemyConvoyRouteTruckCaptured OnConvoyTruckCaptured;

	// Sent once per finished convoy, before OnConvoyFinished: truck fates in total and per cargo type.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route|Behaviours")
	FOnEnemyConvoyRouteCargoOutcome OnConvoyCargoOutcome;

	// The column stands at the stop point now.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route|Behaviours")
	FOnEnemyConvoyRouteStopStarted OnConvoyStopStarted;

	// The stop is over: timed out, attacked during it, skipped because attacked before it, or cancelled.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route|Behaviours")
	FOnEnemyConvoyRouteStopEnded OnConvoyStopEnded;

	// A truck turned off at the fork onto the branch path.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route|Behaviours")
	FOnEnemyConvoyRouteBranchTruck OnBranchTruckLeftColumn;

	// A branch truck reached the branch destination; it is removed right after this event.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route|Behaviours")
	FOnEnemyConvoyRouteBranchTruck OnBranchTruckArrived;

	// A convoy's column centre entered an ambush zone (ZoneIndex: diamonds, then ZoneActors, then tagged actors).
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route|Behaviours")
	FOnEnemyConvoyRouteAmbushZoneEntered OnConvoyEnteredAmbushZone;

	// The hunter squads of a convoy got out and attack-move to their target.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Route|Behaviours")
	FOnEnemyConvoyRouteHuntersDismounted OnHuntersDismounted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy Route|Stop")
	FEnemyConvoyRouteStop Stop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy Route|Branch")
	FEnemyConvoyRouteBranch Branch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy Route|Ambush Zones")
	FEnemyConvoyRouteAmbush Ambush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy Route|Hunters")
	FEnemyConvoyRouteHunters Hunters;

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

	// Relative to the actor: where the column stops (Stop); projected onto the drive path.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Stop", meta = (MakeEditWidget = "true"))
	FVector StopPoint = FVector(4000.f, 0.f, 0.f);

	// Relative to the actor: the first point is the fork on the road, the last the branch destination (Branch).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Branch", meta = (MakeEditWidget = "true"))
	TArray<FVector> BranchPoints;

	// Relative to the actor: centres of ambush zones (Ambush.ZoneRadius each).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Ambush Zones", meta = (MakeEditWidget = "true"))
	TArray<FVector> AmbushZonePoints;

	// Relative to the actor: where the hunters get out (Hunters); projected onto the drive path.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Hunters", meta = (MakeEditWidget = "true"))
	FVector DismountPoint = FVector(4000.f, 0.f, 0.f);

	// Relative to the actor: the hunters' attack-move path after they got out; Hunters.AttackTarget is appended.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Route|Hunters", meta = (MakeEditWidget = "true"))
	TArray<FVector> HunterWaypoints;

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

	// Built once per run from BranchPoints (+ the destination actor); empty when it could not be built.
	FConvoyRoute M_BuiltBranchRoute;
	bool bM_HasTriedBranchRoute = false;

	// Hunters spawn as an attack wave of this controller; looked up again when OnEnemyAIReady gave none.
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	int32 M_BranchDeliveredTruckCount = 0;

	// Hunter waves are told apart by this id (the wave id is not known yet when a wave ends inside its start call).
	int32 M_NextHunterRequestId = 0;
	TArray<int32> M_HunterRequestsWithUnits;

	ERTSGameDifficulty M_Difficulty = ERTSGameDifficulty::Normal;
	FTimerHandle M_NextConvoyTimerHandle;

	int32 M_StartedConvoyCount = 0;
	// Convoys started since the last StartRoute; picks the guard tier step and ends a fixed count.
	int32 M_ConvoysThisRun = 0;
	int32 M_EscapedConvoyCount = 0;
	int32 M_DestroyedConvoyCount = 0;
	int32 M_EscapedTruckCount = 0;
	int32 M_DestroyedTruckCount = 0;
	int32 M_CapturedTruckCount = 0;
	int32 M_StoppedConvoyCount = 0;

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
	void TrackStartedConvoy(AConvoy* Convoy, const int32 GuardTierIndex, const int32 ConvoyIndex);
	bool GetHasMoreConvoysToStart() const;
	void ScheduleNextConvoy();
	void MarkLastConvoyLaunched();
	void TryFinishRoute();

	// ----- Convoy callbacks -----
	UFUNCTION()
	void HandleConvoyFinished(AConvoy* Convoy, EConvoyOutcome Outcome);

	void HandleConvoyEvent(const FConvoyEvent& Event);
	void CountCapturedTruck(const FConvoyEvent& Event);
	FEnemyConvoyRouteConvoyTally* FindTally(const int32 ConvoyId);
	void CountFinishedConvoy(const FEnemyConvoyRouteConvoyTally& Tally, const EConvoyOutcome Outcome,
	                         const int32 EscapedTruckCount);
	void UnbindConvoy(const FEnemyConvoyRouteConvoyTally& Tally);
	void UnbindAllConvoys();

	// ----- Behaviours -----
	static bool GetAppliesToConvoy(const TArray<int32>& OnlyConvoyIndices, const int32 ConvoyIndex);
	/**
	 * @brief Resolves the route's stop, branch, ambush and hunter settings into the convoy's plan.
	 * @param ConvoyIndex Route index of the convoy (filters OnlyConvoyIndices).
	 * @param Route The built drive path the convoy drives.
	 * @return The plan; parts that do not apply stay disabled.
	 */
	FConvoyBehaviourPlan BuildBehaviourPlan(const int32 ConvoyIndex, const FConvoyRoute& Route);
	FConvoyStopPlan BuildStopPlan(const FConvoyRoutePolyline& Polyline) const;
	FConvoyBranchPlan BuildBranchPlan(const FConvoyRoutePolyline& Polyline);
	FConvoyAmbushPlan BuildAmbushPlan() const;
	FConvoyHunterPlan BuildHunterPlan(const FConvoyRoutePolyline& Polyline) const;
	bool TryGetBuiltBranchRoute(FConvoyRoute& OutRoute);
	TArray<FVector> GetBranchControlPoints() const;
	FVector GetStopWorldLocation() const;
	FVector GetDismountWorldLocation() const;

	void HandleConvoyBehaviourEvent(const FConvoyBehaviourEvent& Event);
	void HandleStopEvent(const FConvoyBehaviourEvent& Event, const FEnemyConvoyRouteConvoyTally& Tally);
	void HandleBranchTruckArrived(const FConvoyBehaviourEvent& Event, FEnemyConvoyRouteConvoyTally& Tally);

	// ----- Hunters -----
	void LaunchHunters(const FConvoyBehaviourEvent& Event, const FEnemyConvoyRouteConvoyTally& Tally);
	/**
	 * @brief One wave element per squad of each living truck, beside the truck on the side facing the hunters' path.
	 * @param Event The dismount event with the truck locations and column indices.
	 * @param FirstWaypoint Where the squads head first.
	 * @return The elements; empty when no truck carries a picked squad.
	 */
	TArray<FAttackWaveElement> BuildHunterWaveElements(const FConvoyBehaviourEvent& Event,
	                                                   const FVector& FirstWaypoint) const;
	void AppendTruckHunterElements(const FVector& TruckLocation, const TArray<FEnemyPlacementUnit>& Load,
	                               const FVector& FirstWaypoint, TArray<FAttackWaveElement>& OutElements) const;
	FRotator GetHunterFinalDirection(const TArray<FVector>& Waypoints, const FVector& StartLocation) const;
	const TArray<FEnemyPlacementUnit>& GetHunterLoad(const int32 TruckColumnIndex) const;
	bool TryGetHunterWaypoints(TArray<FVector>& OutWaypoints) const;
	AEnemyController* FindEnemyControllerForHunters();
	void HandleHunterUnitsSpawned(const int32 WaveId, const TArray<AActor*>& Units, const int32 HunterRequestId,
	                              const int32 ConvoyIndex, TWeakObjectPtr<AConvoy> Convoy);
	// A wave can end without units (no enemy wave supply left); the dismount event then goes out empty.
	void HandleHunterWaveEnded(const int32 WaveId, const int32 HunterRequestId, const int32 ConvoyIndex,
	                           TWeakObjectPtr<AConvoy> Convoy);

	// ----- Editor -----
	float GetExitRadius() const;
	bool GetIsPreviewCurrent() const;
	void AddRoutePointShapes(const TArray<FVector>& WorldRoutePoints, FEnemyPlacementShapes& OutSummary,
	                         FEnemyPlacementShapes& OutDetail) const;
	void AddPreviewShapes(FEnemyPlacementShapes& OutDetail) const;
	void AddBehaviourShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const;
};
