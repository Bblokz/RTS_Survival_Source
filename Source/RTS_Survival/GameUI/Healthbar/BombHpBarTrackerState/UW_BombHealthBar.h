// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Healthbar/AmmoHealthBar/UW_AmmoHealthBar.h"
#include "UW_BombHealthBar.generated.h"

class UBombComponent;

/**
 * @brief Bomb icon health bar that tracks an aircraft's bombs via UBombComponent.
 *        Order-independent: safe to set widget material first or bomb component first.
 *        Uses the same DMI pipeline as UW_AmmoHealthBar.
 */
UCLASS()
class RTS_SURVIVAL_API UW_BombHealthBar : public UW_AmmoHealthBar
{
	GENERATED_BODY()

public:
	/**
	 * @brief Assign the bomb component to track; binds delegate and initializes values lazily.
	 * @param BombComponent Bomb component to track (weakly referenced).
	 */
	void SetTrackedBombComponent(UBombComponent* BombComponent);

protected:
	// UUserWidget lifecycle
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// ---- Tracked source ----
	/** Weak ref so UI does not keep gameplay component alive. */
	TWeakObjectPtr<UBombComponent> M_TrackedBombComponent = nullptr;

	/** Handle to unbind the delegate cleanly. */
	FDelegateHandle M_BombMagHandle;

	/** Cached capacity (bomb slots). Determined on first delegate call (reload broadcasts full count). */
	int32 M_MaxCapacityCached = INDEX_NONE;

	// ---- Validation ----
	bool GetIsValidTrackedBombComp(const TCHAR* CallingFunction) const;

	// ---- Internal helpers ----
	void UnbindFromCurrentBombComp();

	// ---- Delegate sink for bombs left ----
	void OnBombsChanged(const int32 BombsLeft);

	// ---- Logging (local to this subclass) ----
	void ReportErrorBomb(const FString& ErrorMessage) const;
	void DebugBomb(const FString& DebugMessage, const FColor Color) const;
};
