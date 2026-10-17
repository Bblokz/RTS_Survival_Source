// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Player/Abilities.h"
#include "TeamWeaponDigInTestSubsystem.generated.h"

class ACPPController;
class ASceneCapture2D;
class ATeamWeapon;
class ATeamWeaponController;
class ATeamWeaponDigInWall;
class UHealthComponent;
class UTeamWeaponDigInComponent;
class UTextureRenderTarget2D;
class URTSOptimizer;

enum class ETWDigInTestStep : uint8
{
	WaitingForSpawn,
	WaitingForDeploy,
	Settling,
	WaitingForDigIn,
	SettlingDugIn,
	CheckingBrokenCover,
	WaitingForBuilding,
	CheckingCancel,
	WaitingForNavMeshRestore,
	DrivingOutOfPit,
	Done
};

/** One team weapon squad spawned by the dig-in test, from its Blueprint class. */
struct FTWDigInTestSubject
{
	FString Name;
	FString ClassPath;
	FVector SpawnLocation = FVector::ZeroVector;
	ETWDigInTestStep Step = ETWDigInTestStep::WaitingForSpawn;
	float StepStartGameSeconds = 0.f;
	TWeakObjectPtr<ATeamWeaponController> Controller;
	// Gun health and damage multiplier before the dig-in; the protection must come off back to these.
	float BaseMaxHealth = 0.f;
	float BaseDamageMultiplier = 1.f;
	// Health a test hit took off the gun before it dug in.
	float BaselineHitDrop = 0.f;
	// Command card before the dig-in; break cover must restore it slot for slot.
	TArray<EAbilityID> CardBeforeDigIn;
	bool bHasCheckedBuildingCard = false;
	// The pit stands at the edge of the navmesh: there is nothing to walk into it from, so the path and guard checks
	// would test the map, not the pit.
	bool bHasNavMeshAroundGun = true;
	// Where the front wall stood: once the pit is gone the navmesh must come back there and the gun drive through.
	FVector FormerFrontWallCentre = FVector::ZeroVector;
	float FormerFrontWallHalfHeightCm = 0.f;
	FVector LocationBeforeDrivingOut = FVector::ZeroVector;
	bool bHasComparisonPicture = false;
};

/**
 * @brief Map test for team weapon sandbag dig-ins, started with -TeamWeaponDigInValidate.
 * With -TWDigInSurvey it only measures every team weapon Blueprint (gun extents, barrel height, crew posts) and
 * writes the JSON the Blender wall generator sizes the pits from; otherwise it digs every gun in and checks the pit.
 */
