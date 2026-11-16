// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallHoleField.h"

#include "WallTankBreachComponent.generated.h"

class UBoxComponent;
class UNavAreaBase;
class UShapeComponent;
class UNavArea;
class UNiagaraSystem;
class UPrimitiveComponent;
class URTSNavCollisionEnvObstacle;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class UStaticMeshComponent;
class UWallHoleDamageComponent;
struct FHitResult;

USTRUCT(BlueprintType)
struct FWallTankBreachSettings
{
	GENERATED_BODY()

	// Only heavy tanks break this wall (Berlin ruin walls); medium tanks go round.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	bool bHeavyTanksOnly = false;

	// A wall without an owner (no RTS component, the Berlin ruins) counts as not the human player's: his tanks break it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	bool bBreachUnownedWall = false;

	// Tall pieces (four-storey facades, firewalls) do not keep standing over a tank-sized gap: when the top would
	// fall, the whole piece dies instead and its own death plays.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	bool bKillWhenTopFalls = false;

	// Wall broken beyond the hull on each side, cm.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	float SideMargin = 25.f;

	// Once the wall above the hull falls, the gap widens by this much per cm of height on each side (a V).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0", ClampMax="2.0"))
	float VSlope = 0.45f;

	// Width of the jagged band along the breach edge, cm.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="10.0"))
	float EdgeBand = 110.f;

	// Broken stub of wall left at the foot of the breach, cm.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float StubHeight = 18.f;

	// Seconds from the first break until the unsupported wall above the hull starts to come down.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float TopCollapseDelay = 0.45f;

	// Seconds the wall above takes to crumble from the slab over the hull out to the full V.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float TopCollapseDuration = 0.3f;

	// A stretch left standing between the V and a free end of the wall narrower than this share of the wall's height
	// cannot stand on its own and falls with the breach.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float MinFreeStandingWidthShare = 0.6f;

	// Debris speed while the hull pushes through, and for the slabs that fall from above.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float PushDebrisSpeedScale = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float FallDebrisSpeedScale = 0.2f;

	// Health lost = removed share of the wall face x the wall's max health x this.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float HealthDamageScale = 1.f;

	// While the hull pushes through, the tank is held to this share of its travel speed (medium, heavy).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.05", ClampMax="1.0"))
	float MediumTankPushSpeedShare = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.05", ClampMax="1.0"))
	float HeavyTankPushSpeedShare = 0.6f;

	// Dust where the hull hits the wall and where the top lands.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	TObjectPtr<UNiagaraSystem> DustSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0.0"))
	float DustScale = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	TObjectPtr<USoundBase> BreachSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	TObjectPtr<USoundAttenuation> BreachAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	TObjectPtr<USoundConcurrency> BreachConcurrency;

	// Rubble decal on the ground once the top has fallen.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	bool bSpawnRubbleDecal = true;

	// Broken pieces of the wall left lying on both sides of a fallen breach (the wall's own debris meshes).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach", meta=(ClampMin="0", ClampMax="40"))
	int32 RubblePieces = 22;

	// Flags, banner poles and other meshes on the fallen part of the wall topple and fall to the ground with it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	bool bHideAttachmentsOverBreach = true;

	// The wall's nav: at setup the wall's own blockers are replaced by boxes along its centre line in the owner's
	// breachable area (only the human player's medium and heavy tanks plan through an AI wall), and every fallen
	// breach is left open for everyone. Without per-side nav filters the boxes block everyone instead.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach|Navigation")
	bool bManageWallNav = true;

	// Half depth of the nav boxes across the wall and their half height, cm.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach|Navigation", meta=(ClampMin="20.0"))
	float NavBlockHalfDepth = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach|Navigation", meta=(ClampMin="50.0"))
	float NavBlockHalfHeight = 400.f;

	// The open gap for navigation is this much narrower than the broken stretch on each side (jagged edges, rubble).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach|Navigation", meta=(ClampMin="0.0"))
	float NavGapInset = 30.f;
};

/** One medium or heavy tank pushing through the wall. */
struct FWallTankCrossing
{
	TWeakObjectPtr<AActor> Tank;
	// Hull footprint in the tank's own space (colliding components), taken at first contact.
	FBox LocalHullBox = FBox(ForceInit);
	FWallHoleFieldBreach Breach;
	// The breach as last cut; a step that would cut the same again is skipped.
	FWallHoleFieldBreach AppliedBreach;
	// Index in the hole component's breaches; INDEX_NONE until the hull has broken something.
	int32 BreachIndex = INDEX_NONE;
	// AppliedBreach holds a step (also one that broke nothing, e.g. through an existing gap).
	bool bHasAppliedStep = false;
	double FirstContactTime = -1.0;
	// When the hull first broke wall; the top falls a moment later.
	double FirstBreakTime = -1.0;
	FVector LastTravel = FVector::ZeroVector;
	// Unrolled (U, Z) box of the hull at the last step: no debris starts inside it.
	FBox2D HullZone = FBox2D(ForceInit);
	bool bIsOverlapping = true;
	// The top has started to fall (effects played) and has finished falling (nav opened).
	bool bHasTopStartedFalling = false;
	bool bHasTopFallen = false;
	// The tank is slowed (ATankMaster mobility restriction, this component as source) while its hull pushes through.
	bool bIsSlowed = false;
};

