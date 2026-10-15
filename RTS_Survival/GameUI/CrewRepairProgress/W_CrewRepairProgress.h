// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_CrewRepairProgress.generated.h"

class UProgressBar;

/**
 * @brief Bar with a crew repair icon that a tank shows above its health bar while the crew repairs modules.
 * The tank syncs the remaining work once per repair tick; the widget fills smoothly in between.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CrewRepairProgress : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Re-anchors the fill on the tank's own bookkeeping, which can jump when other healers or new damage
	 * change the remaining work.
	 * @param RemainingSeconds Crew repair work left on all modules.
	 * @param TotalSeconds Work of the whole repair session, used as the full bar.
	 */
	void SyncRepairProgress(const float RemainingSeconds, const float TotalSeconds);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Bound automatically to the UMG widget by name.
	UPROPERTY(meta = (BindWidget), meta=(AllowPrivateAccess="true"), BlueprintReadOnly)
	TObjectPtr<UProgressBar> M_ProgressBar = nullptr;

	float M_RemainingSecondsAtSync = 0.f;
	float M_TotalSeconds = 0.f;
	double M_SyncWorldTime = 0.0;

	void ApplyProgressAtTime(const double WorldTimeSeconds) const;
	bool GetIsValidProgressBar() const;
};
