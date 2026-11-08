// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "Subsystems/WorldSubsystem.h"
#include "GlobalAbilityAreaTestSubsystem.generated.h"

class AAircraftMaster;
class AFlareProjectile;
class ACPPController;
class APlayerAimAbility;
class ARadixiteNuke;
class UGlobalAbility;
class UGlobalAbilitySetDataAsset;
class UUserWidget;
class UW_CommanderSelectionMenu;
class UW_GlobalAbilityPanel;

UENUM()
enum class EGlobalAbilityAreaTestStep : uint8
{
	WaitingForStart,
	// Every commander kit against the data asset, then pictures of the ability panel and the commander select screen.
	CommanderKits,
	// -GlobalAbilitySkyDrop only: the Sky-Drop Quartermaster's new abilities, each fired once after the kit checks.
	SkyDropMassiveFlare,
	SkyDropPuma,
	SkyDropVehicleParts,
	// Clicked through the controller: the fog must lift along the strip.
	ReconClicked,
	// The aircraft is shot down halfway: the reveal must stop there.
	ReconShotDown,
	FlareCurtain,
	// Clicked through the controller: the mines must land on the previewed spots.
	PlayerMinefield,
	// Executed the way the AI does; its mines must stay hidden from the player.
	EnemyMinefield,
	// The Soviet aircraft, flown from a point the way the AI does.
	EnemyRecon,
	// Clicked like the panel button: no aim, vision bubbles on hidden enemy units.
	SpiesInfiltration,
	Nuke,
	// Fired once the nuke's radiation is over, on the same spot: a low arc in, a blast and no radiation.
	V2Rocket,
	Finished
};

// A player looks at each stage of the aim for a moment; so does the test, which also lets the new shaders compile.
UENUM()
enum class EGlobalAbilityAreaAimTestStage : uint8
{
	None,
	Hovering,
	Drawing
};

