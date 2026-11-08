// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"

#include "AbilityPreviewTestSubsystem.generated.h"

class AFactionPlayerController;
class UW_CommanderSelectionMenu;

/**
 * @brief Plays commander kit ability previews on RTS_FactionSelection as a player would, started with
 * -AbilityPreviewTest (non-shipping builds only). It walks to the commander screen, screenshots a hovered kit row,
 * then for each kit ability clicks its row, screenshots the desert while the ability runs and checks that the
 * preview returns to the hangar by itself (or on BACK with -AbilityPreviewTestManualBack), that the music swaps to
 * combat and back, that nothing the ability spawned is left and that no error was logged.
 * -AbilityPreviewTestOnly=GA_A+GA_B limits the run. Results and screenshots go to Saved/AbilityPreviewTest/;
 * log lines start with RTS_ABILITY_PREVIEW_TEST.
 */
UCLASS()
class RTS_SURVIVAL_API UAbilityPreviewTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FPreviewCase
	{
		EGlobalAbility Ability = EGlobalAbility::GA_None;
		ERTSCommander Commander = ERTSCommander::NoCommander;
		int32 SlotIndex = INDEX_NONE;
	};

	enum class EPhase : uint8
	{
		Boot,
		HoverShot,
		SelectCommander,
		ClickRow,
		Running,
		AfterClose,
		Done
	};

	TArray<FPreviewCase> M_Cases;
	int32 M_CaseIndex = INDEX_NONE;
	EPhase M_Phase = EPhase::Boot;
	double M_PhaseStart = 0.0;
	double M_RunStart = 0.0;
	double M_NextActionTime = 0.0;
	int32 M_ShotsTaken = 0;
	int32 M_HoverStep = 0;
	bool bM_SawFinished = false;
	bool bM_ShotAfterFinished = false;
	bool bM_ManualBackDone = false;
	bool bM_CheckedCombatMusic = false;
	bool bM_ManualBack = false;
	// -AbilityPreviewTestBurst: a screenshot every BurstShotInterval seconds while the ability runs, to see dives.
	bool bM_BurstShots = false;
	double M_NextBurstShotTime = 0.0;
	bool bM_LaunchRequested = false;
	bool bM_VariantsConfirmed = false;
	double M_FinishedTime = -1.0;
	int32 M_Passed = 0;
	int32 M_Failed = 0;
	TArray<FString> M_CaseNotes;
	FString M_MusicDuringPreview;
	// "kind/units/damaged/destroyed" of the preview's Soviet targets, taken when the ability finished.
	FString M_TargetSummary;
	// Most targets seen damaged and destroyed during the case: infantry may heal before the finished sample.
	int32 M_MaxTargetsDamaged = 0;
	int32 M_MaxTargetsDestroyed = 0;
	TArray<FString> M_ResultLines;
	TArray<TWeakObjectPtr<AActor>> M_CaseSpawns;
	FDelegateHandle M_SpawnHandle;
	TSharedPtr<class FAbilityPreviewTestErrorCounter> M_ErrorCounter;

	UPROPERTY()
	TWeakObjectPtr<AFactionPlayerController> M_Controller;

	UPROPERTY()
	TWeakObjectPtr<UW_CommanderSelectionMenu> M_Menu;

	void BuildCases();
	bool EnsureCommanderScreen(double Now);
	void TickHoverShot(double Now);
	void TickCase(double Now);
	void StartCase(double Now);
	void CheckRunning(double Now);
	void FinishCase(double Now);
	void Check(bool bCondition, const FString& Name, const FString& Detail = FString());
	void Shot(const FString& Name);
	FString GetCaseName() const;
	int32 CountLeftoverSpawns() const;
	FString GetMusicName() const;
	FString GetTrackName() const;
	void CheckTargetsSpawned();
	void RecordTargetHealth();
	void TrackTargetHealth();
	void Finish();
	void SetPhase(EPhase NewPhase, double Now);
};
