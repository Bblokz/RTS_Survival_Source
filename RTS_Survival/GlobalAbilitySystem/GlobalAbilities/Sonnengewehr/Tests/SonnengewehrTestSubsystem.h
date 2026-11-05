// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SonnengewehrTestSubsystem.generated.h"

class ACameraActor;
class ASonnengewehrBeam;
class ASquadController;
class UGA_Sonnengewehr;
class UGlobalAbility;

UENUM()
enum class ESonnengewehrTestPhase : uint8
{
	WaitingForStart,
	// Enemy tanks and squads stand on and beside the strip; squads still spawn their soldiers.
	PlacingVictims,
	// The ability was clicked; the beam warns, sweeps and fades.
	Running,
	// The sweep is over; the scorch mark is photographed and the checks run.
	AfterSweep,
	Finished
};

UENUM()
enum class ESonnengewehrVictimLane : uint8
{
	// Inside the beam width: must burn.
	OnPath,
	// Well outside the beam width: must not be touched.
	OffPath,
	// Near the edge of the beam: logged, not checked.
	Edge
};

/**
 * @brief -SonnengewehrValidate: places enemy T-34s and Mosin squads on and beside a 50 m strip at the anchor (camera
 * centre, or -SonnengewehrAnchorX= / -SonnengewehrAnchorY=), fires GA_GerSonnengewehr through the player's two strip
 * clicks and checks the cost, that everything on the path burns in sweep order while nothing beside it does, and that
 * the ability ends once, after the last burn. Screenshots to Saved/SonnengewehrTest; logs RTS_SONNEN_TEST.
 * Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API USonnengewehrTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FVictim
	{
		TWeakObjectPtr<AActor> Actor;
		// Identity only, never dereferenced: a unit the beam kills is already pending kill when the burn is reported,
		// so its weak pointer no longer resolves.
		const AActor* ActorKey = nullptr;
		FString Label;
		bool bIsTank = false;
		ESonnengewehrVictimLane Lane = ESonnengewehrVictimLane::Edge;
		// Position relative to the strip when the ability is clicked.
		float Along = 0.f;
		float Lateral = 0.f;
		float StartHealth = 0.f;
		float LastHealth = 0.f;
		// From the beam's OnActorBurnt: run time, beam distance along the strip and damage after resistances.
		float BurntSeconds = -1.f;
		float BurntBeamDistance = -1.f;
		float BurntDamage = 0.f;
		// Where the victim stood along the strip when it burnt; soldiers move between the click and the burn.
		float BurntAlong = 0.f;
		int32 BurnCount = 0;
		// From polling the health: the first frame the health dropped or the unit died.
		float HurtSeconds = -1.f;
		bool bDied = false;
	};

	ESonnengewehrTestPhase M_Phase = ESonnengewehrTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	float M_RunStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;

	FVector M_Anchor = FVector::ZeroVector;
	FVector M_StripStart = FVector::ZeroVector;
	FVector M_StripEnd = FVector::ZeroVector;
	FVector M_StripDirection = FVector::ForwardVector;

	TArray<FVictim> M_Victims;
	TArray<TWeakObjectPtr<ASquadController>> M_Squads;
	// Squad label per squad, for the soldiers it spawns.
	TArray<FString> M_SquadLabels;

	float M_BeamSpawnSeconds = -1.f;
	float M_SweepStartSeconds = -1.f;
	float M_SweepEndSeconds = -1.f;
	float M_AbilityEndSeconds = -1.f;
	float M_LastBurnSeconds = -1.f;
	int32 M_AbilityEndCount = 0;
	int32 M_BurnsAtAbilityEnd = 0;
	int32 M_TotalBurns = 0;
	bool bM_TookWarningShot = false;
	bool bM_TookIgnitionShot = false;
	bool bM_TookMidShot = false;
	bool bM_TookLateShot = false;
	bool bM_TookGlowingScorchShot = false;

	TWeakObjectPtr<ASonnengewehrBeam> M_Beam;

	UPROPERTY()
	TObjectPtr<UGA_Sonnengewehr> M_Ability;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetRunSeconds() const;
	void EnterPhase(ESonnengewehrTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void BuildStrip();
	void SpawnVictims();
	AActor* SpawnEnemy(const TCHAR* ClassPath, float Along, float Lateral);
	void TickPlacingVictims();
	void CollectSquadSoldiers();
	void ClassifyVictims();
	void AddVictim(AActor* Actor, const FString& Label, bool bIsTank);

	void FireAbility();
	bool CheckTemplate(const UGA_Sonnengewehr& Ability);
	bool EnsureCanAfford(const UGA_Sonnengewehr& Ability, TMap<uint8, int32>& OutBefore);
	void CheckPaymentAndCooldown(const UGA_Sonnengewehr& Ability, const TMap<uint8, int32>& Before);
	void OnBeamSpawned(UGlobalAbility* Ability, ASonnengewehrBeam* Beam);
	void OnActorBurnt(ASonnengewehrBeam* Beam, AActor* Actor, float Damage);
	void OnAbilityEnded(UGlobalAbility* Ability);

	void TickRunning();
	void PollVictimHealth();
	void TrackBeamPhases();
	void TickAfterSweep();
	void RunFinalChecks();
	void CheckVictimLanes();
	void CheckSweepOrder();
	void CheckTiming();

	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
};
