// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "W_FactionDifficultyPicker.generated.h"

class AFactionPlayerController;

/**
 * @brief Game difficulty picker used during the faction campaign setup flow.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionDifficultyPicker : public UW_GameDifficultyPicker
{
	GENERATED_BODY()

public:
	void SetFactionPlayerController(AFactionPlayerController* FactionPlayerController);

protected:
	virtual void OnDifficultyChosen(const int32 DifficultyPercentage, const ERTSGameDifficulty SelectedDifficulty) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AFactionPlayerController> M_FactionPlayerController;

	bool GetIsValidFactionPlayerController() const;
};
