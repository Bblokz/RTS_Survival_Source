// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Ju87GTankBusterTestSubsystem.generated.h"

class AAircraftMaster;
class ACameraActor;
class UGlobalAbility;
class UWeaponState;

UENUM()
enum class EJu87GTestPhase : uint8
{
	WaitingForStart,
	CloseUp,
	Strike,
	Finished
};

/**
 * @brief -Ju87GTankBustersValidate: spawns one Ju 87 G for close-up screenshots and pod/cannon/bomb-bay checks, then
 * clicks GA_GerJu87GTankBusters on the anchor (camera centre, or -Ju87GAnchorX= / -Ju87GAnchorY=) over enemy T-34s
 * and a KV-1 and checks 2 aircraft, APCR shots from the pod muzzles, no bombs, damaged/killed tanks and one clean end.
 * Screenshots to Saved/Ju87GTest; logs RTS_JU87G_TEST. Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UJu87GTankBusterTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FStrikeAircraftTrack
	{
		TWeakObjectPtr<AAircraftMaster> Aircraft;
		FString ClassName;
		TArray<TWeakObjectPtr<UWeaponState>> Weapons;
		TArray<int32> LastMagazines;
		int32 CannonShots = 0;
		int32 ApcrShots = 0;
		int32 OtherShots = 0;
		float NextStateLogSeconds = 0.f;
	};

	struct FTargetTrack
	{
		TWeakObjectPtr<AActor> Tank;
		FString Label;
		float StartHealth = 0.f;
		float LastHealth = 0.f;
		bool bM_Died = false;
	};

	EJu87GTestPhase M_Phase = EJu87GTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	int32 M_CloseUpShotIndex = 0;
	float M_NextCloseUpShotSeconds = 0.f;
	bool bM_CloseUpChecked = false;

	TWeakObjectPtr<AAircraftMaster> M_CloseUpAircraft;
	TSet<TWeakObjectPtr<AActor>> M_PreExistingActors;
	TArray<FStrikeAircraftTrack> M_StrikeAircraft;
	TArray<FTargetTrack> M_Targets;
	int32 M_BombActorsSeen = 0;
	int32 M_AbilityEndedCount = 0;
	float M_AbilityEndedSeconds = -1.f;
	float M_FirstCannonShotSeconds = -1.f;
	float M_LastPeriodicShotSeconds = -1.f;
	int32 M_DiveShotsTaken = 0;
	float M_NextDiveShotSeconds = 0.f;
	TWeakObjectPtr<AAircraftMaster> M_CameraFollowAircraft;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetPhaseSeconds() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;

	void StartCloseUp();
	void TickCloseUp();
	void CheckCloseUpAircraft(AAircraftMaster& Aircraft);
	void PlaceCloseUpCamera(const AAircraftMaster& Aircraft, int32 ShotIndex) const;

	void StartStrike();
	void ClearCopyRequirements(UGlobalAbility& Ability) const;
	void SpawnTargets();
	bool EnsureCanAfford(const UGlobalAbility& Ability);
	void OnAbilityEnded(UGlobalAbility* Ability);
	void TickStrike();
	void TrackStrikeAircraft();
	void TrackWeapons(AAircraftMaster& Aircraft, FStrikeAircraftTrack& Track);
	void OnCannonShot(AAircraftMaster& Aircraft, FStrikeAircraftTrack& Track, const UWeaponState& Weapon);
	int32 CountProjectilesNear(const FVector& Location, float Radius) const;
	void TrackTargets();
	void TrackBombActors();
	void UpdateStrikeCamera();
	void FinishStrike();

	void RequestScreenshot(const FString& Label);
};
