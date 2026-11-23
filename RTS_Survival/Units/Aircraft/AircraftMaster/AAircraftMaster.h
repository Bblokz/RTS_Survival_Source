#pragma once

#include "CoreMinimal.h"
#include "AircraftCommandsData/AircraftCommandsData.h"
#include "FStrafeAircraftSettings/FStrafeAircraftSettings.h"
#include "AircraftDropTypes.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceComponent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/Units/Aircraft/AircraftReloadManager/AircraftReloadManager.h"
#include "RTS_Survival/Units/Aircraft/AircraftTerrainFollow/AircraftTerrainFollow.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"

#include "AAircraftMaster.generated.h"

enum class ERTSAggroBehaviour :uint8;
enum class ETargetPreference : uint8;
struct FRTSVerticalAnimTextSettings;
struct FAircraftReloadManager;
class UAircraftAnimInstance;
class UEnemyStrategicAIComponent;
class UAircraftTargetAcquisition;
class UAircraftWeapon;
class UAircraftMovement;
class UAircraftDeathComponent;
class UCargoRopeSlingComponent;
// forward declare
class UAircraftOwnerComp;

/**
 * @brief RTS aircraft unit: orchestrates VTO/landing, move/attack pipelines, ownership on airbases, and weapon/anim sync.
 *
 * @note ******************************
 * @note Ownership & sockets 
 * @note - Ownership is driven by UAircraftOwnerComp to keep bay arbitration atomic and data-race free.
 * @note - The aircraft never sets its owner pointer directly; SetOwnerAircraft is called by the owner during assignment.
 * @note - On owner changes we first clear the old owner’s record, then route VTO/RTB so the new bay is authoritative.
 * @note ******************************
 * @note State machines
 * @note - LandedState drives vertical flight phases so tick routing is deterministic (VTO, Landing, WaitForBay, etc.).
 * @note - MovementState isolates “pathing vs. idle vs. attack” so command termination can restore idle cleanly.
 * @note ******************************
 * @note Commands 
 * @note - Commands (Move/Attack/RTB/ChangeOwner) can be issued while landed; we queue a post-lift-off action.
 * @note - RTB uses owner arbitration to guarantee doors are open before we start a timed descent.
 * @note ******************************
 * @note Safety & UX 
 * @note - All external triggers use validator helpers; errors are reported once at the source to avoid spam.
 * @note - Anim/weapon systems are kept in lock-step with landed state to avoid firing while docked or mid-transition.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAircraftStraightRunPassedPoint, AAircraftMaster*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAircraftDropPayloadReleased, AAircraftMaster* /*Aircraft*/,
                                     ATankMaster* /*ReleasedTank*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAircraftDropSquadReleased, AAircraftMaster* /*Aircraft*/,
                                     AActor* /*ReleasedSquad*/);

UCLASS()
class AAircraftMaster : public ASelectablePawnMaster, public IExperienceInterface, public IRTSUnit
{
	GENERATED_BODY()

public:
	AAircraftMaster(const FObjectInitializer& ObjectInitializer);
	
	virtual void PropagateNewAggroStance(const ERTSAggroBehaviour NewStance) final;
	virtual void PropagateNewTargetPreference(const ETargetPreference TargetPreference) final;
	
	
	ETargetPreference GetAircraftTargetPreference() const;
	void SetTargetPreference(const ETargetPreference TargetPreference) const;
	
	void SetAggroStance(const ERTSAggroBehaviour NewStance);
	ERTSAggroBehaviour GetEngagementStance() const;

	friend class UAircraftOwnerComp;

	/**
	 * @brief Switch to a new airbase: leave current owner safely, become idle, then route toward the new owner.
	 *
	 * @param NewOwner The target airbase component to take ownership.
	 * @pre NewOwner != GetOwnerAircraft() (caller must not pass the same owner).
	 * @post If landed, we VTO and then switch; if airborne, we switch immediately and force RTB to the new bay.
	 */
	void ChangeOwnerForAircraft(UAircraftOwnerComp* NewOwner);

	/** @brief Owner notifies us it is destroyed/invalid so we can drop ownership and recover to safe airborne idle. */
	void OnOwnerDies(const UAircraftOwnerComp* DeadOrInvalidOwner);

	/**
	 * @brief Owner begins packing up; we must vacate/undo any pending landing and lift off if needed.
	 *
	 * @param OwnerPackingUp The owner that is transitioning to vehicle state.
	 * @post Clears owner on the aircraft; cancels landing/waiting; ensures we end up airborne/idle.
	 */
	void OnOwnerStartPackingUp(const UAircraftOwnerComp* OwnerPackingUp);

	/**
	 * @brief Lift off either via the owner (bay-orchestrated) or directly when orphaned.
	 *
	 * @post If owner exists, VTO is granted after doors open; otherwise we start VTO immediately.
	 */
	UFUNCTION(BlueprintCallable)
	void TakeOffFromGroundOrOwner();

	/** @return The current owner component or nullptr if orphaned. */
	UAircraftOwnerComp* GetOwnerAircraft() const;

	// Cosmetic for non gameplay actions.
	void StartVerticalLanding_Preview();

	void SetPreviewTakeOffHeight(const float NewTakeOffHeight);

	/**
	 * @brief Gather all weapon states for external systems (e.g., targeting UI or debugging).
	 * @return Valid weapon state pointers only (invalid entries are filtered out).
	 */
	TArray<UWeaponState*> GetAllAircraftWeapons() const;
	float GetHighestAircraftWeaponRange() const;
	bool GetIsIdleAndAirborne() const;
	bool GetIsAircraftAirborne() const;

	/** @return Bomb component if present; allows attack-path points to arm/disarm bombs. */
	UBombComponent* GetBombComponent() const;

	/** @return The component that runs the burning crash once the aircraft is shot down. */
	UAircraftDeathComponent* GetDeathComponent() const { return M_DeathComponent; }

	/** @return Current landed state enum (routing hint for animation/weapon/movement). */
	inline EAircraftLandingState GetLandedState() const { return M_LandedState; }

	/** @return Height between the landed position and the airborne position after a vertical take-off. */
	float GetTakeOffHeight() const { return M_AircraftMovementSettings.TakeOffHeight; }

	/** @return Terrain following state, for map tests and debugging. */
	const FAircraftTerrainFollow& GetTerrainFollow() const { return M_TerrainFollow; }

	// Map tests and debugging: one line with the movement state, attack path point, strafe and weapon state.
	FString DescribeFlightForTests() const;

