// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Round5FixesTestSubsystem.generated.h"

class AAircraftMaster;
class ACameraActor;
class ASquadController;
class ATeamWeapon;
class ATeamWeaponController;
class UGlobalAbility;
class UGlobalAbilitiesManager;
class UWeaponState;
enum class EGlobalAbility : uint8;
enum class EGlobalAbilityTier : uint8;

UENUM()
enum class ERound5FixesTestPhase : uint8
{
	WaitingForStart,
	TemplateChecks,
	WaitingForGuns,
	GunsSettling,
	FiringBeyondLeFH,
	LeFHControlShot,
	RunningCargoDoorDrop,
	ReleasedMoveCheck,
	MissionCaps,
	Finished
};

/**
 * @brief -Round5FixesValidate: headless proof of the round-5 fixes on a small map (RTS_ToCopySmall).
 * 1) Templates: no DA template but GA_GerJu87GTankBusters asks for a technology; GA_2xBf110Strafing reads Bf 109;
 *    a GA_3xJu87Strafing copy is clicked through the player's path with no technology researched and pays.
 * 2) Range: a spawned sFH 18 and leFH 18 squad both get an attack-ground order beyond the leFH 18's range; the sFH 18
 *    fires, the leFH 18 does not (then fires at a point inside its range as a control).
 * 3) Cargo door: GA_GerRedSpinePairDrop lands; the walkers appear at the Ju 390's CargoDoor socket, walk out to their
 *    spots on the circle (positions logged every 0.5 s, screenshots into Saved/Round5FixesTest) and obey a new order.
 * With -Round5MissionCaps=<TierI..TierIV> on a mission map it only checks that map's mission manager cap instead.
 * Anchor: -Round5AnchorX= -Round5AnchorY=. Logs RTS_ROUND5_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API URound5FixesTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	ERound5FixesTestPhase M_Phase = ERound5FixesTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FBox M_LandscapeBounds = FBox(ForceInit);
	FVector M_TowardCentre = FVector(0.f, 1.f, 0.f);

	// Range step: the two guns and where they were told to fire.
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_Sfh18Squad;
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_LeFH18Squad;
	FVector M_Sfh18Target = FVector::ZeroVector;
	FVector M_LeFH18Target = FVector::ZeroVector;
	bool bM_Sfh18Fired = false;
	bool bM_LeFH18Fired = false;

	// Cargo door step.
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;
	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> M_StartedAbilities;
	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_KnownSquads;
	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_DroppedSquads;
	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_DropAircraft;
	FVector M_DropAnchor = FVector::ZeroVector;
	// Ground point under the CargoDoor socket, read once the aircraft has landed.
	FVector M_DoorGroundLocation = FVector::ZeroVector;
	bool bM_HasDoorLocation = false;
	// Per dropped squad (same order as M_DroppedSquads): where its controller was spawned, where its walkers first
	// stood once loaded (zero until then) and when it appeared.
	TArray<FVector> M_SpawnPoints;
	TArray<FVector> M_FirstSeenCentres;
	TArray<float> M_FirstSeenSeconds;
	TArray<float> M_MostDistanceFromDoor;
	TArray<FVector> M_ReleaseMoveStarts;
	TArray<FVector> M_ReleaseMoveTargets;
	float M_NextSampleSeconds = 0.f;
	float M_NextScreenshotSeconds = 0.f;
	float M_LastAircraftSeenSeconds = -1.f;
	bool bM_DropEnded = false;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetPhaseSeconds() const;
	void EnterPhase(ERound5FixesTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	UGlobalAbilitiesManager* GetPlayerAbilityManager() const;
	UGlobalAbility* MakeAbilityCopy(EGlobalAbility AbilityType);
	bool EnsureCanAfford(const UGlobalAbility& Ability);

	void RunTemplateChecks();
	void CheckTemplateRequirements();
	void CheckBf109Wording();
	void CheckCargoDoorFlags();
	void ClickJu87StrikeWithoutTechs();

	void StartGuns();
	void SpawnGunSquad(uint8 SquadSubtype, const FVector& Location, bool bIsSfh18);
	void TickWaitingForGuns();
	void TickGunsSettling();
	void TickFiringBeyondLeFH();
	void TickLeFHControlShot();
	static ATeamWeapon* GetGun(ASquadController* Squad);
	static UWeaponState* GetMainWeapon(ATeamWeapon* Gun);
	static bool GetHasFired(ASquadController* Squad);
	FVector OrderAttackGround(ASquadController* Squad, float Distance);

	void StartCargoDoorDrop();
	void TickRunningCargoDoorDrop();
	void CollectNewSquads();
	void SampleDroppedSquads();
	void FinishCargoDoorChecks();
	void TickReleasedMoveCheck();
	bool UpdateDoorLocation();

	void TickMissionCaps();

	FBox FindLandscapeBounds() const;
	FVector KeepInsideLandscape(const FVector& Location) const;
	static FVector GetSquadCentre(ASquadController* Squad);
	void ViewFrom(const FVector& Focus, float Distance, float Pitch, float Yaw) const;
	void RequestScreenshot(const FString& Label);
};
