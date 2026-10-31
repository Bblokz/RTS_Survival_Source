// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "Subsystems/WorldSubsystem.h"
#include "GlobalAbilityAreaTestSubsystem.generated.h"

class AAircraftMaster;
class ACPPController;
class APlayerAimAbility;
class UGlobalAbility;

UENUM()
enum class EGlobalAbilityAreaTestStep : uint8
{
	WaitingForStart,
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
	Nuke,
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
 * -GlobalAbilityAreaCapture=<folder> also saves pictures of the target areas and effects. Logs RTS_ABILITY_TEST; not
 * in shipping.
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

	FB4HowitzerCaptureRig M_CaptureRig;

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
	void CheckPlayerLoadoutFitsPanel();
	void CheckSovietNukeTemplate();
	void StartNuke();
	void TickNuke();
	void CheckNukeResult();

	int32 CountBubbles() const;
	AAircraftMaster* FindRunAircraft() const;
};
