// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "MissionTrainingRestrictionSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMissionTrainingRestriction, Log, All);

/**
 * @brief Lets a mission (tutorials) limit what the player may train: while a restriction is active only allow-listed
 * options can be queued and the training menu shows every other option as locked by the mission.
 * Missions drive it through UMissionBase::RestrictPlayerTrainingTo / AllowPlayerTrainingOptions /
 * ClearPlayerTrainingRestriction; it ends with the world.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionTrainingRestrictionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Only these options stay trainable for the player until allowed further or cleared. */
	void RestrictTo(const TArray<FTrainingOption>& AllowedOptions);

	/** @brief Adds options to an active restriction; does nothing while nothing is restricted. */
	void AllowOptions(const TArray<FTrainingOption>& OptionsToAllow);

	void ClearRestriction();

	/** @return True when the player may train the option (no restriction, or it is allow-listed). */
	bool GetIsOptionAllowed(const FTrainingOption& Option) const;

	bool GetIsRestricted() const { return bM_IsRestricted; }

private:
	bool bM_IsRestricted = false;

	// Only consulted while bM_IsRestricted.
	TSet<FTrainingOption> M_AllowedOptions;

	// The training menu builds its item states on selection and refresh; refresh the player's trainers so an
	// open menu shows the change at once.
	void RefreshPlayerTrainers() const;

	void LogRestriction(const TCHAR* Reason) const;
};