/** A flag or pole on a fallen stretch of wall, tipping over and dropping to the ground. */
struct FWallAttachmentFall
{
	TWeakObjectPtr<USceneComponent> Component;
	FTransform StartTransform;
	// Tips over around this world axis through its bounds centre and drops this far, over the fall time.
	FVector Pivot = FVector::ZeroVector;
	FVector Axis = FVector::RightVector;
	float DropHeight = 0.f;
	float Elapsed = 0.f;
};

/**
 * @brief Lets the human player's medium and heavy tanks drive through an AI wall (the Soviet walls): the hull breaks a
 * gap the width of the tank, the wall above crumbles a moment later, debris tips over along the tank's travel, rubble
 * stays and the wall loses health for the broken area. The AI's own tanks never break (or plan through) it.
 * Needs a UWallHoleDamageComponent in field mode on the same actor; AWallActor::SetupWallCollision turns it on.
 * @note The RTS_NavFilter_* Blueprints must derive from URTSWallGateNavQueryFilter (per-side breachable wall areas).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWallTankBreachComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWallTankBreachComponent();

	/**
	 * @brief Called after the wall's collision setup (which resets every channel): tanks overlap the wall instead of
	 * being ignored, the mesh stops carving the navmesh and the wall's nav becomes the owner's breachable area.
	 * @param WallMesh The mesh the hole component cuts.
	 */
	void OnWallCollisionSetUp(UPrimitiveComponent* WallMesh);

	int32 GetActiveCrossingCount() const { return M_Crossings.Num(); }

	/**
	 * @brief Test hook: a tank crossing without overlap events (headless checks); the tank is tracked until it clears.
	 * @param Tank A medium or heavy tank of the other side.
	 */
	UFUNCTION(BlueprintCallable, Category="Breach|Debug")
	void Debug_StartCrossing(AActor* Tank);

#if WITH_EDITOR
	// Authoring (RTS.WallBreach.ConfigureRusWall).
	void Editor_SetSettings(const FWallTankBreachSettings& InSettings)
	{
		Modify();
		Settings = InSettings;
	}
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breach")
	FWallTankBreachSettings Settings;

