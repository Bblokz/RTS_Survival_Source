// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "RTS_Survival/GameUI/CameraTour/MissionCameraTourTypes.h"

#include "MissionCameraTourSession.generated.h"

class ACPPController;
class UMissionBase;
class UPlayerCameraController;
class UPlayerPortraitManager;
class UW_HoldToSkip;
class UW_MissionCameraTour;
class UW_Portrait;

/** How a camera tour session ended; decides what the mission reports to its Blueprint. */
enum class EMissionCameraTourEnd : uint8
{
	// Played to the end.
	Finished,
	// The screen stayed busy (cinematic, info takeover, tech tree) for too long; the tour never showed.
	SkippedScreenBusy,
	// Ended early by the mission (failure, clean-up); nobody is told.
	Cancelled
};

/**
 * @brief Runs one mission camera tour: waits until the screen is free, takes it over (HUD hidden, camera input
 * locked), moves the RTS camera through the shots while W_MissionCameraTour shows bars and captions, eases back to the
 * player's own view, then hands the screen back and reports to the mission. One real-time clock drives camera and
 * overlay. Created and owned by UMissionBase::PlayCameraTour; RequestCancel ends it early with the same clean-up.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionCameraTourSession : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Queues the tour; it starts at once when the screen is free, else as soon as it frees up.
	 * @return False when the player controller or camera is missing or there are no shots; nothing changes then.
	 */
	bool StartSession(UMissionBase* MissionOwner, const TArray<FMissionCameraTourShot>& Shots,
	                  const FMissionCameraTourSettings& Settings, const int32 ID);

	// Ends the tour now and gives the player the camera, zoom and HUD back.
	void RequestCancel();

	bool GetHasCompleted() const { return bM_HasCompleted; }
	bool GetIsHoldingScreen() const { return bM_IsRunning; }

	// Index of the shot on screen; INDEX_NONE while waiting or easing back after the last shot.
	int32 GetCurrentShotIndex() const { return M_CurrentShotIndex; }

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	/** A camera state the tour blends between. */
	struct FTourPose
	{
		FVector Focus = FVector::ZeroVector;
		float Yaw = 0.f;
		float Pitch = 0.f;
		float ArmLength = 0.f;
	};

	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_Mission;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<UPlayerCameraController> M_CameraController;

	UPROPERTY()
	TObjectPtr<UW_MissionCameraTour> M_Widget;

	// Only while a skippable tour holds the screen (FMissionCameraTourSettings::bAllowSkipping).
	UPROPERTY()
	TObjectPtr<UW_HoldToSkip> M_SkipWidget;

	TArray<FMissionCameraTourShot> M_Shots;
	FMissionCameraTourSettings M_Settings;
	int32 M_ID = 0;

	// The player's view when the tour took the screen; the tour eases back to it.
	FTourPose M_PlayerPose;
	FVector M_PlayerPawnLocation = FVector::ZeroVector;
	FRotator M_ArmRelativeRotation = FRotator::ZeroRotator;
	// Where the camera was when the current shot (or the return) began; the blend-in starts from it.
	FTourPose M_BlendFromPose;
	FTourPose M_LastAppliedPose;

	double M_StartRealSeconds = 0.0;
	float M_SecondsWaitedForScreen = 0.f;
	int32 M_CurrentShotIndex = INDEX_NONE;
	bool bM_IsReturning = false;
	bool bM_IsWaitingForScreen = false;
	bool bM_HasCameraPose = false;
	bool bM_IsRunning = false;
	bool bM_HasCompleted = false;

	/** The HUD portrait's class and screen rect, read before the takeover hides the HUD. */
	struct FHudPortraitPlace
	{
		TSubclassOf<UW_Portrait> PortraitClass = nullptr;
		FVector2D ViewportPosition = FVector2D::ZeroVector;
		FVector2D ViewportSize = FVector2D::ZeroVector;
		// False when the HUD portrait was never laid out; the copy then fills the screen (its canvas places it).
		bool bHasRect = false;
	};

	FHudPortraitPlace GetHudPortraitPlace() const;
	UPlayerPortraitManager* GetPortraitManager() const;

	/** The HUD is hidden during the tour: a portrait copy on the overlay shows speaker and words of voice lines. */
	void HostPortraitOnTour(const FHudPortraitPlace& HudPortraitPlace);

	/** Hands portraits back to the HUD portrait. */
	void ReleaseTourPortrait() const;

	void TickWaitingForScreen(const float DeltaTime);
	void TickRunning();
	bool TryTakeScreen();
	bool CachePlayerPose();

	/** Plays Settings.StartSound; with a transcript its words show in the cinematic subtitle strip at the top. */
	void PlayStartSound() const;

	/** Lifts the fog along every shot with a RevealRadius (and the blend into it), for the whole tour plus a margin. */
	void RevealShotAreas() const;

	/**
	 * @brief Reveals a strip from From to To with overlapping vision bubbles, so a moving shot never looks into fog.
	 * @param From Start of the strip.
	 * @param To End of the strip.
	 * @param Radius Bubble radius; also the spacing of the bubbles.
	 * @param Seconds How long each bubble stays.
	 */
	void RevealAlongPath(const FVector& From, const FVector& To, const float Radius, const float Seconds) const;

	bool GetIsValidSkipWidget(const bool bReportError = true) const;

	void CreateSkipWidget();
	void RemoveSkipWidget();

	// Hold-to-skip finished: the clock jumps to the end of the shots, so the next tick eases back to the player.
	UFUNCTION()
	void HandleSkipHoldCompleted();

	// Shot timeline: start second of each shot, then the return, then the end.
	float GetShotStartSeconds(const int32 ShotIndex) const;
	float GetShotsTotalSeconds() const;
	float GetTotalSeconds() const;

	void EnterShot(const int32 ShotIndex);
	void EnterReturn();
	FTourPose GetShotPose(const FMissionCameraTourShot& Shot, const float ShotAlpha) const;
	static FTourPose BlendPoses(const FTourPose& From, const FTourPose& To, const float Alpha);
	void ApplyPose(const FTourPose& Pose);
	void UpdateWidget(const float ElapsedSeconds) const;

	/** Ends the tour: hands camera, zoom and screen back when it held them and reports EndReason. */
	void CompleteSession(const EMissionCameraTourEnd EndReason);
	void RestorePlayerView();
	float GetElapsedSeconds() const;

	bool GetIsValidPlayerController(const bool bReportError = true) const;
	bool GetIsValidCameraController(const bool bReportError = true) const;
	bool GetIsValidWidget(const bool bReportError = true) const;
};
