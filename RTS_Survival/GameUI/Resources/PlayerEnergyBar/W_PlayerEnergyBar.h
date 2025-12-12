// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_PlayerEnergyBar.generated.h"

class UImage;
class UW_EnergyBarInfo;
class UPlayerResourceManager;
class UProgressBar;

/**
 * @brief Non-linear energy bar with tiered scales 
 *
 * The bar maps the player's *current* energy supply into a "window" (tier). Each window defines
 * how much energy equals a full bar (100%) for that tier. When supply crosses a window’s capacity,
 * the bar resets to 0% in the next window (with its own capacity), giving continuous, observable progress
 * at any production level. Demand is visualized as a movable "level" overlay within the *current* window.
 *
 * - If demand > supply, the level appears above the fill (deficit) and the bar turns red.
 * - If demand <= supply, the level is at/below the fill and the bar is green.
 *
 * Configure the non-linear scale via EnergyWindowSizes. When supply exceeds the sum of defined windows,
 * the last window size repeats indefinitely (to support any amount of energy).
 */
UCLASS()
class RTS_SURVIVAL_API UW_PlayerEnergyBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the player energy bar with the resource manager.
	 * @param NewPlayerResourceManager Resource manager for energy values.
	 * @param NewEnergyInfoWidget Info widget to show on hover.
	 */
	void InitPlayerEnergyBar(UPlayerResourceManager* NewPlayerResourceManager, UW_EnergyBarInfo* NewEnergyInfoWidget);

protected:
	/** Sets up update timer and caches base position for the level overlay. */
	virtual void NativeConstruct() override;

	virtual void BeginDestroy() override;

	/** Hover gateway invoked by internal hover detection. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnHoverEnergyBar(const bool bIsHover);

	/** Polls resource manager and updates visuals. */
	UFUNCTION()
	void UpdateEnergyBar();

	// Track hover state over the ProgressBar itself
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Player resource manager */
	UPROPERTY()
	TObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;

	/** Progress bar that visualizes *supply within current window* (0..1) */
	UPROPERTY(meta = (BindWidget))
	UProgressBar* EnergyProgressBar;

	/** Horizontal line/image showing the *demand* within current window (moves vertically). */
	UPROPERTY(meta = (BindWidget))
	UImage* EnergyBarLevel;

	UPROPERTY(BlueprintReadOnly, Category = "Description")
	TObjectPtr<UW_EnergyBarInfo> EnergyBarInfoWidget;

	// The offset that is the lowest possible for the energy level to be, this offset is added to the y position in its
	// canvas slot to make sure it does not go out of the energy bar on which it is placed on top.
	// In UI - y goes up so this value is positive.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnergyBarLevel")
	float EnergyLevelLowestYOffset = 120.f;

	// The offset that is the highest possible for the energy level to be, this offset is added to the y position in its
	// canvas slot to make sure it does not go out of the energy bar on which it is placed on top.
	// In UI - y goes up so this value is negative.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnergyBarLevel")
	float EnergyLevelHighestYOffset = -120.f;

	/** Window sizes (energy per full bar) for each tier; last entry repeats for unbounded scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnergyScale")
	TArray<int32> EnergyWindowSizes { 300, 150, 200 }; // example defaults; tweak in BP/INI

	/** OK / Deficit colors. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnergyColors")
	FLinearColor OkColor = FLinearColor::Green;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnergyColors")
	FLinearColor DeficitColor = FLinearColor::Red;

private:
	/** Timer handle for updating the energy bar */
	FTimerHandle EnergyBarUpdateTimerHandle;

	// Remember last hover state to detect transitions without spamming
	bool bM_WasProgressBarHovered = false;

	// Base Y (center) position captured from the canvas slot of EnergyBarLevel (we add offsets to this).
	float M_EnergyLevelBasePosY = 0.f;
	bool bM_HasBasePosY = false;

	// ----------------- Validators (report once) -----------------
	bool GetIsValidPlayerResourceManager() const;
	bool GetIsValidEnergyProgressBar() const;
	bool GetIsValidEnergyBarLevel() const;
	bool GetIsValidEnergyInfoWidget() const;

	// ----------------- Non-linear window helpers ----------------
	/** @return Safe, positive window size for index; repeats last when out of range. */
	int32 GetWindowSizeForIndex(const int64 WindowIndex) const;

	/**
	 * @brief Decompose a value into its active window.
	 * @param Value Energy value (>=0).
	 * @param OutWindowStart Sum of all previous window sizes (start energy of the window).
	 * @param OutWindowSize Capacity (energy for full bar) of the active window.
	 * @param OutWithin Amount inside the active window [0..OutWindowSize).
	 */
	void DecomposeIntoWindow(const int32 Value, int64& OutWindowStart, int32& OutWindowSize, int32& OutWithin) const;

	// ----------------- UI helpers -----------------
	void SetEnergyInfoBarVisibility(const bool bVisible) const;
	void UpdateProgressBarUI(const float Fill01, const bool bInDeficit) const;
	void UpdateEnergyLevelUI(const float Level01) const;
	float MapPercentToYOffset(const float Percent01) const;
};
