// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponState/TeamWeaponState.h"
#include "TeamWeaponCrewAnimAudit.h"
#include "TeamWeaponQualityTestSubsystem.generated.h"

class ACPPController;
class ASceneCapture2D;
class ASquadUnit;
class ATankMaster;
class ATeamWeapon;
class ATeamWeaponController;
class UTextureRenderTarget2D;
class URTSOptimizer;

/** Samples of one measured quantity; keeps every value so percentiles can be reported. */
struct FTWQualityMetric
{
	void Add(const float Value) { Values.Add(Value); }
	bool GetHasValues() const { return Values.Num() > 0; }
	float GetMax() const;
	float GetMin() const;
	float GetMean() const;
	float GetPercentile(const float Fraction) const;
	FString Describe() const;

	TArray<float> Values;
};

/** Ground contact of a team weapon: origin height over the ground and the gaps under its footprint corners. */
struct FTWQualityGroundMetrics
{
	// Weapon origin height above the ground below it; positive is floating.
	FTWQualityMetric OriginGapCm;
	// Smallest corner gap: above zero means no part of the footprint touches the ground.
	FTWQualityMetric ContactGapCm;
	// Deepest corner below the ground.
	FTWQualityMetric PenetrationCm;
	FTWQualityMetric TiltDegrees;
};

/** What one squad move (a leg) looked like while the weapon was being pushed. */
struct FTWQualityMoveMetrics
{
	FTWQualityGroundMetrics Ground;
	// Distance of each operator to the crew position it belongs to.
	FTWQualityMetric OperatorSlotDistanceCm;
	FTWQualityMetric GuardDistanceToWeaponCm;
	FTWQualityMetric WeaponSpeedCmPerSec;
	int32 PicturesTaken = 0;
	int32 OperatorSamples = 0;
	// Operator samples that walked in front of the moving weapon: the crew leading its gun.
	int32 OperatorAheadSamples = 0;
	int32 GuardSamples = 0;
	int32 GuardAheadSamples = 0;
	float MovingSeconds = 0.f;
	// Seconds the weapon stood still while in the Moving state; logged with the mover state when it grows.
	float StalledSeconds = 0.f;
	float NextStallLogSeconds = 3.f;
};

/** Crew and weapon placement of a deployed team weapon. */
struct FTWQualityDeployedMetrics
{
	FTWQualityGroundMetrics Ground;
	FTWQualityMetric OperatorPostDistanceCm;
	// Capsule bottom over the ground below the soldier: a soldier standing on the gun shows up here.
	FTWQualityMetric OperatorFeetGapCm;
	FTWQualityMetric GuardDistanceToWeaponCm;
};

/** How the towed weapon moved relative to its truck. */
struct FTWQualityTowMetrics
{
	FTWQualityMetric RelativeYawDegrees;
	FTWQualityMetric RelativePitchDegrees;
	FTWQualityMetric RelativeRollDegrees;
	FTWQualityMetric YawRateDegreesPerSec;
	FTWQualityMetric PitchRateDegreesPerSec;
	FTWQualityMetric HitchDriftCm;
	FTWQualityMetric WheelGroundGapCm;
	FTWQualityMetric TruckSpeedCmPerSec;
	FTWQualityMetric TruckYawRateDegreesPerSec;
};

enum class ETWQualitySquadStep : uint8
{
	WaitingForSpawn,
	WaitingForDeploy,
	SettlingDeployed,
	MeasuringDeployed,
	// Only with -TWQualityCrewAnims: attack ground until weapon 0 reloaded a few times, for the loader montages.
	Firing,
	Moving,
	Done
};