	/** @return Cruise speed of move and attack flights (cm/s), for map tests. */
	float GetMaxMoveSpeedForTests() const { return M_AircraftMovementSettings.MaxMoveSpeed; }

	/** @return Idle / moving / attacking, for map tests. */
	EAircraftMovementState GetMovementStateForTests() const { return M_MovementState; }

	/** @return Whether the aircraft flies into or out of an attack dive, where terrain following must not act. */
	bool GetIsFlyingAttackDive() const
	{
		return M_MovementState == EAircraftMovementState::AttackMove && M_AircraftAttackData.Path.GetIsFlyingDiveSegment();
	}

	/** @brief Blueprint hook for weapon fire montage per index/mode; owned by aircraft animation pipeline. */
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnPlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode);

	/**
	 * @brief Temporarily retunes attack-ground behavior to sweep fire between two ground points.
	 *
	 * @param StrafeAircraftSettings Defines the strafe endpoints, duration, and post-strafe move.
	 * @param OverrideAttackMoveSettings Attack path tuning used only while the timed strafe is active.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void StrafeLocation(const FStrafeAircraftSettings& StrafeAircraftSettings,
	                    const FAircraftAttackMoveSettings& OverrideAttackMoveSettings);

	/**
	 * @brief Starts a one-off carpet run without entering the regular attack/strafe state machines.
	 * @param StartCarpetLocation First point the aircraft reaches before releasing bombs.
	 * @param EndCarpetLocation Opposite run endpoint used while bombs target the aircraft ground projection.
	 * @param RetreatLocation Final point where the aircraft destroys itself after bombing.
	 */
	UFUNCTION(BlueprintCallable)
	void CarpetBombing(const FVector& StartCarpetLocation, const FVector& EndCarpetLocation, const FVector& RetreatLocation);

	UFUNCTION(BlueprintCallable)
	void CarpetBombing_RetreatAndDestroy(const FVector& RetreatLocation);

	/**
	 * @brief Orders this aircraft through the one-shot global-ability drop pipeline without affecting regular commands.
	 * @param DropRequest Payload, landing, stay, spawn, and retreat data owned by this drop run.
	 */
	void OrderAircraftDrop(const FAircraftDropRequest& DropRequest);

	/**
	 * @brief One-shot global-ability flight: flies a straight line at max move speed and constant height from the
	 * current location through PassOverLocation to EndLocation, then destroys itself. Bypasses the bezier paths.
	 * @param PassOverLocation Point on the line (at flight height) that fires OnStraightRunPassedPoint once.
	 * @param EndLocation Run end; the aircraft is destroyed when it gets there.
	 */
	void OrderStraightFlightRun(const FVector& PassOverLocation, const FVector& EndLocation);

	// Fired once when a straight flight run reaches its pass-over point.
	FOnAircraftStraightRunPassedPoint OnStraightRunPassedPoint;

	// Fired once per tank drop (rope-slung or not) after the tank is detached, placed on the ground, selectable and
	// has its weapons back.
	FOnAircraftDropPayloadReleased OnAircraftDropPayloadReleased;

	// Fired once per squad of a landed squad drop, the moment the squad has spawned at the aircraft (before its
	// walk out of the cargo door). Not fired when the aircraft is already gone by the time the squad spawns.
	FOnAircraftDropSquadReleased OnAircraftDropSquadReleased;

	/**
	 * @brief While held, a straight run keeps flying past its end point instead of destroying itself, so payload that
	 * still has to leave the aircraft (paratroopers) can do so; capped at a fixed distance past the end.
	 * @param bHoldAtEnd True to keep flying at the end, false to destroy as soon as the end has been passed.
	 */
	void SetStraightRunHoldAtEnd(bool bHoldAtEnd);

private:
	/**
	 * @brief Owner-agnostic VTO entry point (extracted) so owners can grant lift-off exactly when doors are open.
	 *
	 * @note Use when orphaned or when UAircraftOwnerComp finishes opening and explicitly triggers our VTO.
	 */
	void StartVto();

	/**
	 * @brief Begin the landing sequence once the owner confirms the bay is open and safe.
	 *
	 * @pre UAircraftOwnerComp has called this as part of RequestOpenBayDoorsForLanding flow.
	 * @post Anim instance receives time-boxed descent; LandedState switches to VerticalLanding.
	 */
	void StartVerticalLanding();

	/**
	 * @brief Owner assignment setter used exclusively by UAircraftOwnerComp.
	 *
	 * @param NewOwner New owner component.
	 * @note Only flips the pointer and abilities; it does not move or clear old owner data by itself.
	 * @note DO NOT CALL from gameplay code; AircraftOwnerComp controls assignment consistency.
	 */
	void SetOwnerAircraft(UAircraftOwnerComp* NewOwner);

	/**
	 * @brief Spawn-time gating to prevent immediate selection/command spam until the unit is ready.
	 *
	 * @param bSetDisabled If true the unit is hidden, not selectable, and collision disabled.
	 * @param TimeNotSelectable Cooldown (sec) before becoming selectable when enabling.
	 * @param MoveTo Optional move location to place the unit.
	 * @post Selection state and collision reflect the disabled flag; BP receives BP_OnRTSUnitSpawned.
	 */
	virtual void OnRTSUnitSpawned(
		const bool bSetDisabled,
		const float TimeNotSelectable = 0.f,
		const FVector MoveTo = FVector::ZeroVector) override;