/**
 * @brief Map test of the strip and area global abilities, started with -GlobalAbilityAreaValidate on a map with open
 * ground. Runs them through the real click path where a player would, and checks what they do on the ground.
 * -GlobalAbilityAreaCapture=<folder> also saves pictures of the target areas and effects. Starts with the commander kit
 * checks; -GlobalAbilityKitsOnly stops after them, -GlobalAbilityKitPictures=<folder> saves the panel and commander
 * select screen there. -GlobalAbilitySkyDrop fires the massive flare, the Puma drop and the vehicle parts drop once
 * after the kit checks. Logs RTS_ABILITY_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UGlobalAbilityAreaTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EGlobalAbilityAreaTestStep M_Step = EGlobalAbilityAreaTestStep::WaitingForStart;
	float M_StepStartSeconds = -1.f;
	float M_NextCaptureSeconds = 0.f;
	int32 M_CaptureIndex = 0;
	int32 M_FailedChecks = 0;
	int32 M_PassedChecks = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_LineStart = FVector::ZeroVector;
	FVector M_LineEnd = FVector::ZeroVector;
	bool bM_HasKilledAircraft = false;
	int32 M_BubblesAtKill = 0;
	int32 M_ExpectedMines = 0;
	bool bM_SawSovietRecon = false;

	EGlobalAbilityAreaAimTestStage M_AimStage = EGlobalAbilityAreaAimTestStage::None;
	FString M_AimLabel;
	bool bM_IsAimingStrip = false;
	// Everything flying when the test started; a map can have its own aircraft.
	TArray<TWeakObjectPtr<AAircraftMaster>> M_AircraftOfTheMap;

	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> M_Abilities;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_ActiveAbility;

	TWeakObjectPtr<AActor> M_CoreVictim;
	TWeakObjectPtr<AActor> M_EdgeVictim;
	TWeakObjectPtr<AActor> M_FarBystander;

	// Enemy tanks spawned out of sight for the spies, and every enemy unit that was hidden when they were sent.
	TArray<TWeakObjectPtr<AActor>> M_SpiesTargets;
	TArray<TWeakObjectPtr<AActor>> M_HiddenBeforeSpies;
	TArray<TWeakObjectPtr<AActor>> M_RevealedBySpies;
	bool bM_HasSentSpies = false;

	// The V2 is aimed only after the nuke's radiation ends; its flight is measured while it is in the air.
	TWeakObjectPtr<ARadixiteNuke> M_V2Rocket;
	bool bM_HasAimedV2Rocket = false;
	float M_V2MaxApproachDistance = 0.f;
	float M_V2MaxHeight = 0.f;
	float M_V2BlastSeconds = -1.f;

	FB4HowitzerCaptureRig M_CaptureRig;

	// The commander kits step draws a stand-alone ability panel and the commander select screen into pictures.
	UPROPERTY()
	TObjectPtr<UW_GlobalAbilityPanel> M_KitPanel;

	UPROPERTY()
	TObjectPtr<UW_CommanderSelectionMenu> M_CommanderMenu;

	FString M_KitPictureDirectory;
	int32 M_KitPicturePhase = 0;

	// Sky-Drop steps: what the Puma drop and the vehicle parts drop left on the ground.
	TArray<TWeakObjectPtr<AActor>> M_PumasBeforeDrop;
	TArray<TWeakObjectPtr<AActor>> M_LandedSupplyCrates;
	int32 M_ExpectedSupplyCrates = 0;
	// Set once the step has seen its first result (flare ignited, Puma unloaded); the step clock restarts there.
	bool bM_HasCapturedSkyDropResult = false;

	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());
	float GetStepSeconds() const;
	void EnterStep(EGlobalAbilityAreaTestStep NewStep);
	void Finish();

	UGlobalAbility* MakeAbility(const EGlobalAbility AbilityType, const int32 OwningPlayer);
	ACPPController* GetController() const;
	APlayerAimAbility* FindAimActor() const;
	bool FindGround(const FVector2D& Location, FVector& OutGround) const;

	/**
	 * @brief Starts aiming an ability as a player does; TickAim then clicks it through the controller.
	 * @param Ability The ability; made free so the test needs no resources.
	 * @param Label Name of the pictures taken of the preview.
	 */
	void StartAim(UGlobalAbility* Ability, const FString& Label);

	/** @return True while the aim is still going, so the step must not look at the result yet. */
	bool TickAim();
	void ClickStripStart();
	void ClickFinalLocation();
	void FinishPendingShaders() const;
	void Capture(const FVector& Focus, const FString& Label) const;
	void CaptureEvery(const float IntervalSeconds, const FVector& Focus, const FString& Label);

	void TickWaitingForStart();
	void StartRecon();
	void TickRecon();
	void StartReconShotDown();
	void TickReconShotDown();
	void StartFlareCurtain();
	void TickFlareCurtain();
	void StartPlayerMinefield();
	void TickPlayerMinefield();
	void StartEnemyMinefield();
	void TickEnemyMinefield();
	void StartEnemyRecon();
	void TickEnemyRecon();
	void StartCommanderKits();
	void TickCommanderKits();
	void CheckPlayerLoadoutFitsPanel();

	/**
	 * @brief One kit: seven abilities, no ability twice, and per slot a template whose tier fits the slot.
	 * @param Commander The kit's commander, for the log.
	 * @param Kit Its config entry.
	 * @param AbilitySet DA_GlobalAbilitySystem.
	 * @param OutMissingTemplates Gets every kit ability without a template (still being authored elsewhere).
	 */
	void CheckCommanderKit(const ERTSCommander Commander, const FRTSCommanderSettings& Kit,
	                       const UGlobalAbilitySetDataAsset* AbilitySet, TArray<FString>& OutMissingTemplates);

	void CheckKitSlot(const FString& SlotText, const int32 SlotIndex, const EGlobalAbility Ability,
	                  const UGlobalAbilitySetDataAsset* AbilitySet, TArray<FString>& OutMissingTemplates);

	UGlobalAbilitySetDataAsset* LoadAbilitySet() const;
	void BuildKitPanelPicture();
	void BuildCommanderMenuPicture();
	void FinishCommanderKits();

	void StartSkyDropMassiveFlare();
	void TickSkyDropMassiveFlare();
	void CheckMassiveFlare(const AFlareProjectile& Flare);
	void StartSkyDropPuma();
	void TickSkyDropPuma();
	TArray<AActor*> FindLandedPumas() const;
	void StartSkyDropVehicleParts();
	void TickSkyDropVehicleParts();
	void CheckVehiclePartsCrates();
	// The kit-only run ends after the Sky-Drop steps; the full run goes on with the area abilities.
	void FinishSkyDrop();

	/** @return True when the widget was drawn and its picture written to the kit picture folder. */
	bool SaveWidgetPicture(UUserWidget* Widget, const FString& Name) const;
	void CheckSovietNukeTemplate();
	void StartSpiesInfiltration();
	void TickSpiesInfiltration();
	void SendSpies();
	void CheckSpiesReveal();
	TArray<AActor*> GetHiddenEnemyUnits() const;
	void StartNuke();
	AActor* SpawnVictimNearAnchor(const FVector2D& Offset, const uint8 OwningPlayer) const;
	void TickNuke();
	void CheckNukeResult();
	void StartV2Rocket();
	void AimV2Rocket();
	void TickV2Rocket();
	void TrackV2Flight();
	void CheckV2Result();
	bool IsAnyNukeActive() const;

	int32 CountBubbles() const;
	AAircraftMaster* FindRunAircraft() const;
};