/** A team weapon squad that is spawned, measured deployed and then pushed out along a lane and back. */
struct FTWQualitySquadSubject
{
	FString Name;
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	// Move destinations in order; the last one brings the weapon back.
	TArray<FVector> LegDestinations;
	int32 LegIndex = 0;
	ETWQualitySquadStep Step = ETWQualitySquadStep::WaitingForSpawn;
	float StepStartSeconds = 0.f;
	bool bLeftDeployedSinceOrder = false;
	// Frames a deployed gun was not deployed while it had no order; it should never pack by itself.
	int32 UnorderedStateChanges = 0;
	TWeakObjectPtr<ATeamWeaponController> Controller;
	FTWQualityDeployedMetrics Deployed;
	FTWQualityMoveMetrics Move;
	FVector LastWeaponLocation = FVector::ZeroVector;
	bool bHasLastWeaponLocation = false;
	bool bLoggedConfig = false;
	bool bHasFired = false;
	bool bIsRegisteredForCrewAnimAudit = false;
	FVector FireTarget = FVector::ZeroVector;
	float NextForcedReloadSeconds = 0.f;
	int32 ForcedReloads = 0;
};

enum class ETWQualityTowStep : uint8
{
	WaitingForSpawn,
	SettlingTowed,
	Driving,
	SettlingStopped,
	WaitingForDetachDeploy,
	SettlingDetached,
	MeasuringDetached,
	MovingDetachedGun,
	SettlingAfterGunMove,
	MeasuringAfterGunMove,
	WaitingForRetow,
	SettlingRetowed,
	RetowDriving,
	Done
};

/** A truck spawned with its team weapon in tow; driven over a curve, detached, the gun moved and towed again. */
struct FTWQualityTowSubject
{
	FString Name;
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;
	FVector SpawnLocation = FVector::ZeroVector;
	TArray<FVector> DriveWaypoints;
	FVector GunMoveDestination = FVector::ZeroVector;
	FVector RetowDriveDestination = FVector::ZeroVector;
	ETWQualityTowStep Step = ETWQualityTowStep::WaitingForSpawn;
	float StepStartSeconds = 0.f;
	TWeakObjectPtr<ATankMaster> Truck;
	TWeakObjectPtr<ATeamWeaponController> Controller;
	TWeakObjectPtr<ATeamWeapon> Weapon;
	// Weapon transform relative to the tow socket on the first towed frame; deviations from it are the sway.
	FTransform RestRelativeTransform = FTransform::Identity;
	bool bHasRestRelativeTransform = false;
	FRotator LastRelativeRotation = FRotator::ZeroRotator;
	float LastTruckYaw = 0.f;
	bool bHasLastRelativeRotation = false;
	bool bLeftDeployedSinceOrder = false;
	int32 UnorderedStateChanges = 0;
	FTWQualityTowMetrics Drive;
	FTWQualityTowMetrics Stopped;
	FTWQualityTowMetrics Retow;
	FTWQualityDeployedMetrics DetachedDeployed;
	FTWQualityMoveMetrics GunMove;
	FTWQualityDeployedMetrics AfterGunMove;
	FVector LastWeaponLocation = FVector::ZeroVector;
	bool bHasLastWeaponLocation = false;
	int32 DrivePicturesTaken = 0;
	float NextOrderDebugLogSeconds = 3.f;
	// Where the gun stood when the re-tow was ordered; the retow_debug lines show how far it was moved since.
	FVector RetowOrderWeaponLocation = FVector::ZeroVector;
	float NextRetowDebugLogSeconds = 0.f;
	// Last logged controller state and tow links, so every change shows up once in the log.
	FString LastLoggedTowStatus;
};

/** What a tow subject's step functions need each frame, resolved and validated once by the dispatcher. */
struct FTWQualityTowFrame
{
	ATankMaster* Truck = nullptr;
	ATeamWeaponController* Controller = nullptr;
	ATeamWeapon* Weapon = nullptr;
	ETeamWeaponState State = ETeamWeaponState::Uninitialized;
	float StepSeconds = 0.f;
};

/** A picture the capture rig takes once it has rendered a few frames aimed at the subject. */
struct FTWQualityPictureRequest
{
	TWeakObjectPtr<AActor> Focus;
	FString Label;
	int32 FramesAimed = 0;
};

