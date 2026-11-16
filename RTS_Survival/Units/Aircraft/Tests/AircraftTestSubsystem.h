// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"

#include "AircraftTestSubsystem.generated.h"

class AAircraftMaster;
class ACPPTurretsMaster;
class ANomadicVehicle;
class UGlobalAbility;

UENUM()
enum class EAircraftTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	// -AntiAirValidate: player AA units must find, fire at and damage an enemy aircraft circling over them.
	AntiAirEngaging,
	// The same units, the aircraft gone, must turn on an enemy tank: anti-air guns fire at ground targets too.
	AntiAirGroundEngaging,
	// -AircraftDeathValidate: every aircraft is shot down in the air and must burn, crash with its speed and break up.
	AircraftDeathRunning,
	// -AircraftVtoValidate: a parked aircraft must take off vertically on its montages and fire its guns.
	AircraftVtoRunning,
	// -AircraftStrafeValidate: the player's Ho 229 strafing global ability must bring two Ho 229s that fire.
	AircraftStrafeRunning,
	// -AircraftDropValidate: the German vehicle drop abilities must land every vehicle they promise, in its exact variant.
	AircraftDropRunning,
	// -AircraftBaseTakeOffValidate: an aircraft parked in an airbase bay takes off, flies out, lands back and takes off again.
	AircraftBaseTakeOffRunning,
	// -AircraftTerrainFollowValidate: an airborne aircraft flies over the bumpiest route from the anchor and keeps its
	// clearance above the ground; then an attack dive, which terrain following must leave alone.
	AircraftTerrainFollowRunning,
	Finished
};

UENUM()
enum class EAircraftVtoTestStep : uint8
{
	// Parked; BeginPlay, the anim instance and the weapons settle before the checks.
	Settling,
	TakingOff,
	Firing,
	Done
};

/** The aircraft of -AircraftVtoValidate and what was seen of its take-off. */
USTRUCT()
struct FAircraftVtoUnderTest
{
	GENERATED_BODY()

	FString ClassPath;
	// Landing gear bone the wing-rotor animations fold away in flight; NAME_None skips the gear checks.
	FName GearBone = NAME_None;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> Aircraft;

	EAircraftVtoTestStep Step = EAircraftVtoTestStep::Settling;
	float StepStartSeconds = 0.f;
	float StartZ = 0.f;
	bool bSawVerticalTakeOff = false;
	bool bSawTakeOffMontage = false;
	int32 MagazineBeforeAttack = 0;
};

UENUM()
enum class EAircraftBaseTakeOffStep : uint8
{
	WaitingForAirbase,
	// Parked in its bay; BeginPlay, the anim instance and the bay settle before the move order.
	Settling,
	TakingOff,
	// Airborne after the first take-off: the hand-over to normal flight must not jolt the height.
	FlyingOut,
	ReturningToBase,
	SecondTakeOff,
	Done
};

/** The aircraft of -AircraftBaseTakeOffValidate and what was seen of its take-offs and landing. */
USTRUCT()
struct FAircraftBaseTakeOffUnderTest
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> Airbase;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> Aircraft;

	EAircraftBaseTakeOffStep Step = EAircraftBaseTakeOffStep::WaitingForAirbase;
	float StepStartSeconds = 0.f;
	FVector BayLocation = FVector::ZeroVector;
	float ParkedZ = 0.f;
	float LastZ = 0.f;
	float LargestZStepPerSecond = 0.f;
	float HeightAboveGroundAtAirborne = 0.f;
	bool bSawVerticalTakeOff = false;
};

UENUM()
enum class EAircraftTerrainFollowTestStep : uint8
{
	// Airborne at the route start; the clearance is captured.
	Settling,
	FlyingRoute,
	AttackDiving,
	Done
};

/** The aircraft of -AircraftTerrainFollowValidate and what was measured over the route. */
USTRUCT()
struct FAircraftTerrainFollowFlightUnderTest
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> Aircraft;

	EAircraftTerrainFollowTestStep Step = EAircraftTerrainFollowTestStep::Settling;
	float StepStartSeconds = 0.f;
	FVector RouteStart = FVector::ZeroVector;
	FVector RouteEnd = FVector::ZeroVector;
	float RouteLowestGroundZ = 0.f;
	float RouteHighestGroundZ = 0.f;
	float Clearance = 0.f;
	float LastZ = 0.f;
	float LowestAircraftZ = BIG_NUMBER;
	float HighestAircraftZ = -BIG_NUMBER;
	float LowestAboveGround = BIG_NUMBER;
	float HighestAboveGround = -BIG_NUMBER;
	float FastestVerticalRate = 0.f;
	float NextSampleLogSeconds = 0.f;
	int32 DiveFrames = 0;
	int32 DiveFramesWithTerrainMotion = 0;
	float LastDiveSeconds = -1.f;
	FVector AttackPoint = FVector::ZeroVector;
};

