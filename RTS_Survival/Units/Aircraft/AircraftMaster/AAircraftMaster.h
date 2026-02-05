#pragma once

#include "CoreMinimal.h"
#include "AircraftCommandsData/AircraftCommandsData.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceComponent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/Units/Aircraft/AircraftReloadManager/AircraftReloadManager.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"

#include "AAircraftMaster.generated.h"

struct FRTSVerticalAnimTextSettings;
struct FAircraftReloadManager;
class UAircraftAnimInstance;
class UAircraftWeapon;
class UAircraftMovement;
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
UCLASS()
class AAircraftMaster : public ASelectablePawnMaster, public IExperienceInterface, public IRTSUnit
{
	GENERATED_BODY()
	AAircraftMaster(const FObjectInitializer& ObjectInitializer);

	friend class UAircraftOwnerComp;

public:

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

	/** @return Bomb component if present; allows attack-path points to arm/disarm bombs. */
	UBombComponent* GetBombComponent() const;

	/** @return Current landed state enum (routing hint for animation/weapon/movement). */
	inline EAircraftLandingState GetLandedState() const { return M_LandedState; }

	/** @brief Blueprint hook for weapon fire montage per index/mode; owned by aircraft animation pipeline. */
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnPlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
	virtual void PostInitializeComponents() override;

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


	// ----------------- START EXPERIENCE INTERFACE -----------------
	/** @brief Tracks XP, notifies level ups, and exposes player-facing stats. */
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	TObjectPtr<URTSExperienceComp> ExperienceComponent;

	/** @return Experience component used by the interface. */
	virtual URTSExperienceComp* GetExperienceComponent() const override final;

	/** @brief Level-up hook: propagate to BP for VFX, UI, and stat refresh. */
	virtual void OnUnitLevelUp() override final;

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

	/** @brief Update phase and propagate to weapons/anim to keep them consistent. */
	void UpdateLandedState(const EAircraftLandingState NewState);

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
	// ----------------------- Attack move END -----------------------
	// -------------------------------------------------------------

	/** @brief BP and data pulls to finalize gameplay setup after BeginPlay. */
	void BeginPlay_InitAircraft();

	/** @brief Push initial LandedState into weapons/anim to keep them consistent on spawn. */
	void BeginPlay_PropagateStateToWpAndAnimInst() const;


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