/**
 * @brief Quality control of team weapons in one unattended run: ground contact while deployed, pushed and towed,
 * crew keeping their posts while the gun is pushed out and back, and the towed gun swaying behind its truck.
 * Started with -TeamWeaponQualityValidate; logs RTS_TWQC lines, pictures with -TWQualityDir=<folder>.
 * Not compiled into shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UTeamWeaponQualityTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// The game boots paused behind the start widget; the test has to tick to start it.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	bool bM_IsActive = false;
	bool bM_HasStartedGame = false;
	bool bM_HasSpawnedSubjects = false;
	bool bM_IsFinished = false;
	bool bM_IsSuppressingErrorDialogs = false;
	double M_StartRealSeconds = 0.0;
	mutable double M_NextGameUnlockRealSeconds = 0.0;
	float M_SubjectsSpawnedGameSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	FString M_OutputDirectory;
	bool bM_TakePictures = false;
	FVector M_TestCentre = FVector::ZeroVector;
	float M_LegLengthCm = 0.f;
	// -TWQualityFireYaw=: turns the attack ground target away from the spawn facing so the gun fires off-axis.
	float M_FireYawOffsetDegrees = 0.f;
	// -TWQualityCrewAnims: crew montage audit plus a firing step so reload montages play.
	bool bM_AuditCrewAnims = false;
	FTeamWeaponCrewAnimAudit M_CrewAnimAudit;

	TArray<FTWQualitySquadSubject> M_Squads;
	TArray<FTWQualityTowSubject> M_Tows;
	TArray<FTWQualityPictureRequest> M_Pictures;
	// Optimizers of test units switched off once; their timer would otherwise throttle the units again.
	mutable TSet<TWeakObjectPtr<URTSOptimizer>> M_DisabledOptimizers;
	int32 M_PictureIndex = 0;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget = nullptr;

	float GetGameSeconds() const;
	void Check(const bool bCondition, const FString& Subject, const TCHAR* Description, const FString& Details);
	void Finish(const TCHAR* Reason);

	// ---- Start up ----
	bool TryStartGame();
	void KeepGameRunning() const;
	bool TryFindTestCentre();
	bool TryUseRandomNavigableCentre(const double RealSeconds);
	/**
	 * @brief Moves the test centre to the most hilly walkable area nearby; slopes are where guns float or sink.
	 * @param SearchRadiusCm How far from the start spot candidate centres are tried.
	 */
	void MoveCentreToHillyGround(const float SearchRadiusCm);
	float GetReliefAround(const FVector& Centre, float& OutMaxSlopeDegrees) const;
	void ReadSubjectsFromCommandLine(TArray<ESquadSubtype>& OutSquads,
	                                 TArray<TPair<ETankSubtype, ESquadSubtype>>& OutTows) const;
	void PlanAndSpawnSubjects();

	/**
	 * @brief Picks the lane from an origin with the most height change that is still walkable, so the ground
	 * conformance is measured on slopes rather than on a flat parade ground.
	 * @param Origin Lane start on the navmesh.
	 * @param AvoidLocations Other subject origins the lane must not run towards.
	 * @param OutLaneEnd Lane end projected onto the navmesh.
	 * @return False when no direction has a navigable lane of the requested length.
	 */
	bool TryFindSlopedLane(const FVector& Origin, const TArray<FVector>& AvoidLocations, FVector& OutLaneEnd) const;
	bool TryProjectToNavMesh(FVector& InOutLocation) const;
	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	bool SpawnSquadSubject(FTWQualitySquadSubject& Subject, const int32 SpawnId);

	// ---- Squads ----
	void TickSquadSubject(FTWQualitySquadSubject& Subject);
	static FString GetSquadDeployedLabel(const FTWQualitySquadSubject& Subject);
	// Not deployed while no command is active: the gun packed or turned by itself.
	static bool GetIsNotDeployedWithoutOrder(ATeamWeaponController& Controller);
	void TickSquadWaitingForSpawn(FTWQualitySquadSubject& Subject, const ATeamWeapon* Weapon, const float StepSeconds);
	void TickSquadWaitingForDeploy(FTWQualitySquadSubject& Subject, const ATeamWeaponController& Controller,
	                               const float StepSeconds);
	void TickSquadSettlingDeployed(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller,
	                               ATeamWeapon& Weapon, const float StepSeconds);
	void TickSquadMeasuringDeployed(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller,
	                                ATeamWeapon& Weapon, const float StepSeconds);
	void TickSquadMoving(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon,
	                     const float StepSeconds);
	void StartSquadFiring(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon);
	void TickSquadFiring(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon,
	                     const float StepSeconds);
	void EnterSquadStep(FTWQualitySquadSubject& Subject, const ETWQualitySquadStep NewStep);
	void OrderNextLeg(FTWQualitySquadSubject& Subject);
	void SampleSquadMove(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon,
	                     FTWQualityMoveMetrics& Metrics, FVector& InOutLastWeaponLocation, bool& bInOutHasLast);
	void ReportSquadLeg(const FString& SubjectName, const FString& LegLabel, const FTWQualityMoveMetrics& Metrics,
	                    const float DistanceToDestinationCm);
	void LogTeamWeaponConfig(const FString& SubjectName, const ATeamWeapon& Weapon) const;

	// ---- Shared measurements ----
	void SampleGround(const ATeamWeapon& Weapon, FTWQualityGroundMetrics& Metrics) const;
	void SampleDeployed(ATeamWeaponController& Controller, ATeamWeapon& Weapon, FTWQualityDeployedMetrics& Metrics) const;
	void ReportDeployed(const FString& SubjectName, const FString& Label, const FTWQualityDeployedMetrics& Metrics);
	void ReportGround(const FString& SubjectName, const FString& Label, const FTWQualityGroundMetrics& Metrics,
	                  const bool bIsMoving);
	void KeepUnitsUnthrottled(AActor* Unit) const;
	/**
	 * @brief Keeps a squad and its gun ticking at full rate and alive.
	 * @param Controller Squad to keep running.
	 * @param bKeepTurretEnabled False switches the turret off so subjects never engage each other.
	 */
	void KeepSquadUnthrottled(ATeamWeaponController& Controller, const bool bKeepTurretEnabled) const;

	// ---- Towing ----
	void TickTowSubject(FTWQualityTowSubject& Subject);
	void TickTowWaitingForSpawn(FTWQualityTowSubject& Subject, const float StepSeconds);
	void TickTowSettlingTowed(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void TickTowDriving(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void TickTowSettlingStopped(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void TickTowWaitingForDetachDeploy(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);

	/**
	 * @brief Waits for a detached or pushed gun to settle while counting frames it is not deployed without an order.
	 * @param Subject Tow subject.
	 * @param Frame Resolved actors and step time.
	 * @param OutMetrics Metrics reset for the measuring step that follows.
	 * @param MeasuringStep Step entered once settled.
	 */
	void TickTowSettlingDeployed(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame,
	                             FTWQualityDeployedMetrics& OutMetrics, const ETWQualityTowStep MeasuringStep);
	void ReportTowDeployedWindow(FTWQualityTowSubject& Subject, const TCHAR* Label,
	                             const FTWQualityDeployedMetrics& Metrics);
	void TickTowMeasuringDetached(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void TickTowMovingDetachedGun(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void TickTowMeasuringAfterGunMove(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void TickTowWaitingForRetow(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void LogRetowApproach(const FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame) const;
	void TickTowSettlingRetowed(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void TickTowRetowDriving(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame);
	void EnterTowStep(FTWQualityTowSubject& Subject, const ETWQualityTowStep NewStep);
	bool TryFindTowedPair(FTWQualityTowSubject& Subject);
	void SampleTow(FTWQualityTowSubject& Subject, FTWQualityTowMetrics& Metrics);
	void LogTowStatusChanges(FTWQualityTowSubject& Subject) const;
	void LogLargeTowWheelGap(const FTWQualityTowSubject& Subject, const ATeamWeapon& Weapon, const ATankMaster& Truck,
	                         const float WheelGapCm) const;
	void ReportTow(const FString& SubjectName, const FString& Label, const FTWQualityTowMetrics& Metrics,
	               const bool bExpectSway);
	void OrderTruckRoute(FTWQualityTowSubject& Subject, const TArray<FVector>& Waypoints) const;
	bool GetIsTruckIdle(const FTWQualityTowSubject& Subject) const;

	// ---- Pictures ----
	bool CreateCaptureRig();
	void RequestPicture(AActor* Focus, const FString& Label);
	void TickPictures();
};
