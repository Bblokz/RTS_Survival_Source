// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#include "UnitPortraitCaptureSubsystem.generated.h"

class ACPPController;
class ASkyLight;
class ASceneCapture2D;
class ATankMaster;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UWeaponState;

enum class EUnitPortraitPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	SpawningUnit,
	Settling,
	WaitingForShot,
	CapturingShot,
	ClearingUnit,
	Done
};

/** A camera around the unit, relative to the unit's forward so every vehicle is pictured from the same side. */
struct FUnitPortraitView
{
	FString Name;
	// Degrees from the unit's nose; negative puts the camera on the unit's left.
	float YawFromUnitForward = 0.f;
	float Pitch = 0.f;
	// Only for comparing looks: the fog of war is a post-process material, so portraits leave them out.
	bool bKeepPostProcessMaterials = false;
};

/** One vehicle of the capture list: the file name stem and the Blueprint class to spawn. */
struct FUnitPortraitEntry
{
	FString Name;
	FString ClassPath;
};

/** A scene capture per view; it renders every frame so its exposure and temporal history settle before a picture. */
struct FUnitPortraitCamera
{
	FUnitPortraitView View;
	TWeakObjectPtr<ASceneCapture2D> Capture;
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
};

/** What is known of the unit's main gun while waiting for and picturing its shots. */
struct FUnitPortraitShotState
{
	TWeakObjectPtr<UWeaponState> MainWeapon;
	int32 LastMagazine = INDEX_NONE;
	int32 ShotCount = 0;
	float ShotGameSeconds = 0.f;
	// Index into the capture delays after a shot; the next picture to take.
	int32 NextMomentIndex = 0;
};

/**
 * @brief Takes the raw action pictures for unit portraits: spawns each listed vehicle on the map's
 * BP_UnitImage_LandscapeAnchor, orders it to attack the ground out of frame and saves square scene-capture pictures
 * right after its main gun fires. Started with -UnitPortraitCapture=<list file> (best with -game -RenderOffscreen so
 * nothing shows on screen); Tools/UnitPortraits has the runner and the crop-to-256 step. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UUnitPortraitCaptureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EUnitPortraitPhase M_Phase = EUnitPortraitPhase::Inactive;
	// Real seconds pace the start (the game boots paused); game seconds pace everything around the unit.
	double M_PhaseStartRealSeconds = 0.0;
	float M_PhaseStartGameSeconds = 0.f;

	TArray<FUnitPortraitEntry> M_Entries;
	int32 M_EntryIndex = 0;
	FString M_OutputDirectory;
	int32 M_ShotsPerUnit = 3;

	TArray<FUnitPortraitCamera> M_Cameras;
	TWeakObjectPtr<AActor> M_Anchor;
	TWeakObjectPtr<ASkyLight> M_SkyFill;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<ATankMaster> M_Unit;
	FUnitPortraitShotState M_ShotState;
	bool bM_IsSuppressingErrorDialogs = false;

	void TickWaitingForGameStart();
	void TickSpawningUnit();
	void TickSettling();
	void TickWaitingForShot();
	void TickCapturingShot();
	void TickClearingUnit();

	void EnterPhase(EUnitPortraitPhase NewPhase);
	double GetRealSecondsInPhase() const;
	float GetGameSecondsInPhase() const;
	float GetGameSeconds() const;

	bool LoadEntries();
	bool TryStartGame();
	// The game start tutorial opens on a difficulty picker that pauses the game; picks Normal and unpauses.
	void KeepGameRunning() const;
	bool FindAnchor();
	bool CreateCameras();
	void CopyPlayerCameraPostProcess(USceneCaptureComponent2D& CaptureComponent) const;
	// Logs the sky lights and post process volumes, for when pictures are lit differently from the game view.
	void LogMapLighting() const;
	void LogSunLights() const;
	void SpawnSkyFill();

	ATankMaster* SpawnUnit(const FUnitPortraitEntry& Entry) const;
	// Unattended runs never draw the unit; its meshes would skip their pose and it could die to the map's enemies.
	void KeepUnitReady() const;
	void AimCamerasAtUnit() const;
	bool GetUnitVisibleBounds(FBox& OutBounds) const;

	UWeaponState* FindMainWeapon() const;
	void OrderAttackGroundOutOfFrame();
	bool GetDidMainWeaponFire();
	void ReloadMainWeapon();

	/**
	 * @brief Saves what every camera rendered last frame, so one moment is pictured from all sides.
	 * @param MomentLabel Part of the file name telling when the picture was taken, e.g. shot2_t3.
	 */
	void ExportCameras(const FString& MomentLabel) const;
	/**
	 * @brief Projects the vehicle's bounds into the camera's picture so the crop step can square it on the vehicle.
	 * @param Camera Camera whose picture is cropped.
	 * @param OutCropBox Pixel box around the vehicle, with a margin; may reach past the picture edges.
	 * @return False when the unit or camera is gone.
	 */
	bool GetUnitCropBox(const FUnitPortraitCamera& Camera, FBox2D& OutCropBox) const;
	FString GetCurrentEntryName() const;
	void DestroyUnit();
	void Finish();

	bool GetIsValidAnchor() const;
	bool GetIsValidPlayerController() const;
};