private:
	UFUNCTION()
	void OnWallBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnWallEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                      int32 OtherBodyIndex);

	/** @return True for a medium or heavy tank of the human player against a wall of the AI side. */
	bool GetIsBreachingTank(const AActor* Actor) const;
	/** @return The wall's owning player; false when the wall has no RTS component. */
	bool TryGetWallOwningPlayer(uint8& OutOwningPlayer) const;
	void StartCrossing(AActor* Tank);
	FWallTankCrossing* FindCrossing(const AActor* Tank);
	void EnsureUpdateTimer();
	void UpdateCrossings();

	/**
	 * @brief Widens the crossing's breach to the hull's current footprint and lets the top crumble when its time comes.
	 * @param Crossing The crossing to advance.
	 * @return False when the crossing is finished (tank gone or clear of the wall, top fallen).
	 */
	bool UpdateCrossing(FWallTankCrossing& Crossing);
	void UpdateCrossing_Push(FWallTankCrossing& Crossing);
	void UpdateCrossing_TopFall(FWallTankCrossing& Crossing, double Now);

	/**
	 * @brief The along-wall interval and height the hull covers now, in the wall mesh's space (scaled walls too).
	 * @param Crossing The crossing; its tank must be valid.
	 * @param OutUMin Receives the interval start along the wall.
	 * @param OutUMax Receives the interval end along the wall.
	 * @param OutGroundZ Receives the hull's bottom (not below the wall's foot).
	 * @param OutHullHeight Receives the hull's height.
	 * @return False when the hull does not reach the wall's centre line.
	 */
	bool GetHullInterval(const FWallTankCrossing& Crossing, float& OutUMin, float& OutUMax, float& OutGroundZ,
	                     float& OutHullHeight) const;

	FVector GetTravelDirection(const FWallTankCrossing& Crossing) const;
	void SlowTank(FWallTankCrossing& Crossing);
	void ReleaseTank(FWallTankCrossing& Crossing) const;
	void ReleaseAllTanks();

	/** @return Face area the step removed, cm²; zero when nothing changed or nothing was standing there. */
	float ApplyBreachStep(FWallTankCrossing& Crossing, float DebrisSpeedScale, bool bKeepDebrisOutOfHull);

	// A free end of the wall too narrow to stand on its own once the V is cut falls with it.
	void ExtendBreachOverFreeEnds(FWallHoleFieldBreach& Breach) const;
	void OnTopStartsFalling(const FWallTankCrossing& Crossing);
	void OnTopFallen(const FWallTankCrossing& Crossing);
	void PlayBreachEffects(const FVector& WorldLocation, float DustScale) const;
	void SpawnRubble(const FWallTankCrossing& Crossing);
	void ApplyPendingBreachDamage();
	/**
	 * @brief Everything standing on the fallen stretch (flags, poles) tips over along the tank's travel and drops.
	 * @param Crossing The crossing whose top has fallen.
	 */
	void DropAttachmentsOverBreach(const FWallTankCrossing& Crossing);
	void StartAttachmentFall(UStaticMeshComponent* Attachment, const FVector& TravelDirection);
	void UpdateAttachmentFalls();
	FVector GetBreachWorldPoint(const FWallHoleFieldBreach& Breach, float HeightAboveGround) const;

	// ---- Weapon collision with a gap ----
	/**
	 * @brief Once a breach has fallen, shells and sight lines pass through it: the mesh stops blocking weapon and
	 * visibility traces and boxes along the standing stretches block them instead (same responses as the mesh had).
	 */
	void RebuildWeaponCollision();
	void AddWeaponBlock(int32 SegmentIndex, float UFrom, float UTo);
	void OnWallDied();
	// A breached Berlin ruin's remnant: keeps its gap (nav and weapon blocks) instead of the kit's full-length boxes.
	void OnWallDied_KeepBreachInRemnant();
	/**
	 * @brief The kit's death graph spawns collapse heaps (cover Blueprints that block every unit) from the owner's
	 * DeathHeapsSmall / DeathHeapsLarge arrays: the ones that would land in an open breach are dropped, so the gap a
	 * tank drove through stays a way through.
	 */
	void RemoveDeathHeapsOverOpenBreaches() const;
	bool GetIsHeapOverOpenBreach(const FTransform& ActorRelativeHeap, float HeapRadius,
	                             const TArray<FVector2f>& OpenIntervals) const;
	// Owners that are not AWallActor (the Berlin ruins) set up their collision on the template: start the next tick.
	void BeginPlay_SetUpOwnBreachSoon();

	// ---- Navigation ----
	void BuildWallNav();
	// Switches off the wall's own blockers (its nav modifier and the nav-blocker child actors) once.
	void TakeOverWallNav();
	/**
	 * @return The breachable wall area of the wall's side (player-owned, AI-owned or heavy-only); NavArea_Null while
	 * the agents' nav filters cannot tell the sides apart (not URTSWallGateNavQueryFilter), so nobody plans through an
	 * intact wall.
	 */
	TSubclassOf<UNavArea> GetWallNavArea();
	static bool GetDoNavFiltersKnowWallSides();
	/** @return The open stretches along the wall (fallen breaches, merged), narrowed by the nav gap inset. */
	TArray<FVector2f> GetOpenNavIntervals() const;
	void AddNavBlock(int32 SegmentIndex, float UFrom, float UTo, TSubclassOf<UNavArea> BlockedArea);
	// The kit's own box obstacles (Berlin ruins: hidden boxes, bDynamicObstacle) keep blocking until a breach opens:
	// they carry the wall's area meanwhile, and get their own area back when the piece dies.
	void CollectKitObstacleBoxes();
	void SetKitObstacleArea(TSubclassOf<UNavArea> AreaClass, bool bIsRelevant);
	void HandNavBackToKit();

	bool GetIsValidHoleDamage() const;
	bool GetIsValidWallMesh() const;

	UPROPERTY()
	TObjectPtr<UWallHoleDamageComponent> M_HoleDamage;

	// The wall's own mesh component (same actor), set by the collision setup.
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> M_WallMesh;

	TArray<FWallTankCrossing> M_Crossings;

	// Nav boxes along the wall, rebuilt whenever a breach opens.
	UPROPERTY()
	TArray<TObjectPtr<URTSNavCollisionEnvObstacle>> M_NavBlocks;

	TArray<FWallAttachmentFall> M_AttachmentFalls;

	// Boxes that block shells and sight along the standing stretches once a breach has fallen.
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> M_WeaponBlocks;

	// The mesh's weapon/visibility responses, taken over by the weapon blocks.
	FCollisionResponseContainer M_WeaponResponses;
	bool bM_HasTakenOverWeaponCollision = false;
	FDelegateHandle M_WallDiedHandle;

	// Rubble left by the breaches.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_RubblePieces;

	bool bM_HasTakenOverNav = false;
	bool bM_HasWarnedAboutNavFilters = false;

	// The kit's box obstacles and the areas they had (Berlin ruins); empty on the Rus walls.
	UPROPERTY()
	TArray<TObjectPtr<UShapeComponent>> M_KitObstacleBoxes;

	UPROPERTY()
	TArray<TSubclassOf<UNavAreaBase>> M_KitObstacleAreas;

	// Health the breach steps of one update took; applied after the crossings loop, as it can destroy the wall.
	float M_PendingBreachDamage = 0.f;

	FTimerHandle M_UpdateTimer;
};
