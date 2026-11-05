// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleTypes.h"

#include "AddOnArmorPanelsValidateSubsystem.generated.h"

class ACPPController;
class ASceneCapture2D;
class ATankMaster;
class UArmorCalculation;
class UMeshComponent;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

enum class EAddOnArmorPanelsValidateStep : uint8
{
	WaitingForGameStart,
	WaitingForModules,
	Intact,
	ProbeAllHidden,
	ProbeSection,
	FirstHit,
	HitUntilDestroyed,
	CrewRepair,
	FullService,
	Done
};

/** A scene capture of the validation, aimed at the tank every frame. */
struct FAddOnArmorPanelsCamera
{
	FString Name;
	float Yaw = 0.f;
	float Pitch = 0.f;
	float Distance = 0.f;
	TWeakObjectPtr<ASceneCapture2D> Capture;
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
};

/** Add-on armour measured through the armour calculation at one side of the tank. */
struct FAddOnArmorPanelsSideReading
{
	float RawArmor = 0.f;
	float StructuralArmor = 0.f;
	float AddOnArmor = 0.f;
	int32 PlateRegistrationId = INDEX_NONE;
	FString PlateName;
};

/** A shell aimed at a panel's centre, resolved by the armour calculation. */
struct FAddOnArmorPanelsShot
{
	FVector HitLocation = FVector::ZeroVector;
	float EffectiveArmor = 0.f;
	float RawArmor = 0.f;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	int32 PlateRegistrationId = INDEX_NONE;
};

/**
 * @brief Checks a tank with add-on armour panels end to end: started with -AddOnArmorPanelsValidate in an offscreen
 * game run (Tools/AddOnArmorBreak/run_panels_validate.sh). Spawns the tank, shoots its left panels through the
 * weapons' module damage event and checks module state, armour and the hidden-panel mask after the first hit, until
 * destroyed, after a crew repair stage and after a full service; pictures go to Saved/AddOnArmorBreak/<tag>.
 * -AddOnArmorPanelsInject=<hull mesh> adds the panels component and that hull to a tank Blueprint without them.
 * Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UAddOnArmorPanelsValidateSubsystem : public UTickableWorldSubsystem
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
	EAddOnArmorPanelsValidateStep M_Step = EAddOnArmorPanelsValidateStep::WaitingForGameStart;
	double M_StepStartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	bool bM_StepActionDone = false;

	FString M_VehicleClassPath;
	FString M_InjectedHullPath;
	FString M_InjectReplacedMeshPath;
	FString M_OutputDirectory;
	FString M_Tag;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<ATankMaster> M_Tank;
	TWeakObjectPtr<UArmorCalculation> M_Armor;
	TWeakObjectPtr<UStaticMeshComponent> M_PanelMesh;
	// Registered armour mesh shots resolve on: the panel mesh, or the mesh a bolted-on panel mesh hangs on.
	TWeakObjectPtr<UMeshComponent> M_ArmorMesh;
	FVector M_BaseLocation = FVector::ZeroVector;
	// Where the tank was on the last tick it was alive; reported when it disappears without a wreck.
	FVector M_LastTankLocation = FVector::ZeroVector;
	float M_NextWaitLogSeconds = 0.f;
	TArray<FAddOnArmorPanelsCamera> M_Cameras;
	bool bM_IsSuppressingErrorDialogs = false;
	bool bM_RunProbes = false;
	bool bM_StartModuleSetup = false;
	bool bM_DidStartModuleSetup = false;

	// Add-on modules in slot order (left zone, then right zone) and the layout's panels per side.
	int32 M_LeftModuleId = INDEX_NONE;
	int32 M_RightModuleId = INDEX_NONE;
	int32 M_LeftPanelMask = 0;
	int32 M_RightPanelMask = 0;
	TArray<int32> M_LeftPanels;
	// Left panels whose centre resolves to a plate the left zone covers; shells elsewhere never reach the zone.
	TArray<int32> M_CoveredLeftPanels;
	FAddOnArmorPanelsSideReading M_IntactLeft;
	FAddOnArmorPanelsSideReading M_IntactRight;

	int32 M_HitSerial = 0;
	int32 M_PreviousHiddenLeftCount = 0;
	float M_NextHitGameSeconds = 0.f;
	// Game seconds at which the left zone was seen destroyed; negative until then.
	float M_DestroyedGameSeconds = -1.f;
	int32 M_FailureCount = 0;

	void EnterStep(EAddOnArmorPanelsValidateStep NewStep);
	float GetGameSecondsInStep() const;
	bool TryStartGame();
	void KeepGameRunning() const;

	bool SpawnTank();
	void InjectPanels(ATankMaster& Tank) const;
	bool CollectModulesAndPanels();
	void CreateCameras();
	void AimCameras() const;
	void ExportPictures(const FString& Label) const;

	// Each step acts once, waits for the pictures to settle, then checks and moves on.
	void TickStep(EAddOnArmorPanelsValidateStep NextStep, TFunctionRef<void()> Action, TFunctionRef<void()> Check);
	void TickHitUntilDestroyed();

	/**
	 * @brief Fires a penetrating shell at a left panel through the weapons' module damage event.
	 * @param PanelIndex Layout index of the panel to hit.
	 * @return False when the armour resolved no plate at the panel.
	 */
	bool FireAtPanel(int32 PanelIndex);
	FAddOnArmorPanelsShot ResolveShotAtPanel(int32 PanelIndex) const;
	bool GetIsPanelOnLeft(int32 PanelIndex) const;
	FAddOnArmorPanelsSideReading ReadSide(bool bLeftSide) const;
	int32 GetHiddenPanelMask() const;
	// Probe: the panel section in the engine's grid material (true) or back in its own material (false).
	void ShowPanelSection(bool bShow) const;
	EVehicleModuleState GetModuleState(int32 ModuleId) const;

	void CheckIntact();
	void CheckFirstHit();
	void CheckDestroyed();
	void CheckCrewRepair();
	void CheckFullService();
	void CheckRightSideUntouched();
	void StartModuleSetupOnce();
	/** @brief Says every few seconds why the tank is not ready, so a stalled wait names its cause. */
	void LogModuleWaitReason();
	void Expect(bool bCondition, const FString& What);
	void Finish();
};