UCLASS()
class RTS_SURVIVAL_API UTeamWeaponDigInTestSubsystem : public UTickableWorldSubsystem
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
	bool bM_IsFinished = false;
	bool bM_HasStartedGame = false;
	bool bM_HasSpawnedSubjects = false;
	bool bM_IsSurvey = false;
	bool bM_IsSuppressingErrorDialogs = false;
	bool bM_TakePictures = false;
	double M_StartRealSeconds = 0.0;
	mutable double M_NextGameUnlockRealSeconds = 0.0;
	float M_SubjectsSpawnedGameSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	int32 M_PictureIndex = 0;
	FVector M_TestCentre = FVector::ZeroVector;
	FString M_OutputDirectory;
	TArray<FTWDigInTestSubject> M_Subjects;
	// One JSON object per surveyed Blueprint, joined into the survey file at the end.
	TArray<FString> M_SurveyEntries;
	mutable TSet<TWeakObjectPtr<URTSOptimizer>> M_DisabledOptimizers;

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
	void PlanAndSpawnSubjects();
	bool SpawnSubject(FTWDigInTestSubject& Subject) const;
	bool TryProjectToNavMesh(FVector& InOutLocation) const;
	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	void KeepUnitsUnthrottled(AActor* Unit) const;
	void KeepSquadUnthrottled(ATeamWeaponController& Controller) const;

	// ---- Steps ----
	void TickSubject(FTWDigInTestSubject& Subject);
	void EnterStep(FTWDigInTestSubject& Subject, const ETWDigInTestStep NewStep);
	void TickSettled(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon);
	void TickDigInSteps(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon,
	                    const float StepSeconds);

	// ---- Dig-in checks ----
	void StartDigInRound(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon);
	void TickWaitingForDigIn(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
	                         const UTeamWeaponDigInComponent& DigIn, const float StepSeconds);
	void TickSettlingDugIn(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller, ATeamWeapon& Weapon,
	                       UTeamWeaponDigInComponent& DigIn, const float StepSeconds);
	void TickWaitingForBuilding(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
	                            const UTeamWeaponDigInComponent& DigIn, const float StepSeconds);
	void CheckDugInCard(const FTWDigInTestSubject& Subject, ATeamWeaponController& Controller, const TCHAR* Label);
	void CheckProtection(const FTWDigInTestSubject& Subject, const UHealthComponent& Health);
	void CheckHitTransfer(const FTWDigInTestSubject& Subject, ATeamWeapon& Weapon, ATeamWeaponDigInWall& Wall,
	                      UHealthComponent& Health);
	void CheckWallTraces(const FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon,
	                     const ATeamWeaponDigInWall& Wall);
	void CheckNavigation(const FTWDigInTestSubject& Subject, const ATeamWeaponDigInWall& Wall);
	void CheckCrewInsidePit(const FTWDigInTestSubject& Subject, const ATeamWeaponController& Controller,
	                        const UTeamWeaponDigInComponent& DigIn);
	void CheckWallOnGround(const FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon,
	                       const ATeamWeaponDigInWall& Wall);
	/**
	 * @brief After break cover or a cancelled dig-in: no pit, health and damage back to the base, card restored.
	 * @param Subject The gun under test.
	 * @param Controller Its squad.
	 * @param Weapon The gun.
	 * @param Label Which removal is checked (break_cover, cancel).
	 */
	void CheckDigInRemoved(const FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
	                       const ATeamWeapon& Weapon, const TCHAR* Label);
	static float TakeTestHit(AActor& HitActor, UHealthComponent& Health);
	void TickWaitingForNavMeshRestore(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
	                                  const ATeamWeapon& Weapon, const float StepSeconds);
	void TickDrivingOutOfPit(FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon, const float StepSeconds);
	float GetWallGapAt(const ATeamWeapon& Weapon, const ATeamWeaponDigInWall& Wall, const double LocalX,
	                   const double LocalY) const;
	// -TWDigInCompareMesh=<static mesh>: the same picture of an existing pit mesh beside the first gun, for its look.
	void CaptureComparisonMesh(FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon);
	bool GetIsPointOnNavMesh(const FVector& Location, const float RadiusCm, const float HeightCm) const;
	// The front wall's footprint has left the navmesh: the pit's modifiers are built in.
	bool GetIsPitCarvedIntoNavMesh(const ATeamWeaponDigInWall& Wall) const;
	// Navmesh in front of the pit and behind its entrance; a pit at the map's navmesh edge has none.
	bool GetHasNavMeshAroundPit(const ATeamWeaponDigInWall& Wall) const;

	// ---- Survey ----
	void SurveySubject(const FTWDigInTestSubject& Subject, const ATeamWeaponController& Controller,
	                   ATeamWeapon& Weapon);
	// Squad and gun subtypes, owners, the gun's weapons and the crew's infantry weapons, as JSON fields.
	FString GetRosterJson(const ATeamWeaponController& Controller, ATeamWeapon& Weapon) const;
	void WriteSurveyFile() const;

	// ---- Pictures ----
	bool CreateCaptureRig();
	/**
	 * @brief Renders the focus actor from behind its right shoulder, far enough out to show the whole pit.
	 * @param Focus Actor in the middle of the picture; its yaw sets the view direction.
	 * @param Label File name part.
	 * @param DistanceScale Multiplies the camera offset; 1 frames a gun, about 1.6 a large pit.
	 */
	void CapturePicture(const AActor& Focus, const FString& Label, const float DistanceScale = 1.f);
};
