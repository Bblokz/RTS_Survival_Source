// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "BehaviourAbilityClipCaptureSubsystem.generated.h"

class ACPPController;
class AActor;
class ASceneCapture2D;
class UTextureRenderTarget2D;
class UWeaponState;

enum class EBehaviourAbilityClipPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForUnits,
	// The tank fires at its normal rate and reloads, so the clip shows the difference.
	NormalFire,
	// The ability is on; recording runs until the T-26 has burned for a moment or the time is up.
	AbilityActive,
	Done
};

/** One clip: which tank uses which behaviour ability card on the parked T-26, and when. */
struct FBehaviourAbilityClipScenario
{
	// Command line switch that starts this clip, and the Saved/InfoTakeover/<FolderName>/Frames output folder.
	const TCHAR* CaptureSwitch = nullptr;
	const TCHAR* FolderName = nullptr;
	const TCHAR* TankClassPath = nullptr;
	EBehaviourAbilityType AbilityType = EBehaviourAbilityType::RapidFireAutoCannon;
	// Part of the behaviour class name that must show up on the tank once the card is clicked.
	const TCHAR* BehaviourNamePart = nullptr;
	// The ability goes on the moment this many reloads of the main gun have finished: a reload that is already
	// running keeps its old length, so the clip shows the normal pace first.
	int32 ReloadsBeforeAbility = 1;
	// Yaw of the T-26; picked per gun so it lives long enough for the ability to show.
	float TargetYaw = 0.f;
	// Weapons below this calibre (machine guns) are left out of the shot count.
	float MainGunMinimalCalibre = 20.f;
};

/**
 * @brief Records the frames of a behaviour ability field manual clip, started with -RattleFeedClipCapture or
 * -AdrenalineShotClipCapture (with -game -RenderOffscreen -benchmark -fps=24 so every game tick is the same length):
 * a player tank fires at a parked enemy T-26 at its normal rate, reloads, then fires with the ability on; every tick is
 * saved to Saved/InfoTakeover/<Folder>/Frames (or -AbilityClipDir=). Optional -AbilityClipAt=X,Y picks the spot,
 * -AbilityClipSettle= the seconds the units stand first. Grep ABILITY_CLIP (it logs the reloads and every main gun
 * shot). Quits when done.
 */
UCLASS()
class RTS_SURVIVAL_API UBehaviourAbilityClipCaptureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EBehaviourAbilityClipPhase M_Phase = EBehaviourAbilityClipPhase::Inactive;
	FBehaviourAbilityClipScenario M_Scenario;
	double M_PhaseStartGameSeconds = 0.0;
	// Game seconds at which the T-26 died; negative while it lives.
	double M_TargetDeathGameSeconds = -1.0;
	int32 M_FramesSaved = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	// Set once the ability's behaviour is seen on the tank, which proves the card did more than queue it.
	bool bM_HasSeenAbilityBehaviour = false;
	// Rounds in the main gun's magazine last tick; a drop is a shot, a rise a finished reload. INDEX_NONE until found.
	int32 M_LastMainGunRounds = INDEX_NONE;
	int32 M_ReloadsFinished = 0;
	// Saved-frame index of every main gun shot in the current phase, logged when the phase ends.
	TArray<int32> M_MainGunShotFrames;
	FVector M_Spot = FVector::ZeroVector;
	FString M_OutputDirectory;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<AActor> M_Tank;
	TWeakObjectPtr<AActor> M_Target;
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget;

	static bool FindScenarioOnCommandLine(FBehaviourAbilityClipScenario& OutScenario);

	void EnterPhase(const EBehaviourAbilityClipPhase NewPhase);
	double GetGameSeconds() const;
	double GetSecondsInPhase() const;

	void TickWaitingForGameStart();
	void TickWaitingForUnits();
	void TickNormalFire();
	void TickAbilityActive();
	void CheckAbilityBehaviourAppeared();

	bool SpawnUnits();
	bool CreateCamera();
	bool GetIsActorAlive(const TWeakObjectPtr<AActor>& Unit) const;
	void DisableTurretsOf(const TWeakObjectPtr<AActor>& Unit) const;
	void OrderAttack();
	void ActivateAbility();
	bool GetHasAbilityBehaviour() const;
	UWeaponState* FindMainGun() const;
	/** Counts main gun shots (magazine drops) and finished reloads (magazine rises). */
	void TrackMainGun();
	/** Logs the main gun shots of the phase that ends, with the gun's current cooldown, magazine and reload. */
	void LogMainGunShots(const TCHAR* PhaseName);
	void SaveFrame();
	void Fail(const FString& Reason);
	void Finish(const bool bSucceeded);
};
