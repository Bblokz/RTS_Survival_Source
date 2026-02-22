// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/RTSProgressBarType.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapAbilityTypes.h"
#include "TurretSwapComp.generated.h"

class UChildActorComponent;
class ACPPTurretsMaster;
class URTSTimedProgressBarManager;
class ICommands;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FTurretSwapTimingProgressSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret|TimingProgress")
	float SwapTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret|TimingProgress")
	FVector AttachedAnimProgressBarOffset = FVector(0.0f, 0.0f, 200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret|TimingProgress")
	bool bUseDescriptionText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret|TimingProgress")
	ERTSProgressBarType BarType = ERTSProgressBarType::Default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret|TimingProgress")
	FString BarText = TEXT("Swapping turret");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret|TimingProgress")
	UMaterialInterface* OldTurretMaterialOverride = nullptr;

	static constexpr float BarScaleMlt = 0.1f;
};

USTRUCT(BlueprintType)
struct FTurretSwapAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret")
	int32 PreferredAbilityIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret")
	int32 Cooldown = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret")
	TSubclassOf<ACPPTurretsMaster> SwapTurretClass;

	// Subtype currently active on the command card for this component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret")
	ETurretSwapAbility ActiveSwapAbilityType = ETurretSwapAbility::DefaultSwitchAT;

	// Subtype that becomes active after this component finishes swapping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret")
	ETurretSwapAbility SwapBackAbilityType = ETurretSwapAbility::SwitchFlak;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SwapTurret")
	FTurretSwapTimingProgressSettings TimingProgress;
};

/**
 * @brief Turret swap ability component that owns the full runtime flow for replacing one mounted turret with another.
 * Attach this to a tank or building expansion that has one child actor component dedicated to a mounted turret. Configure
 * SwapTurretClass, cooldown, and the two swap ability subtypes so this component can swap command-card entries and toggle
 * which subtype is currently active after each completed swap.
 *
 * Setup workflow for designers:
 * 1) Add this component to the owning unit and configure TurretSwapAbilitySettings.
 * 2) Call InitTurretSwapComponent in blueprint with the child actor component that currently hosts a turret master.
 * 3) Ensure the unit command card contains only one SwapTurret ability entry for this component's active subtype.
 * 4) Use OldTurretMaterialOverride and timing/progress settings to control visuals during the swap window.
 *
 * Runtime flow:
 * - ExecuteTurretSwap disables the current turret, applies temporary materials, and shows a timed anchored progress bar.
 * - Once the timer completes, the old turret is removed from owner tracking, destroyed through the child actor component,
 *   and replaced by the configured turret class.
 * - The component then swaps command-card subtype, starts cooldown on the new subtype, refreshes behaviours, and re-enables
 *   auto-engage on the newly mounted turret.
 *
 * @note InitTurretSwapComponent: call in blueprint to bind the turret child actor component before any swap command.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTurretSwapComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurretSwapComp();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="TurretSwap")
	void InitTurretSwapComponent(UChildActorComponent* ChildActorComponent);

	void ExecuteTurretSwap();
	void TerminateTurretSwap();

	ETurretSwapAbility GetCurrentActiveSwapAbilityType() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TurretSwap")
	FTurretSwapAbilitySettings M_TurretSwapAbilitySettings;

private:
	void BeginPlay_AddAbility();
	void AddAbilityToCommands();
	void StartSwapProgressAndTimer();
	void ExecuteSwapNow();
	void CompleteSwapWithNewTurret(ACPPTurretsMaster* NewTurret);
	void UpdateOwnerTurretArrays(ACPPTurretsMaster* OldTurret, ACPPTurretsMaster* NewTurret) const;
	void ApplyMaterialOverrideToTurret(ACPPTurretsMaster* TurretToUpdate) const;
	void SwapCommandCardSubtypeAndStartCooldown();
	void StopProgressBar();
	void RefreshOwnerBehaviours() const;
	void SetOwnerTurretsAutoEngage() const;
	void SwapActiveAndSwapBackAbilityTypes();

	bool GetIsValidChildActorComponent() const;
	bool GetIsValidOwnerCommandsInterface() const;
	bool GetIsValidTimedProgressBarManager() const;
	bool GetIsValidSwapTurretClass() const;

	UPROPERTY()
	UChildActorComponent* M_TurretChildActorComponent = nullptr;

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	UPROPERTY()
	TWeakObjectPtr<URTSTimedProgressBarManager> M_TimedProgressBarManager;

	UPROPERTY()
	FTimerHandle M_SwapTimerHandle;

	UPROPERTY()
	int32 M_ProgressBarActivationId = 0;
};
