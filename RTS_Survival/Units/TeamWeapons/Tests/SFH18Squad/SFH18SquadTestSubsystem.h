// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFH18SquadTestSubsystem.generated.h"

class ACameraActor;
class ASquadController;
class ATeamWeapon;
class ATeamWeaponController;
class UGlobalAbility;

UENUM()
enum class ESFH18SquadTestPhase : uint8
{
	WaitingForStart,
	WaitingForSpawnedSquad,
	SettlingSpawnedSquad,
	Firing,
	AfterFire,
	StartingDrop,
	Dropping,
	SettlingDrop,
	CheckingDrop,
	FinalShot,
	Finished
};

/**
 * @brief -SFH18SquadValidate: proves the 15 cm sFH 18 team weapon squad (Squad_Ger_SFH18_150mm) and its airdrop.
 * 1. Spawns one squad through the async spawner at the anchor (where the camera looks, or -SFH18AnchorX= /
 *    -SFH18AnchorY=): the gun must be the sFH 18 (SK_Ger_SFH18Anim, weapon sFH18_150MM_TW), deploy, and have its
 *    operators at their posts playing the crew animation; then it fires at a ground target at about half its range.
 * 2. Executes GA_GerSFH18Drop at the anchor: two sFH 18 guns must land upright on cargo chutes, deploy, be crewed and
 *    belong to player 1; the template must cost radixite only.
 * Screenshots into Saved/SFH18SquadTest; logs RTS_SFH18_TEST CHECK PASS/FAIL and a RESULT line, then quits.
 * On /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall pass -SFH18AnchorX=-12145 -SFH18AnchorY=-24477.
 */
UCLASS()
class RTS_SURVIVAL_API USFH18SquadTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	ESFH18SquadTestPhase M_Phase = ESFH18SquadTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FString M_StepLabel = TEXT("setup");
	FVector M_Anchor = FVector::ZeroVector;
	float M_SpawnYaw = 0.f;
	FVector M_FireTarget = FVector::ZeroVector;
	bool bM_SawFire = false;
	bool bM_TookImpactShot = false;
	bool bM_AbilityEnded = false;
	bool bM_TookDescentShot = false;
	bool bM_TookLandedShot = false;
	float M_LandedShotSeconds = 0.f;
	FVector M_CameraFocus = FVector::ZeroVector;
	float M_CameraDistance = 2000.f;
	float M_CameraPitch = -25.f;
	float M_CameraYaw = 35.f;

	TWeakObjectPtr<ASquadController> M_SpawnedSquad;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	// Actors that existed before the drop; only new ones are checked.
	TSet<TWeakObjectPtr<AActor>> M_ActorsBeforeDrop;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartSpawn();
	void OnSquadSpawned(AActor* SpawnedActor);
	void TickWaitingForSpawnedSquad();
	void TickSettlingSpawnedSquad();
	void StartFiring(ATeamWeaponController& Squad, ATeamWeapon& Gun);
	void TickFiring();
	void TickAfterFire();
	void CleanUpSpawnedSquad();

	void StartDrop();
	void TickDropping();
	void TickSettlingDrop();
	void CheckDrop();

	/** Checks that the squad is the sFH 18 squad with the sFH 18 gun, owned by the test player. */
	void CheckSquadIdentity(ATeamWeaponController& Squad, const FString& Label);
	/** Checks that the gun is deployed, upright on the ground and that every operator is at its post, crewing. */
	void CheckSquadCrewed(ATeamWeaponController& Squad, const FString& Label);
	TArray<AActor*> GetNewActorsOfClass(UClass* ActorClass) const;
	bool TryFindMidDescentGun(FVector& OutLoadLocation) const;
	void SetCamera(const FVector& Focus, float Distance, float Pitch, float Yaw);
	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
	void OnAbilityEnded(UGlobalAbility* Ability);
};
