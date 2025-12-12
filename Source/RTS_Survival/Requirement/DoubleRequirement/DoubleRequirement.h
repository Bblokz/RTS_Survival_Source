// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "DoubleRequirement.generated.h"

class UTrainerComponent;
class URTSRequirement;
struct FTrainingOption;
enum class ETechnology : uint8;

/**
 * @brief Composite requirement that is met only if BOTH child requirements are met.
 *
 * UI:
 * - Requirement text shows unmet parts as: "<A> AND <B>".
 * - The widget can show two icons: one per unmet child (no priority heuristic).
 * - We cache per-child unmet kind + unit/tech so the UI can render both.
 */
UCLASS()
class RTS_SURVIVAL_API UDoubleRequirement : public URTSRequirement
{
	GENERATED_BODY()

public:
	UDoubleRequirement();

	/**
	 * @brief Initialize with two child requirements. The children are duplicated
	 *        into this object as Outer to ensure proper ownership & snapshot duplication.
	 * @param First  The first requirement (Unit/Tech/Expansion/Pad/etc.)
	 * @param Second The second requirement (Unit/Tech/Expansion/Pad/etc.)
	 */
	UFUNCTION()
	void InitDoubleRequirement(URTSRequirement* First, URTSRequirement* Second);

	/** @return true only if BOTH child requirements are met. */
	virtual bool CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem) override;

	/** Per-child status (widget uses these for two icons). */
	virtual ETrainingItemStatus GetTrainingStatusWhenRequirementNotMet() const override;
	virtual ETrainingItemStatus GetSecondaryRequirementStatus() const override;

	/** "Primary" (first child) missing unit/tech after last check (if any). */
	virtual FTrainingOption GetRequiredUnit() const override { return M_RequiredUnit1; }
	virtual ETechnology    GetRequiredTechnology() const override { return M_RequiredTech1; }

	/** Secondary accessors for UI composition. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Requirement")
	FTrainingOption GetSecondaryRequiredUnit() const { return M_RequiredUnit2; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Requirement")
	ETechnology GetSecondaryRequiredTech() const { return M_RequiredTech2; }

	bool IsFirstRequirementMet() const { return M_bFirstRequirementMet; }
	bool IsSecondRequirementMet() const { return M_bSecondRequirementMet; }

private:
	// Stored child requirements (owned so snapshots duplicate fully).
	UPROPERTY(VisibleAnywhere, Instanced)
	TObjectPtr<URTSRequirement> M_First;

	UPROPERTY(VisibleAnywhere, Instanced)
	TObjectPtr<URTSRequirement> M_Second;

	// Per-child cached presentation state from the last failed check.
	UPROPERTY()
	FTrainingOption M_RequiredUnit1;             // child 1
	UPROPERTY()
	ETechnology    M_RequiredTech1;              // child 1
	UPROPERTY()
	FTrainingOption M_RequiredUnit2;    // child 2
	UPROPERTY()
	ETechnology    M_RequiredTech2;     // child 2

	// What kind of requirement is unmet per child (no priority).
	UPROPERTY()
	ERTSRequirement M_FirstRequirementType = ERTSRequirement::Requirement_None;

	UPROPERTY()
	ERTSRequirement M_SecondRequirementType = ERTSRequirement::Requirement_None;

	// Was the second requirement met last time we checked?
	// We store only this bool as if it was met then the first requirement must not be met.
	// if both are met the UI does not use this bool (no requirement widget).
	bool M_bSecondRequirementMet = false;
	bool M_bFirstRequirementMet = false;


	// Cache unmet kinds + required unit/tech per child (no icon priority).
	void UpdatePresentationFromChildren(
		const bool bFirstMet, const bool bSecondMet,
		UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem);
};