protected:
	virtual void BeginPlay() override;
	virtual void CheckForUpgrades();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent,  Category = "Bomb")
	UMeshComponent* GetBombOverrideMeshComp();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
	virtual void PostInitializeComponents() override;

	/** @brief Shot down: stops every action and frees its bay, then lets the death component burn and crash it;
	 * the actor goes once the wreck has hit the ground. */
	virtual void UnitDies(const ERTSDeathType DeathType) override;

	/** @return Whether anti-air may target this aircraft: it is then registered with the game unit manager whatever
	 * its Blueprint says. Scripted aircraft that are no combat units override this. */
	virtual bool GetIsTargetableByAntiAir() const { return true; }

	// --------------------------
	// -- Start Blueprint Updates
	// --------------------------
	/** @brief BP hook: announce we entered VTO preparation (doors/FX/countdown UI). */
	UFUNCTION(BlueprintImplementableEvent, Category="VTO")
	void BP_OnPrepareVto();

	/** @brief BP hook: announce VTO motor spin-up / ascent start. */
	UFUNCTION(BlueprintImplementableEvent, Category="VTO")
	void BP_OnVtoStart();

	/** @brief BP hook: announce VTO finished and we are airborne. */
	UFUNCTION(BlueprintImplementableEvent, Category="VTO")
	void BP_OnVtoCompleted();

	/** @brief BP hook: announce the landing sequence started (doors open & timed descent). */
	UFUNCTION(BlueprintImplementableEvent, Category="Landing")
	void BP_OnLandingStart();

	/** @brief BP hook: announce the landing completed (inside and docked). */
	UFUNCTION(BlueprintImplementableEvent, Category="Landing")
	void BP_OnLandingCompleted();
	// --------------------------
	// -- END Blueprint Updates
	// --------------------------

	/**
	 * @brief One-time init after components are spawned/assigned (selection decal, etc.).
	 *
	 * @param SelectionDecal Decal projected to ground while airborne. Errors are reported if invalid.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitAircraft(UDecalComponent* SelectionDecal);

	/** @brief Movement tuning values (idle radius, VTO accel, hover bob, etc.). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FAircraftMovementData M_AircraftMovementSettings;

	/** @brief Route Tick to the active phase (VTO, Landing, Move, Attack, Idle circle/hover). */
	virtual void Tick(float DeltaSeconds) override;

	// ---------- Commands ----------
	/** @brief User Move command; if landed it queues a post-VTO move, otherwise starts pathing immediately. */
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;

	/** @brief Cancel user Move, clear queued post-VTO move, and restore Idle if we were mid-path. */
	virtual void TerminateMoveCommand() override;

	/** @brief User Attack command; same queueing semantics as Move, but pathing targets an actor. */
	virtual void ExecuteAttackCommand(AActor* TargetActor) override;

	/** @brief User Attack Ground command; queues or starts an attack run against a fixed world location. */
	virtual void ExecuteAttackGroundCommand(const FVector GroundLocation) override;

	/** @brief Cancel user Attack Ground using the same cleanup as actor attacks. */
	virtual void TerminateAttackGroundCommand() override;

	/** @brief Cancel user Attack, stop fire/bombs if active, and restore Idle if we were mid-path. */
	virtual void TerminateAttackCommand() override;

	/**
	 * @brief Return-to-base: navigate above bay, request door open, then land via timed descent.
	 *
	 * @post If already landed → completes immediately. If mid-VTO → switches to landing. Otherwise starts RTB nav.
	 */
	virtual void ExecuteReturnToBase() override;

	/** @brief Cancel RTB cleanly, including pending door waits and landing animation, and return to airborne/idle. */
	virtual void TerminateReturnToBase() override;

	/**
	 * @brief Let Blueprints set subtype/weapons/FX once before gameplay begins.
	 * @note Keep heavy asset wiring here to keep C++ BeginPlay lean and deterministic.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void BP_BeginplayInitAircraft();

	/** @brief Weapon component driving attached guns and their fire logic. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UAircraftWeapon> M_AircraftWeapon;

	UPROPERTY()
	TObjectPtr<UAircraftTargetAcquisition> M_TargetAcquisition;
	bool GetIsValidTargetAcquisition() const;

	// Burning fall and breakup; its settings can be changed per aircraft Blueprint.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Death")
	TObjectPtr<UAircraftDeathComponent> M_DeathComponent;
	bool GetIsValidDeathComponent() const;


	// ----------------- START EXPERIENCE INTERFACE -----------------
	/** @brief Tracks XP, notifies level ups, and exposes player-facing stats. */
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	TObjectPtr<URTSExperienceComp> ExperienceComponent;

	/** @return Experience component used by the interface. */
	virtual URTSExperienceComp* GetExperienceComponent() const override final;

	/** @brief Level-up hook: propagate to BP for VFX, UI, and stat refresh. */
	virtual void OnUnitLevelUp(const int32 Level, const EVeterancyIconSet IconSetUsed) override final;

	/** @brief XP source hook: award XP when we kill something. */
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnUnitLevelUp();

	/** @brief Notify the exp system on any aircraft kill. */
	virtual void OnAnyActorKilledByAircraft(AActor* KilledActor);
	// ----------------- END EXPERIENCE INTERFACE -----------------


	/** @brief BP spawn hook to toggle initial enabled/disabled presentation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitSpawning")
	void BP_OnRTSUnitSpawned(const bool bDisabled);

	/** @brief Handles ammo/repair/landed-state callbacks for reload stations/owners. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload Manager")
	FAircraftReloadManager ReloadManager;

private:
	/** @brief Clear owner info and associated ability; used when leaving or on owner death. */
	void ClearOwnerAircraft();

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_AircraftMesh;
	/** @return Whether the aircraft mesh exists; reports error otherwise. */
	bool EnsureAircraftMeshIsValid() const;

	UPROPERTY()
	TObjectPtr<UAircraftAnimInstance> M_AnimInstAircraft;
	/** @return Whether the anim instance exists and derives from UAircraftAnimInstance. */
	bool EnsureAnimInstanceIsValid() const;

	UPROPERTY()
	TObjectPtr<UBombComponent> M_AircraftBombComponent;
	/** @return Whether a bomb component is present. */
	bool IsValidBombComponent() const;

	/** @brief Authoritative vertical/landing phase (tick routing + anim/weapon gating). */
	EAircraftLandingState M_LandedState = EAircraftLandingState::None;

	/** @brief Update phase and propagate to FOW/weapons/anim to keep them consistent. */
	void UpdateLandedState(const EAircraftLandingState NewState);

	/** @brief Full airborne vision radius from aircraft data, cached so landed state can temporarily scale it. */
	float M_FullVisionRadius = 0.f;

	/** @return Whether the inherited FOW component exists; reports setup errors otherwise. */
	bool EnsureFowComponentIsValid() const;

	/** @brief Cache the aircraft data vision radius and immediately apply the radius for the current landed state. */
	void SetFullVisionRadius(const float NewFullVisionRadius);

	/** @brief Apply the current landed-state FOW radius without changing the cached full radius. */
	void UpdateFowVisionRadiusForLandedState() const;

	/** @return Full radius while airborne/taking off; reduced radius only once fully landed. */
	float GetVisionRadiusForCurrentLandedState() const;

	/** @brief High-level horizontal move/attack/idle mode. */
	EAircraftMovementState M_MovementState = EAircraftMovementState::None;

	/** @brief Selection decal (projected to terrain) while airborne. */
	UPROPERTY()
	TObjectPtr<UDecalComponent> M_SelectionDecal;

	/** @brief Pawn movement driver with VTO/landing helpers. */
	UPROPERTY()
	TObjectPtr<UAircraftMovement> M_AircraftMovement;

	// Keeps track of from where the aircraft took of and with what rotation.
	// Also caches the calculated z value above this position the aircraft needs to reach before being tagged
	// as airborne.
	UPROPERTY()
	FAircraftLandedData M_AircraftLandedData;

	/**
	 * @brief The only way to set the cruise height from outside terrain following: the clearance above the ground is
	 * then captured again from the new height.
	 * @param NewTargetAirborneHeight Cruise height (world Z) to fly at from now on.
	 */
	void SetTargetAirborneHeight(const float NewTargetAirborneHeight);

	// Moves the cruise height (TargetAirborneHeight) with the terrain below; see Tick_TerrainFollow.
	FAircraftTerrainFollow M_TerrainFollow;

	/** @brief Starts the ground samples of this aircraft at a random moment so aircraft do not all trace together. */
	void BeginPlay_InitTerrainFollow();

	/**
	 * @brief Samples the ground at an interval and eases the cruise height, the aircraft and its current path up or
	 * down with it. Skipped while the aircraft aims (dives, bomb runs) or flies a height planned elsewhere.
	 */
	void Tick_TerrainFollow(const float DeltaTime);

	/** @return Whether the current flight may change height with the terrain. */
	bool TerrainFollow_GetIsAllowed() const;

	/** @brief Traces the ground below the aircraft and along its flight ahead, and hands it to M_TerrainFollow. */
	void TerrainFollow_SampleGround();

	/** @brief Moves the aircraft, its cruise height and the path it flies by DeltaZ, keeping all of them in step. */
	void TerrainFollow_ShiftCruiseHeight(const float DeltaZ);

	/** @return Nose pitch (degrees) that shows the current terrain climb or descent. */
	float TerrainFollow_GetClimbPitch() const;

	/**
	 * @brief Ability flights spawn at a height chosen for their target, often far off over other ground: keep the
	 * cruise height's clearance above the target's ground instead of above the spawn point.
	 * @param TargetLocation XY whose ground the clearance is measured from.
	 * @param MinClearance Lowest clearance kept; below it the aircraft climbs to it smoothly. 0 uses the terrain
	 * following's MinGroundClearance.
	 */
	void TerrainFollow_CaptureClearanceAboveTarget(const FVector& TargetLocation, const float MinClearance = 0.f);

	/**
	 * @brief Traces down to the landscape at one XY location; movable things that block the landscape channel (team
	 * weapons) are skipped so aircraft do not hop over them.
	 * @param Location XY to sample; Z is the centre of the trace.
	 * @param OutGroundZ Ground height when hit.
	 * @return Whether the landscape was hit.
	 */
	bool TraceLandscapeGroundZ(const FVector& Location, float& OutGroundZ) const;

	/** @brief Idle circling parameters (center, angle, grace-steering). */
	UPROPERTY()
	FAircraftIdleData M_AircraftIdleData;

	/** @brief Cached pathing inputs for Move command. */
	UPROPERTY()
	FAAircraftMoveData M_AircraftMoveData;

	/** @brief Cached pathing inputs for Attack command. */
	UPROPERTY()
	FAAircraftAttackData M_AircraftAttackData;

	/** @brief Deferred action (Move/Attack/ChangeOwner) executed right after lift-off. */
	FPendingPostLiftOffAction M_PendingPostLiftOffAction;

	/** @brief Dispatch Move/Attack/ChangeOwner once VTO completes to honor player intent. */
	void IssuePostLifOffAction();

	/** @brief Project the selection decal onto landscape asynchronously. */
	void Tick_ProjectDecalToLandscape(const float DeltaTime);

	/** @brief Time-integrated ascent during VerticalTakeOff phase; completes at TargetAirborneHeight. */
	void Tick_VerticalTakeOff(float DeltaTime);

	/** @brief Initialize idle circle once we switch to idle so banking is smooth and deterministic. */
	void InitIdleCircleFromCurrentPose();

	/** @brief Idle circling (or hover) behavior while airborne with no active command. */
	void Tick_IdleCircling(float DeltaTime);

	// -------------------------------------------------------------
	// ----------------------- Bank-to-turn flight -----------------------
	// -------------------------------------------------------------
	/** @return Whether move, attack and idle flights use the bank-to-turn flight model (not -AircraftLegacyFlight). */
	bool GetIsFlightModelActive() const;

	/**
	 * @brief Bank-to-turn path following: passes the points the aircraft has flown by (issuing the attack actions of
	 * each new point), then aims a little ahead along the path.
	 * @param Path Move or attack path.
	 * @param bIsAttackPath Attack paths issue fire / bomb actions per point and end in OnAttackMoveCompleted.
	 * @param DeltaTime Frame time.
	 */
	void Tick_FlyPath(FAircraftPath& Path, const bool bIsAttackPath, const float DeltaTime);

	/** @return False once the path ended this frame (its completion may already have started a new one). */
	bool FlyPath_AdvancePassedPoints(FAircraftPath& Path, const bool bIsAttackPath);

	/**
	 * @brief The frame a path ends still flies: idle circling or the new path that the completion started, so the
	 * turn never freezes for a frame. Nothing when the aircraft hovers, lands or is destroyed.
	 */
	void FlyPath_ContinueAfterPathEnded(const float DeltaTime);

	/** @return How far ahead along the path or idle circle the aircraft aims at its current speed (cm). */
	float GetFlightLookAheadDistance(const bool bIsDiving = false) const;

	/** @brief One bank-to-turn frame along a path: aim ahead, dive floor and altitude hold outside dives. */
	void FlyAlongPath(const FAircraftPath& Path, const float DeltaTime);

	/** @return Cruise speed, or less on the way into a hover (return to base, drop point). */
	float FlyPath_GetWantedSpeed(const FAircraftPath& Path) const;

	/** @return Braking rate into a hover: the comfortable rate, harder when the path left is too short for it. */
	float FlyPath_GetArrivalDeceleration(const FAircraftPath& Path) const;

	/**
	 * @brief Raises the dive floor to stay clear of the ground under the aircraft (sampled while diving), not only
	 * of the target's ground; only limits the descent, so the dive's aim is not changed above the floor.
	 * @param PathFloorZ The dive segment's own floor.
	 * @param DeltaTime Frame time, for the sample timer.
	 * @return The floor to fly this frame.
	 */
	TOptional<float> FlyPath_GetDiveFloorAboveGroundBelow(const TOptional<float>& PathFloorZ, const float DeltaTime);

	/** Ground under a diving aircraft, sampled now and then and eased between samples, for the dive floor. */
	struct FAircraftDiveGroundFloor
	{
		// Ground height the floor follows; moves toward the last sample at a capped speed, so the floor never steps.
		float GroundZ = 0.f;
		float SampledGroundZ = 0.f;
		float SecondsToNextSample = 0.f;
		bool bM_HasGround = false;

		void Reset()
		{
			SecondsToNextSample = 0.f;
			bM_HasGround = false;
		}
	};

	FAircraftDiveGroundFloor M_DiveGroundFloor;

	/**
	 * @brief One bank-to-turn frame toward a point.
	 * @param AimPoint Where to fly toward.
	 * @param WantedSpeed Speed to reach and keep (cm/s).
	 * @param FloorZ Dive floor, when one applies.
	 * @param bHoldAltitude Cruise and idle: ease to the aim height instead of pointing the nose at the aim point.
	 * @param DeltaTime Frame time.
	 * @param bIsArriving Slowing into a hover: brakes at the arrival deceleration.
	 * @param ArrivalDeceleration Braking rate of the arrival (at least the settings' ArrivalDeceleration).
	 */
	void FlyTowardPoint(const FVector& AimPoint, const float WantedSpeed, const TOptional<float>& FloorZ,
	                    const bool bHoldAltitude, const float DeltaTime, const bool bIsArriving = false,
	                    const float ArrivalDeceleration = 0.f);

	/** @brief Gives a new move or attack path the turn radius it is planned with and the side already turned to. */
	void PreparePathPlanning(FAircraftPath& Path);

	// Attack plans since the last dive: after a few the dive checks relax, so the aircraft never circles forever.
	int32 M_AttackPlansWithoutDive = 0;

	/** @brief PostInitializeComponents: an aircraft spawned in the air starts at idle speed along its nose. */
	void PostInit_SetAirborneSpawnSpeed() const;

	/** @return Flight heading (velocity, or the nose when nearly still) as a level rotation for path planning. */
	FRotator GetFlightPlanningRotation() const;

	/** @brief Puts the idle circle beside the flight, tangent to it on the side the aircraft already turns to. */
	void InitIdleCircleAlongFlight();

	/** @brief Bank-to-turn idle circling: aims a little ahead on the circle at the idle speed. */
	void Tick_IdleCircling_FlightModel(float DeltaTime);

	struct FIssuedActionsState
	{
		bool bM_AreWeaponsActive = false;
		bool bM_AreBombsActive = false;
	};

	FIssuedActionsState IssuedActionsState;


	// -------------------------------------------------------------
	// ----------------------- VTO Start -----------------------
	// -------------------------------------------------------------
	/**
	 * @brief Arm the prepare-timer and fire OnPrepareVto/OnVtoStart depending on delay.
	 *
	 * @param TimeTillStartVto Delay before VTO start; 0 fires immediately.
	 * @post Prevents re-arming while already taking off to avoid double VTO.
	 */
	void SetVtoTimer(const float TimeTillStartVto);

	/** @brief Notify animation/UI we entered preparation; shows “Preparing for takeoff” vertical text. */
	void OnPrepareVto(const float PrepTime);

	/** @brief Switch LandedState to VerticalTakeOff and start ascent with a computed duration. */
	void OnVtoStart();

	/** @brief Finish ascent, set airborne, hand control back to movement and post-VTO action. */
	void OnVtoCompleted(const float TargetZ);

	// -------------------------------------------------------------
	// ----------------------- VTO END-----------------------
	// -------------------------------------------------------------

	/** @brief Resolve anim instance, init aircraft weapon, and find optional bomb component. */
	void PostInit_InitWeaponInitAnim();

	// -------------------------------------------------------------
	// ----------------------- Return To Base Start -----------------------
	// -------------------------------------------------------------
	/**
	 * @brief Begin RTB by moving to the air position above our assigned bay; owner gates the actual landing.
	 *
	 * @note This isolates navigation (our job) from door arbitration (owner’s job).
	 */
	void StartNavigatingToBase();

	/** @brief Called after the Move-To above bay completes; requests door opening and transitions to landing/wait. */
	void OnReturnToBase_MoveCompleted();

	/** @return Whether we are already fully landed (RTB becomes a no-op). */
	bool ExecuteReturnToBase_IsAlreadyLanded() const;

	/** @return Whether we are currently in VTO (RTB should flip into landing rather than navigate). */
	bool ExecuteReturnToBase_IsVto() const;

	/** @brief Kill movement and enter hover-wait while doors open; also align to landed rotation. */
	void SetAircraftToHoverWaiting();

	/** @brief Bob in place while waiting for the bay to open. */
	void Tick_WaitingForBayOpening(const float DeltaTime);

	float M_HoverWaitBaseZ = 0.f;
	float M_HoverWaitPhase = 0.f;

	/** @brief Guard against impossible state mixes when cancelling RTB (debug visibility). */
	void TerminateReturnToBase_CheckLandedAndMovementState() const;

	/** @brief If landing was cancelled mid-descent, recover by VTO back to airborne at our cached target height. */
	void CancelReturnToBase_VtoBackToAirborne();

	// -------------------------------------------------------------
	// ----------------------- Return To Base End -----------------------
	// -------------------------------------------------------------

	// -------------------------------------------------------------
	// ----------------------- Move To Start -----------------------
	// -------------------------------------------------------------
	/** @brief Start spline/path flight to the cached Move target. */
	void StartMoveTo();

	/**
	 * @brief Compute the normalized start pose for pathing, nudging height first if needed for safe bank angles.
	 *
	 * @param OutRotation Output starting rotation (pitch baked for climb/descend recovery).
	 * @param CurrentLocation Current actor location.
	 * @param CurrentRotation Current actor rotation.
	 * @return Start location for the path solver at target airborne height.
	 *
	 * @note WHY: Prevents sudden vertical snaps by blending climb/descend into the first segment at bounded pitch.
	 */
	FVector OnMoveStartGetLocationAndRotation(FRotator& OutRotation, const FVector& CurrentLocation,
	                                          const FRotator& CurrentRotation);

	/** @brief Per-frame homing toward the next Move path point; advances points and completes on final. */
	void Tick_MoveTo(float DeltaTime);

	/** @brief Choose Z for “move target” depending on phase (landed → lift height, airborne → cached airborne height). */
	float GetMoveToZValue() const;

	/**
	 * @brief Segment progress helper for path advancing/overshoot handling.
	 *
	 * @param StartSegment Segment start.
	 * @param EndSegment Segment end.
	 * @param OutDistanceToNext Remaining distance to end.
	 * @return [0..1] fraction traveled along the segment.
	 */
	float GetRatioSegmentCompleted(const FVector& StartSegment, const FVector& EndSegment,
	                               float& OutDistanceToNext) const;

	/**
	 * @brief When overshooting a path point, adjust rotations so the aircraft “curves back” instead of zig-zagging.
	 *
	 * @param InOvershotPoint Path point we just passed; becomes our new local origin.
	 * @param InFutureTargetPoint Next path point to steer to; gets its rotation adjusted.
	 * @note WHY: Maintains visual smoothness by preventing hard flips after overshoot on high speeds.
	 */
	void OnPointOvershot_AdjustFutureRotation(FAircraftPathPoint& InOvershotPoint,
	                                          FAircraftPathPoint& InFutureTargetPoint) const;

	/** @brief Cleanup & switch back to Idle after Move-To completes (non-RTB). */
	void CleanUpMoveAndSwitchToIdle();

	/** @brief Move-To completion handler: either finishes Move command or continues RTB flow. */
	void OnMoveCompleted();
	// -------------------------------------------------------------
	// ----------------------- Move To END -----------------------
	// -------------------------------------------------------------

	// -------------------------------------------------------------
	// ----------------------- Attack move Start -----------------------
	// -------------------------------------------------------------
	/** @brief Begin attack pathing against a visible target (dives/out-of-dives emit weapon/bomb actions). */
	void StartAttackActor();

	/** @brief Begin attack pathing against the cached ground target location. */
	void StartAttackGround();

	/** @brief Per-frame homing for attack path; manages action handoff on point type changes. */
	void Tick_AttackMove(float DeltaTime);

	/** @brief Issue fire/bomb actions when entering new point types (dive/out-of-dive). */
	void AttackMove_IssueActionForNewTargetPoint(const FAircraftPathPoint& NextPoint);

	/** @brief Stop any previous continuous action when we switch to a point type that shouldn’t keep it. */
	void StopIssuedActionForNewType(const EAirPathPointType NewType);

	/** @brief Stop bombs (if any) and update state. */
	void StopBombThrowing();

	/** @brief Start weapon fire on attack dive entry. */
	void StartWeaponFire();

	/** @brief Fan out fire command to all attached weapons with current target context. */
	void FireAttachedWeapons() const;

	/** @brief Start bomb throwing for the current attack target. */
	void StartBombThrowing(AActor* TargetActor);

	/** @brief Start bomb throwing for the current attack ground location. */
	void StartBombThrowingAtLocation(const FVector& TargetLocation);

	/** @brief Stop weapon fire and update state. */
	void StopWeaponFire();

	/** @return Whether the homing tick should target a location (true on final segments and dive phases). */
	static bool GetIsHomingTargetLocation(const FAircraftPathPoint& NextPoint,
	                                      const bool bIsMovingToFinalPoint);

	/** @brief Attack path completed; either re-path if target still visible or finish the Attack command. */
	void OnAttackMoveCompleted();

	/** @brief Cleanup & switch back to Idle after Attack completes or is cancelled. */
	void CleanUpAttackAndSwitchToIdle();


	// -------------------------------------------------------------
	// ----------------------- Strafe Start -----------------------
	// -------------------------------------------------------------
	struct FAircraftStrafeState
	{
		FVector StrafeStartLocation = FVector::ZeroVector;
		FVector StrafeEndLocation = FVector::ZeroVector;
		FVector ActiveLerpStartLocation = FVector::ZeroVector;
		FVector ActiveLerpEndLocation = FVector::ZeroVector;
		// Reset at recovery start so repeated custom strafes keep engaging the authored target instead of drifting.
		FVector NextRunStartLocation = FVector::ZeroVector;
		FVector PostStrafeMoveToLocation = FVector::ZeroVector;
		float StrafePointTotalLerpTime = 0.f;
		float CurrentLerpTime = 0.f;
		// How far along the active run the guns aim, from where the aircraft is; only ever moves forward.
		float FlightProgressAlpha = 0.f;
		// Cached from the ordered lane and reused for orientation-based follow-up runs.
		float StrafePointDistance = 0.f;
		bool bM_IsStrafing = false;
		bool bM_IsLerpingStrafePoint = false;
		bool bM_IsFirstAttackRun = true;
		// An attack path from far away is only an approach without a dive; such a path does not count as a run.
		bool bM_HasDivedThisRun = false;
		bool bM_IsRetreatingToDestroy = false;
		// Keeps attacking the authored target instead of switching to aggro after the first run.
		bool bM_RepeatRunsOverTarget = false;
		FTimerHandle StrafeTimerHandle;

		void ResetRuntime()
		{
			CurrentLerpTime = 0.f;
			FlightProgressAlpha = 0.f;
			bM_HasDivedThisRun = false;
			bM_IsStrafing = false;
			bM_IsLerpingStrafePoint = false;
			bM_IsFirstAttackRun = true;
			StrafeTimerHandle.Invalidate();
		}
	};

	enum class EAircraftCarpetBombingPhase : uint8
	{
		Inactive,
		MovingToStart,
		BombingToEnd,
		BombingToStart,
		Retreating
	};

	struct FAircraftCarpetBombingState
	{
		FVector StartCarpetLocation = FVector::ZeroVector;
		FVector EndCarpetLocation = FVector::ZeroVector;
		FVector RetreatLocation = FVector::ZeroVector;
		EAircraftCarpetBombingPhase Phase = EAircraftCarpetBombingPhase::Inactive;

		void ResetRuntime()
		{
			Phase = EAircraftCarpetBombingPhase::Inactive;
		}
	};

	FAircraftCarpetBombingState M_CarpetBombingState;

	struct FAircraftStraightRunState
	{
		FVector StartLocation = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		// Distances along Direction from StartLocation.
		float PassOverDistance = 0.f;
		float EndDistance = 0.f;
		bool bM_IsActive = false;
		bool bM_HasPassedPoint = false;
		bool bM_HoldAtEnd = false;
	};

	// Owns the aircraft's movement while active; Tick routes here before every other movement state.
	FAircraftStraightRunState M_StraightRunState;
	void Tick_StraightRun(float DeltaTime);
	void StraightRun_ApplyFullSpeed() const;

	FAircraftDropRequest M_AircraftDropRequest;
	FTimerHandle M_AircraftDropStayLandedTimerHandle;
	void AircraftDrop_MoveToDropLocation();
	void AircraftDrop_OnMoveCompleted();
	void AircraftDrop_OnLanded();
	void AircraftDrop_OnStayLandedFinished();
	void AircraftDrop_OnVtoCompleted();
	void AircraftDrop_SpawnSquads() const;
	void AircraftDrop_SpawnSquad(ESquadSubtype SquadSubtype, int32 SquadIndex) const;
	// Spawns the squads one after another at the cargo door, so they file out instead of stacking in the door.
	void AircraftDrop_SpawnSquadsThroughCargoDoor() const;
	/** @return The navmesh point at the mesh's cargo door socket; the landing point when the mesh has no such socket. */
	FVector AircraftDrop_GetProjectedCargoDoorLocation() const;
	/**
	 * @brief Sends a squad that appeared at the cargo door to its spot outside the aircraft; from then on it is a
	 * normal unit (the move is an ordinary command the player can override).
	 * @param SpawnedSquad The squad controller the async spawner made.
	 * @param DoorLocation Where the squad appeared.
	 * @param OutsideLocation Its spot on the RadiusSpawnSquads circle.
	 */
	static void AircraftDrop_OrderSquadWalkOut(AActor* SpawnedSquad, const FVector& DoorLocation,
	                                           const FVector& OutsideLocation);
	void AircraftDrop_AttachTank();
	void AircraftDrop_DetachTank();
	void AircraftDrop_RegisterEnemyPayloadUnit(AActor* DroppedUnit) const;
	UEnemyStrategicAIComponent* AircraftDrop_GetEnemyStrategicAIComponent() const;
	void AircraftDrop_PlayDropOffSound() const;
	FVector AircraftDrop_GetProjectedSquadSpawnLocation(int32 SquadIndex) const;

	// ----------------------- Rope-slung tank drop -----------------------
	// Timed, eased flight of one rope-sling phase; the phase itself is M_AircraftDropRequest.State
	// (VerticalLanding = descent until the payload touches down, LandedWaiting = hover, VerticalTakeOff = climb).
	struct FAircraftRopeSlingFlightState
	{
		FVector StartLocation = FVector::ZeroVector;
		FRotator StartRotation = FRotator::ZeroRotator;
		FVector TargetLocation = FVector::ZeroVector;
		float Elapsed = 0.f;
		float Duration = 0.f;
		float HoverBaseZ = 0.f;
		float HoverPhase = 0.f;
	};

	FAircraftRopeSlingFlightState M_RopeSlingFlight;

	// Only exists while a rope-slung payload is carried or its ropes winch up; destroys itself afterwards.
	UPROPERTY()
	TObjectPtr<UCargoRopeSlingComponent> M_CargoRopeSling;

	/** @return Whether rope visuals exist; not an error, only rope-slung drops create them. */
	bool GetHasCargoRopeSling() const;

	/** @return Whether a rope-slung drop owns the aircraft's movement (descent, hover or climb). */
	bool AircraftDrop_GetIsRopeSlingFlightActive() const;

	void AircraftDrop_CreateRopeSling(ATankMaster* Tank);
	void AircraftDrop_DestroyRopeSling();

	/** @return Cruise Z that keeps the slung payload clear of the ground between here and the drop point. */
	float AircraftDrop_GetRopeSlingCruiseZ() const;

	/** @return Highest ground Z sampled on the straight line from the aircraft to the drop point. */
	float AircraftDrop_GetHighestGroundZAlongApproach() const;

	void AircraftDrop_StartRopeSlingDescent();
	void AircraftDrop_OnRopeSlingPayloadGrounded();
	void AircraftDrop_StartRopeSlingClimb();

	/**
	 * @brief Starts one eased rope-sling flight phase from the current pose.
	 * @param TargetLocation Where the aircraft origin ends.
	 * @param Speed Average speed in cm/s; sets the duration.
	 * @param MinDuration Lower bound of the duration in seconds.
	 */
	void AircraftDrop_StartRopeSlingPhase(const FVector& TargetLocation, const float Speed, const float MinDuration);

	/** @return Eased 0..1 progress of the current rope-sling phase after advancing it by DeltaTime. */
	float AircraftDrop_AdvanceRopeSlingPhase(const float DeltaTime);

	void Tick_RopeSlingDrop(const float DeltaTime);
	void Tick_RopeSlingDescent(const float DeltaTime);
	void Tick_RopeSlingHover(const float DeltaTime);
	void Tick_RopeSlingClimb(const float DeltaTime);

	/** @brief Puts a released rope-slung tank exactly on the drop point, upright with its current heading. */
	void AircraftDrop_PlaceRopeSlungTankOnGround(ATankMaster* Tank) const;

	/** @return False for a nomadic vehicle in building form, whose hidden chassis must not start simulating. */
	static bool AircraftDrop_ShouldEnablePayloadPhysics(const ATankMaster* Tank);

	FAircraftStrafeState M_StrafeState;
	FAircraftAttackMoveSettings M_PreStrafeAttackMoveSettings;

	void Strafe_StartTimer(const float TotalStrafeTime);
	void Strafe_OnTimerFinished();
	void Strafe_TickTargetLocation(const float DeltaTime);
	// Aims the guns where the aircraft's own flight will put them on the run line, so fire walks with the aircraft.
	void Strafe_TickTargetFromFlightProgress();
	void Strafe_UpdateWeaponTargetLocation() const;
	FVector Strafe_GetOrientedEndLocation(const FVector& CurrentTargetLocation) const;
	FVector Strafe_GetNextRunEndLocation() const;
	void Strafe_StartAttackRun();
	// Makes the run's dive steep and long enough that the nose (and so the fixed guns) points at the ground.
	void Strafe_ApplyDiveProfile();
	void IssuePostLiftOffAttackGroundAction();
	void Strafe_OnAttackDiveStarted();
	void Strafe_OnDiveRecoveryStarted();
	void Strafe_PrepareNextAttackRun();
	void Strafe_CompleteInitialRunAndEnableAggro();
	void Strafe_StopAndRestoreSettings();
	void Strafe_ClearTimer();
	void CarpetBombing_MoveToLocation(const FVector& TargetLocation);
	void CarpetBombing_StartRunToEnd();
	void CarpetBombing_StartRunToStart();
	void CarpetBombing_HandleMoveCompleted();
	void CarpetBombing_HandleCompletedRunToEnd();
	void CarpetBombing_HandleCompletedRunToStart();
	void CarpetBombing_UpdateBombTargetLocation() const;
	bool CarpetBombing_HasBombsRemaining() const;

	// -------------------------------------------------------------
	// ----------------------- Strafe END -----------------------
	// -------------------------------------------------------------

	// -------------------------------------------------------------
	// ----------------------- Attack move END -----------------------
	// -------------------------------------------------------------

	/** @brief BP and data pulls to finalize gameplay setup after BeginPlay. */
	void BeginPlay_InitAircraft(UMeshComponent* OverrideBombComponentMesh);

	/** @brief Push initial LandedState into weapons/anim to keep them consistent on spawn. */
	void BeginPlay_PropagateStateToWpAndAnimInst() const;

	/** @brief Removes movement blockers from every aircraft primitive while retaining enemy weapon hits and selection. */
	void BeginPlay_SetupWeaponHitCollision() const;


	// -------------------------------------------------------------
	// ----------------------- Ability Swap Start -----------------------
	// -------------------------------------------------------------
	/** @brief Replace “no owner” ability with RTB when we gain an owner (player affordance). */
	void SwapAbilityToReturnToBase();

	/** @brief Replace RTB with “no owner” ability when we lose an owner. */
	void SwapAbilityToNoOwner();
	// -------------------------------------------------------------
	// ----------------------- Ability Swap End-----------------------
	// -------------------------------------------------------------

	/** @return Whether movement component is valid; reports one error at source. */
	bool EnsureAircraftMovementIsValid() const;

	/** @return Whether selection decal exists; returns false silently when absent. */
	bool EnsureSelectionDecalIsValid() const;

	/** @return Whether experience component is valid; reports one error at source. */
	bool EnsureExperienceComponentIsValid() const;

	/** @return True if idle behavior is to hover in place instead of circling. */
	bool IsIdleHover() const;

	/** @brief Async trace callback to place the selection decal on landscape. */
	void OnLandscapeTraceHit(const FHitResult& TraceHit) const;

	/** @brief Switch to idle movement (sets up circle center and grace steering). */
	void SetMovementToIdle();


	/** @return Whether weapon component is valid; reports one error at source. */
	bool EnsureAircraftWeaponIsValid() const;

	/** @brief Propagate landed state to weapons so they can gate fire logic. */
	void UpdateAircraftWeaponWithLandedState() const;

	/** @brief Propagate landed state to anim instance for correct blends and timers. */
	void UpdateAircraftAnimationWithLandedState() const;


	// -------------------------------------------------------------
	// ----------------------- Change Owner Start -----------------------
	// -------------------------------------------------------------
	/** @return Whether a change-owner request is admissible (not full/packed and not same owner). */
	bool EnsureValidChangeOwnerRequest(TWeakObjectPtr<UAircraftOwnerComp> NewOwner) const;

	/**
	 * @brief Clear old owner safely and then transition toward the new owner.
	 *
	 * @param NewOwner Target owner; if we are landed we open doors then VTO and switch post-lift-off.
	 */
	void ChangeOwner_FirstClearOldOwner(UAircraftOwnerComp* NewOwner);

	/** @brief Executed after lift-off if the queued action is ChangeOwner. */
	void PostLiftOff_ChangeOwner();

	/**
	 * @brief Remove our record from the old owner and set the new one (also updates landing cache).
	 *
	 * @param NewOwner Owner to take us over.
	 * @return True on success; false keeps us airborne idle to avoid half-assignment.
	 */
	bool RemoveDataOnOldOwner_SetNew(UAircraftOwnerComp* NewOwner);

	/**
	 * @brief Refresh the precise landed transform for the new owner’s socket so landing is exact.
	 *
	 * @param NewOwner New owner that holds the assigned socket.
	 */
	void UpdateLandingPositionForNewOwner(UAircraftOwnerComp* NewOwner);

	/** @brief Owner died while we were owned: kill movement, clear caches, and recover to airborne idle. */
	void OnOwnerDies_ResetToIdleAirborne();

	/** @brief Death: no more weapon fire, bombs, timed runs, commands or movement of its own. */
	void UnitDies_StopAllActions();

	/** @brief Death: the airbase gets its bay back and the reload work for this aircraft is dropped. */
	void UnitDies_ReleaseOwnerBay();

	/** @brief Death: out of the selection, no health bar or selection decal on the falling wreck. */
	void UnitDies_RemoveFromSelectionAndUI();

	/** @brief Death: a tank carried by a drop run goes down with the aircraft. */
	void UnitDies_DestroyCarriedTank();

	// -------------------------------------------------------------
	// ----------------------- Change Owner End -----------------------
	// -------------------------------------------------------------

	// ------------------------- Aircraft Owner Related Data ---------------------------
	/** @brief RTB bookkeeping (who/where/door-wait). */
	UPROPERTY()
	FAAircraftReturnToBaseData M_AircraftReturnData;

	/** @brief Weak owner pointer (assignment is managed by the owner component). */
	TWeakObjectPtr<UAircraftOwnerComp> M_AircraftOwner;

	/** @brief Socket index assigned by owner (INDEX_NONE if unassigned). */
	UPROPERTY()
	int32 M_AssignedSocketIndex = INDEX_NONE;

	/** @return True if owner pointer is valid; orphan is not an error. */
	bool GetIsValidAircraftOwner() const;

	/** @brief Finalize landing (state update + reload/repair callbacks + BP). */
	void OnLandingFinish();

	/** @brief Timed descent phase driver; snaps to Z and calls OnLandingFinish when done. */
	void Tick_VerticalLanding(float DeltaTime);


	/** @brief Spawn helper: enable, selection, and collision toggles with initial cooldown. */
	void OnRTSUnitSpawned_SetEnabled(const float TimeNotSelectable);

	/** @brief Spawn helper: fully disable presentation & selection for staged spawns or tutorials. */
	void OnRTSUnitSpawned_SetDisabled();

	/** @brief Vertical text helpers for UX. */
	void VerticalText_WaitForLanding() const;
	void VerticalText_PrepTakeOff() const;

	/** @brief Common spawn for vertical animated text (centralizes style). */
	void VerticalAnimatedText(const FVector& Location, const FString& InText,
	                          FRTSVerticalAnimTextSettings Settings) const;

	/**
	 * @brief Compute how long VTO should take given current Z, target Z, accel and caps.
	 *
	 * @param FromZ Start Z.
	 * @param ToZ Target airborne height Z.
	 * @return Duration in seconds; 0 if already at/above target.
	 *
	 * @note WHY: Keeps the ascents time-boxed and animation-driven rather than frame-rate dependent.
	 */
	float ComputeTimeToAirborne(float FromZ, float ToZ) const;

	/** @return Whether the prepare-VTO timer is currently active. */
	bool IsPreparingVTO() const;

	/**
	 * @brief Cancel the prepare phase and remain landed (used by RTB or bay/owner changes).
	 *
	 * @post Clears the timer and calls OnLandingFinish to restore landed state consistently.
	 */
	void CancelPrepareVTO_StayLanded();
};