/** One anti-air unit under test and what its turrets did against the aircraft. */
USTRUCT()
struct FAntiAirUnitUnderTest
{
	GENERATED_BODY()

	FString Label;
	FString ClassPath;

	UPROPERTY()
	TWeakObjectPtr<AActor> Unit;

	UPROPERTY()
	TArray<TWeakObjectPtr<ACPPTurretsMaster>> Turrets;

	// Magazine of every weapon of every turret at the last count; a drop is a shot.
	TArray<int32> LastMagazineCounts;
	// Rounds it must fire at the aircraft to count as engaging it, so one stray round is not enough.
	int32 ShotsNeeded = 3;
	int32 ShotsWhileTargetingAircraft = 0;
	int32 ShotsWhileTargetingGround = 0;
	bool bHasTargetedAircraft = false;
	bool bHasTargetedGround = false;
	float FirstTargetedSeconds = -1.f;
	FString LastTargetName;
};

UENUM()
enum class EAircraftDeathTestStep : uint8
{
	// The airbase it parks in unpacks into its building first.
	WaitingForAirbase,
	Flying,
	Falling,
	WaitingForCleanup,
	Done
};

/** One aircraft that is shot down by the death test and what was seen of its fall. */
USTRUCT()
struct FAircraftDeathUnderTest
{
	GENERATED_BODY()

	FString Label;
	FString ClassPath;
	bool bExpectWingFire = false;
	// Parked on the ground: no fall, it must break up where it stands.
	bool bSpawnLanded = false;
	// Parked in a bay of an unpacked airbase: its pieces must stay inside the building, on the bay floor.
	bool bParkInAirbase = false;
	uint8 OwningPlayer = 2;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> Airbase;

	FVector BayLocation = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> Aircraft;

