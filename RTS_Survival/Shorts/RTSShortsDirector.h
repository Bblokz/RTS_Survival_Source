// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSShortsJobTypes.h"

#include "RTSShortsDirector.generated.h"

class ACineCameraActor;
class ACPPController;
class UMoviePipeline;
class USoundMix;

/** Phases of one shorts run; the director advances through them in order. */
UENUM()
enum class ERTSShortsDirectorPhase : uint8
{
	WaitingForController,
	WaitingForStartDialog,
	SpawningUnits,
	WaitingForTurrets,
	Running,
	Finished
};

/**
 * @brief Runs one shorts job on the RTS_Shorts stage: confirms the start dialog, hides the UI, spawns and parks
 * the units, then shows them one per segment and orders attack-ground while Movie Render Queue renders.
 * Timing is keyed on the rendered output frame so the manifest maps every segment to exact frames.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSShortsDirector : public AActor
{
	GENERATED_BODY()

public:
	ARTSShortsDirector();

	/** Hands the job to the director; must be called before BeginPlay runs its first tick. */
	void InitShortsDirector(const FRTSShortsJob& Job);

	const FRTSShortsManifest& GetManifest() const { return M_Manifest; }
	bool GetIsFinished() const { return M_Phase == ERTSShortsDirectorPhase::Finished; }

	/** Writes Manifest.json into the job output directory; safe to call repeatedly. */
	void WriteManifest();

	/** Called by the render subsystem when Movie Render Queue finished; ends the open segment and seals the manifest. */
	void NotifyRenderFinished();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void TickWaitingForController();
	void TickWaitingForStartDialog();
	void TickSpawningUnits();
	void TickWaitingForTurrets();
	void TickRunning();

	bool FindStageActors();
	void ConsiderStageActor(AActor* Actor, const FString& WantedActorName, const FName WantedStage);
	void HideGameUI() const;
	void PushShortsSoundMix();
	void LockViewOnCamera() const;
	void ApplyStageVisibility() const;

	AActor* SpawnSegmentUnit(const int32 SegmentIndex);
	AActor* SpawnPitAsPlaced(const FString& ClassPath, const FTransform& Transform) const;
	static TArray<class ACPPTurretsMaster*> GetUnitTurrets(const AActor* Unit);
	static bool GetHasTurretOwner(const AActor* Unit);
	FTransform GetPenTransform(const int32 SegmentIndex) const;

	/** @return The current timeline frame: rendered output frame when rendering, else world time * fps. */
	int32 GetTimelineFrame() const;
	UMoviePipeline* GetActivePipeline() const;

	void FlushStageEffects(const int32 SegmentIndex) const;
	void BeginSegment(const int32 SegmentIndex);
	void EndSegment(const int32 SegmentIndex);
	void PlaceUnitOnStage(AActor* Unit, const FRTSShortsSegment& Segment) const;
	void ParkUnit(AActor* Unit, const int32 SegmentIndex) const;

	/** The actor the camera frames: the unit itself, or for squads the team weapon or the leader soldier. */
	const AActor* GetFramingSubject(const AActor* Unit) const;

	/** Cuts runtime-spawned particle effects after Segment.EffectMaxSeconds and keeps exhaust emitters off. */
	void LimitStageEffects(AActor* Unit, const struct FRTSShortsSegment& Segment, const int32 Frame);

	/** True when an effect first seen at FirstSeenFrame appeared after Segment.EffectLimitUntilSeconds. */
	bool GetIsEffectExemptFromLimits(const struct FRTSShortsSegment& Segment, const int32 FirstSeenFrame) const;

	/** Frame at which each active runtime effect was first seen (for EffectMaxSeconds). */
	TMap<TWeakObjectPtr<class UFXSystemComponent>, int32> M_EffectFirstSeen;

	/** Scale each tracked runtime effect had when it was first seen (the fade-out shrinks from it). */
	TMap<TWeakObjectPtr<class UFXSystemComponent>, FVector> M_EffectBaseScale;
	/** Effects this director deactivated after EffectMaxSeconds; one that turns active again is a new shot. */
	TSet<TWeakObjectPtr<class UFXSystemComponent>> M_EffectStopped;

	/** Squads: stops projected decals from painting the soldiers' skeletal meshes. */
	void SuppressUnitDecals(AActor* Unit, const int32 SegmentIndex) const;
	/** Segment.bDisarmTarget: the spawned dummy target can be shot but never fires back. */
	void DisarmSegmentTarget() const;
	void PlaceSquadMembers(AActor* Unit, const FVector& Location, const FRotator& Rotation) const;
	void FrameCamera(const AActor* Unit, const FRTSShortsSegment& Segment, const FVector& TargetWorld) const;
	void FrameImpactCamera(const AActor* Unit, const FRTSShortsSegment& Segment) const;
	void ApplyCameraCut(const FVector& CameraLocation) const;
	void ForceAnimationTicking(AActor* Unit) const;
	void DisableUnitOptimisation() const;
	void ParkStartingUnits() const;
	void KeepAudioUnmuted() const;
	void ForceSoundsNonSpatialized() const;
	void RunAudioProbe(const int32 Local) const;
	void LogActiveSounds(const int32 Frame) const;
	void BeginRealtimeAudioRecording();
	void EndRealtimeAudioRecordingAndExit();
	void StartSubmixCapture();
	void StopSubmixCapture();
	void UpdateSubmixCaptureWindow(const int32 Frame);
	void WriteSubmixCaptureWav();
	FVector GetTargetWorldLocation(const FRTSShortsSegment& Segment, const AActor* Unit) const;
	float ReadLargestWeaponRange(const AActor* Unit) const;
	void OrderAttackGround(const int32 SegmentIndex);
	void OrderStop(const int32 SegmentIndex);
	int32 ReadLargestCaliberMm(const AActor* Unit) const;
	void MarkSegmentSkipped(const int32 SegmentIndex, const FString& Reason);

	bool GetIsValidPlayerController() const;
	bool GetIsValidStage() const;
	bool GetIsValidCamera() const;

	FRTSShortsJob M_Job;
	FRTSShortsManifest M_Manifest;

	ERTSShortsDirectorPhase M_Phase = ERTSShortsDirectorPhase::WaitingForController;
	double M_PhaseStartRealSeconds = 0.0;
	double M_RunStartWorldSeconds = 0.0;
	double M_AudioRecordStartWorldSeconds = 0.0;

	/** Frame at which each segment starts; one extra entry holds the end of the last segment. */
	TArray<int32> M_SegmentStartFrames;
	int32 M_CurrentSegment = -1;
	bool bM_FireOrdered = false;
	bool bM_StopOrdered = false;
	bool bM_SecondFireOrdered = false;
	/** Per segment: squad members hidden by MaxSquadVisible (their attack orders are cancelled every tick). */
	mutable TArray<TArray<TWeakObjectPtr<class ASquadUnit>>> M_HiddenSoldiers;
	int32 M_StopAfterFireFrame = -1;
	int32 M_HoldFramesLeft = 0;
	FVector M_PlacedLocation = FVector::ZeroVector;
	bool bM_ImpactCutDone = false;

	/** Own capture of the main submix (see FRTSShortsAudioCapture); clock window of frames 0..TotalFrames. */
	TSharedPtr<class FRTSShortsAudioCapture, ESPMode::ThreadSafe> M_AudioCapture;
	bool bM_CaptureStartClockSet = false;
	double M_CaptureStartClock = 0.0;
	double M_CaptureEndClock = 0.0;
	bool bM_CaptureWritten = false;
	bool bM_StageMatchedByName = false;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> M_Units;

	/** Team weapon actor spawned by each segment's squad (null for tanks and plain infantry). */
	TArray<TWeakObjectPtr<AActor>> M_SegmentTeamWeapons;
	/** Dummy enemy actor of the running segment (TargetClassPath), destroyed at the segment's end. */
	TWeakObjectPtr<AActor> M_SegmentTarget;

	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TObjectPtr<AActor> M_Stage;

	UPROPERTY()
	TObjectPtr<AActor> M_Pen;

	UPROPERTY()
	TObjectPtr<ACineCameraActor> M_Camera;

	UPROPERTY()
	TObjectPtr<USoundMix> M_SoundMix;

	bool bM_HasSuppressedDialogs = false;
};
