
// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "OrDoubleRequirement.generated.h"

class UTrainerComponent;
class URTSRequirement;
struct FTrainingOption;
enum class ETechnology : uint8;

/**
 * @brief Composite requirement that is met if EITHER child requirement is met.
 *
 * UI when NOT met (both children unmet):
 *  - Requirement text shows: "<A> OR <B>" (only the unmet parts are listed, which is both here).
 *  - Two icons can be shown by the widget: one for each child requirement that is unmet.
 *  - We cache the per-child missing kinds (no priority ordering).
 */
UCLASS()
class RTS_SURVIVAL_API UOrDoubleRequirement : public URTSRequirement
{
	GENERATED_BODY()

public:
	UOrDoubleRequirement();

	/** Duplicate+own children so snapshots clone the full tree. */
	UFUNCTION()
	void InitOrRequirement(URTSRequirement* First, URTSRequirement* Second);

	/** @return true if at least ONE child requirement is met. */
	virtual bool CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem) override;

	/** Per-child status (widget uses these for two icons). */
	virtual ETrainingItemStatus GetTrainingStatusWhenRequirementNotMet() const override;
	virtual ETrainingItemStatus GetSecondaryRequirementStatus() const override;

	/** Primary missing unit/tech (only meaningful if the OR is currently unmet). */
	virtual FTrainingOption GetRequiredUnit() const override { return M_RequiredUnit1; }
	virtual ETechnology    GetRequiredTechnology() const override { return M_RequiredTech1; }

	/** Secondary accessors so the widget can display both unmet parts. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Requirement")
	FTrainingOption GetSecondaryRequiredUnit() const { return M_RequiredUnit2; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Requirement")
	ETechnology GetSecondaryRequiredTech() const { return M_RequiredTech2; }

	bool IsFirstRequirementMet() const { return M_bFirstRequirementMet; }
	bool IsSecondRequirementMet() const { return M_bSecondRequirementMet; }

private:
	/** Owned children (so DuplicateObject on this will deep-copy the graph). */
	UPROPERTY()
	TObjectPtr<URTSRequirement> M_First;

	UPROPERTY()
	TObjectPtr<URTSRequirement> M_Second;

	/** Cached presentation state from the last failed (both-unmet) check. */
	UPROPERTY()
	FTrainingOption M_RequiredUnit1;

	UPROPERTY()
	ETechnology M_RequiredTech1;

	UPROPERTY()
	FTrainingOption M_RequiredUnit2;

	UPROPERTY()
	ETechnology M_RequiredTech2;

	/** What kind of requirement is unmet per child (no priority). */
	UPROPERTY()
	ERTSRequirement M_FirstRequirementType = ERTSRequirement::Requirement_None;

	UPROPERTY()
	ERTSRequirement M_SecondRequirementType = ERTSRequirement::Requirement_None;

	
	// Was the second requirement met last time we checked?
	// We store only this bool as if it was met then the first requirement must not be met.
	// if both are met the UI does not use this bool (no requirement widget).
	bool M_bSecondRequirementMet = false;
	bool M_bFirstRequirementMet = false;

	/**
	 * Cache unmet kinds and required unit/tech per child (no icon priority).
	 */
	void UpdatePresentationFromChildren(
		const bool bFirstMet, const bool bSecondMet,
		UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem);
};