	EAircraftDeathTestStep Step = EAircraftDeathTestStep::Flying;
	float StepStartSeconds = 0.f;
	float KilledSeconds = -1.f;
	float ImpactSeconds = -1.f;
	FVector LocationAtDeath = FVector::ZeroVector;
	float HorizontalSpeedAtDeath = 0.f;
	FVector VelocityAtDeath = FVector::ZeroVector;
	FVector ImpactLocation = FVector::ZeroVector;
	float GroundZ = 0.f;
	float ActorClearedSeconds = -1.f;
	bool bCheckedParkedSpread = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> Wreck;
	int32 NextCaptureIndex = 0;
};
/**
 * @brief Map tests for aircraft, started from the command line on a map with open ground (UT_Nomadics):
 * -AntiAirValidate spawns the player's anti-air units around the camera and an enemy aircraft circling over them;
 * every unit must target the aircraft and fire at it. -AircraftDeathValidate shoots every aircraft down in flight
 * and checks the burning crash and the breakup. -AircraftVtoValidate parks one aircraft (the Ho 229 unless
 * -AircraftVtoClass=<path under Blueprints/Aircraft/> names another), takes off and fires at the ground.
 * -AircraftStrafeValidate runs the player's Ho 229 strafing global ability on the anchor.
 * -AircraftDropValidate runs the German vehicle drop abilities around the anchor.
 * Logs RTS_AIRCRAFT_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UAircraftTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EAircraftTestPhase M_Phase = EAircraftTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	float M_NextStatusSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	FVector M_Anchor = FVector::ZeroVector;

	UPROPERTY()
	TArray<FAntiAirUnitUnderTest> M_AntiAirUnits;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_TargetAircraft;

	float M_TargetAircraftDamageTaken = 0.f;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_GroundTarget;

	UPROPERTY()
	TArray<FAircraftDeathUnderTest> M_DeathAircraft;

	int32 M_DeathAircraftIndex = 0;

	UPROPERTY()
	FAircraftVtoUnderTest M_VtoAircraft;

	// -AircraftStrafeValidate: magazine of every strafing aircraft when first seen; a drop is a shot.
	TMap<TWeakObjectPtr<AAircraftMaster>, int32> M_StrafeAircraftFirstMagazines;
	int32 M_StrafeMostAircraftSeen = 0;
	bool bM_StrafeAnyFired = false;

	// -AircraftDropValidate: nothing else holds the running abilities, and their load callbacks outlive StartAircraftDrop.
	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> M_DropAbilities;
	int32 M_DropMostAircraftSeen = 0;

	// -AircraftDeathCapture=<folder>: offscreen pictures of every crash.
	FB4HowitzerCaptureRig M_CaptureRig;

	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void EnterPhase(EAircraftTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	bool GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const;

	/**
	 * @brief Spawns a unit Blueprint for the given player the way a trained unit appears, with the owner set
	 * before BeginPlay so its weapons, vision and registration are those of that player.
	 * @param ClassPath Blueprint class path ending in _C.
	 * @param Transform Where the unit appears.
	 * @param OwningPlayer 1 for the player, 2 for the enemy.
	 * @return The spawned unit or nullptr.
	 */
	AActor* SpawnUnitForPlayer(const FString& ClassPath, const FTransform& Transform, uint8 OwningPlayer) const;

	// ---------------- Anti-air ----------------
	void StartAntiAir();
	void SpawnAntiAirUnits();
	void SpawnTargetAircraft();
	void TickAntiAir();
	void CollectTurrets(FAntiAirUnitUnderTest& Unit) const;
	/** @return Rounds the unit's turrets fired since the last count. */
	int32 CountShots(FAntiAirUnitUnderTest& Unit) const;
	void KeepTargetAircraftAlive();
	void StartAntiAirGround();
	void TickAntiAirGround();
	void FinishAntiAirGround();
	void LogAntiAirStatus() const;
	void FinishAntiAir();

	// ---------------- Aircraft death ----------------
	void StartAircraftDeath();
	void TickAircraftDeath();
	bool SpawnDeathAircraft(FAircraftDeathUnderTest& Entry);
	bool SpawnAirbaseForDeathAircraft(FAircraftDeathUnderTest& Entry, const FVector& Ground);
	void TickDeathWaitingForAirbase(FAircraftDeathUnderTest& Entry);
	/** @return Whether the aircraft was spawned and parked in a bay of the entry's unpacked airbase. */
	bool ParkDeathAircraftInAirbase(FAircraftDeathUnderTest& Entry);
	void CheckNoSmokeLingers(const FAircraftDeathUnderTest& Entry);
	void CheckPiecesCameDown(const FAircraftDeathUnderTest& Entry);
	void LogSunkPieces(const FAircraftDeathUnderTest& Entry) const;
	void CheckParkedPieces(FAircraftDeathUnderTest& Entry);
	void CheckPiecesStayInTheBay(const FAircraftDeathUnderTest& Entry);
	void LogStaticHitsUnder(const FVector2D& Location2D) const;
	/** @return World locations of the wreck's pieces that have geometry; empty without a wreck. */
	/** @return World centre of every wreck piece; OutTransformIndices/OutExtents (optional) tell which piece each is. */
	TArray<FVector> GetWreckPieceLocations(const FAircraftDeathUnderTest& Entry, TArray<int32>* OutTransformIndices = nullptr,
	                                       TArray<FVector>* OutExtents = nullptr) const;
	void TickDeathFlying(FAircraftDeathUnderTest& Entry);
	void KillDeathAircraft(FAircraftDeathUnderTest& Entry);
	void TickDeathFalling(FAircraftDeathUnderTest& Entry);
	void TickDeathWaitingForCleanup(FAircraftDeathUnderTest& Entry);
	void CheckRightAfterKill(FAircraftDeathUnderTest& Entry);
	void CheckImpact(FAircraftDeathUnderTest& Entry);
	void CaptureDeathMoment(FAircraftDeathUnderTest& Entry, const FVector& Focus);
	void EnterDeathStep(FAircraftDeathUnderTest& Entry, EAircraftDeathTestStep NewStep);
	void ApplyDeathEffectOverrides(AAircraftMaster& Aircraft) const;
	void LogWreck(const FAircraftDeathUnderTest& Entry) const;
	void LogNiagaraUserParameters(const class UNiagaraSystem* System) const;

	// ---------------- Vertical take-off (AircraftVtoTest.cpp) ----------------
	void StartAircraftVto();
	void TickAircraftVto();
	void TickVtoSettling(FAircraftVtoUnderTest& Entry);
	void TickVtoTakingOff(FAircraftVtoUnderTest& Entry);
	void TickVtoFiring(FAircraftVtoUnderTest& Entry);
	/** Rig, anim instance and weapon of the parked aircraft, before it takes off. */
	void CheckVtoLoadout(const FAircraftVtoUnderTest& Entry);
	void EnterVtoStep(FAircraftVtoUnderTest& Entry, EAircraftVtoTestStep NewStep);

	// ---------------- Take-off from an airbase (AircraftBaseTakeOffTest.cpp) ----------------
	UPROPERTY()
	FAircraftBaseTakeOffUnderTest M_BaseTakeOff;

	void StartAircraftBaseTakeOff();
	void TickAircraftBaseTakeOff();
	void TickBaseTakeOffWaitingForAirbase(FAircraftBaseTakeOffUnderTest& Entry);
	void TickBaseTakeOffSettling(FAircraftBaseTakeOffUnderTest& Entry);
	/**
	 * @brief Waits for the vertical take-off out of the bay and checks it ends exactly at the take-off height.
	 * @param Entry The aircraft under test.
	 * @param Label Prefix of the check names, one per take-off.
	 * @param NextStep Step once airborne.
	 */
	void TickBaseTakeOffTakingOff(FAircraftBaseTakeOffUnderTest& Entry, const FString& Label,
	                              EAircraftBaseTakeOffStep NextStep);
	void TickBaseTakeOffFlyingOut(FAircraftBaseTakeOffUnderTest& Entry);
	void TickBaseTakeOffReturning(FAircraftBaseTakeOffUnderTest& Entry);
	/** @return Whether a move order away from the airbase was accepted; a parked aircraft takes off for it. */
	bool OrderBaseTakeOffMoveOut(FAircraftBaseTakeOffUnderTest& Entry, const FString& Label);
	/** @return Height of the aircraft above the landscape below it; -1 when no landscape is below. */
	float GetHeightAboveLandscape(const AAircraftMaster& Aircraft) const;
	void EnterBaseTakeOffStep(FAircraftBaseTakeOffUnderTest& Entry, EAircraftBaseTakeOffStep NewStep);

	// ---------------- Terrain following over relief (AircraftTerrainFollowFlightTest.cpp) ----------------
	UPROPERTY()
	FAircraftTerrainFollowFlightUnderTest M_TerrainFlight;

	void StartAircraftTerrainFollow();
	void TickAircraftTerrainFollow();
	/**
	 * @brief Picks the straight route from the anchor with the largest ground height range, so the flight crosses
	 * the strongest relief the map has nearby.
	 * @param Entry Receives route start, end and its lowest / highest ground.
	 * @return Whether a route over landscape was found.
	 */
	bool FindBumpiestRoute(FAircraftTerrainFollowFlightUnderTest& Entry) const;
	/** @return Landscape height at the XY; false where no landscape is. */
	bool GetLandscapeZ(const FVector2D& Location2D, float& OutGroundZ) const;
	void TickTerrainFollowSettling(FAircraftTerrainFollowFlightUnderTest& Entry);
	void TickTerrainFollowFlyingRoute(FAircraftTerrainFollowFlightUnderTest& Entry);
	void TickTerrainFollowAttackDiving(FAircraftTerrainFollowFlightUnderTest& Entry);
	/** @brief Updates the extremes and vertical rate, and logs a sample row now and then. */
	void RecordTerrainFollowSample(FAircraftTerrainFollowFlightUnderTest& Entry, const TCHAR* StepName);
	void EnterTerrainFollowStep(FAircraftTerrainFollowFlightUnderTest& Entry, EAircraftTerrainFollowTestStep NewStep);

	// ---------------- Strafing global ability (AircraftStrafeTest.cpp) ----------------
	void StartAircraftStrafe();
	void TickAircraftStrafe();
	/** @return Whether any strafing aircraft has fired since it was first seen. */
	bool TrackStrafeAircraft();

	// ---------------- Vehicle drop global abilities (AircraftDropTest.cpp) ----------------
	void StartAircraftDrop();
	void TickAircraftDrop();
};
