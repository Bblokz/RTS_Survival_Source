// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IntroCinematicTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "IntroCinematicSubsystem.generated.h"

class ALevelSequenceActor;
class UMovieSceneSequencePlayer;
class UW_CinematicOverlay;
class UW_IntroCinematicHud;

/**
 * @brief Runs the screen layer of the 1976 intro cinematic. It only wakes up in the map RTS_IntroCinematic_1976:
 * there it finds the master sequence (played by the map's own sequence actor or by Movie Render Queue), shows the
 * HUD and the subtitles on the sequence clock, and switches off what the streamed mission maps bring along
 * (fog of war post process, error dialogs, placed gameplay units).
 * Needs no setup: the cues come from DT_IntroCues.
 */
UCLASS()
class RTS_SURVIVAL_API UIntroCinematicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	bool bM_IsActive = false;

	UPROPERTY()
	TArray<FIntroCinematicCueRow> M_Cues;

	// Streamed-level actors to remove (from the HideActors cues): Key = part of the class name, or of the mesh
	// name for static mesh actors; Value = part of the level name it applies to (empty = every streamed level).
	TArray<TPair<FString, FString>> M_RemovalRules;

	FDelegateHandle M_LevelAddedHandle;
	FDelegateHandle M_LevelRemovedHandle;

	UPROPERTY()
	TObjectPtr<UW_IntroCinematicHud> M_Hud;

	UPROPERTY()
	TWeakObjectPtr<UW_CinematicOverlay> M_Subtitles;

	UPROPERTY()
	TWeakObjectPtr<UMovieSceneSequencePlayer> M_SequencePlayer;

	// The world is swept for fog of war and gameplay actors once per interval, not every frame.
	float M_SecondsUntilWorldSweep = 0.0f;

	bool bM_AutoQuit = false;
	float M_ScreenshotIntervalSeconds = 0.0f;
	float M_NextScreenshotSeconds = 0.0f;
	float M_NextProgressLogSeconds = 0.0f;

	void ReportProgress(const float SequenceSeconds);
	void ApplyMaterialRamps(const float SequenceSeconds) const;
	void ApplyMaterialRamp(const FIntroCinematicCueRow& Cue, const float SequenceSeconds) const;

	UFUNCTION()
	void HandleSequenceFinished();

	bool GetIsValidHud() const;
	bool GetIsValidSequencePlayer() const;

	void LoadCues();
	void EnsureHud();
	void EnsureSequencePlayer();
	UMovieSceneSequencePlayer* FindMasterSequencePlayer() const;
	void SweepWorld();
	void RemoveFogOfWar() const;
	void RemoveGameplayActors() const;
	bool GetShouldRemoveActor(const AActor* Actor, const FString& LevelName) const;
	void HideRuleMatchedMeshComponents(const AActor* Actor, const FString& LevelName) const;
	bool GetIsNameMatchedByRule(const FString& Name, const FString& LevelName) const;

	/** Takes other screens' widgets (the old intro's captions) and world-space unit widgets (health bars) away. */
	void RemoveForeignWidgets() const;

	/** A mission map was switched on: take out what must not run or show in the film before it is drawn. */
	void HandleLevelAdded(ULevel* Level, UWorld* World);

	/** A mission map is switched off: its actors' timers would fire into a world they have left. */
	void HandleLevelRemoved(ULevel* Level, UWorld* World);

	void StopForeignSequences(const ULevel* Level) const;
};
