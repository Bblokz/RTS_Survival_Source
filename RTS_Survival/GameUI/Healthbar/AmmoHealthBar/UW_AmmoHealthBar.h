#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Healthbar/W_HealthBar.h"
#include "UW_AmmoHealthBar.generated.h"

class UImage;
class UWeaponState;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * @brief Ammo icon health bar that tracks a weapon's magazine amount.
 *        The widget owns binding/unbinding to the weapon and survives widget rebuilds.
 * @note InitAmmoHealthBar: call in Blueprint to set the parameter names for the DMI.
 */
UCLASS()
class RTS_SURVIVAL_API UW_AmmoHealthBar : public UW_HealthBar
{
	GENERATED_BODY()

public:
	/**
	 * @brief Assign the weapon to track; binds delegates and initializes values when possible.
	 * @param WeaponToTrack Weapon state to track (weakly referenced).
	 */
	void SetTrackedWeapon(UWeaponState* WeaponToTrack);

	/**
	 * @brief Configure the ammo icon material and vertical UV slice ratio.
	 * @param AmmoIconMaterial Material used for the ammo icon (DMI will be created).
	 * @param VerticalSliceUVRatio 0..1 range; falls back to default if invalid.
	 */
	void ConfigureAmmoIcon(UMaterialInterface* AmmoIconMaterial, float VerticalSliceUVRatio);

protected:
	// --- Expose DMI pipeline to subclasses (bomb tracker uses the same UI material) ---
	// Contains the parameter names needed to affect which bullets of the ammo icon will be displayed.
	FHealthBarDMIAmmoParameter M_AmmoDMIParams;

	/** Runtime DMI for the ammo icon. */
	UPROPERTY()
	UMaterialInstanceDynamic* M_DMI_AmmoIcon = nullptr;

	/** Cached vertical slice ratio; reused across rebuilds. */
	float M_VerticalSliceUVRatio = 0.33f;

	/** Parameter names for the DMI; set from Blueprint once. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitAmmoHealthBar(FHealthBarDMIAmmoParameter AmmoDMIParams);

	/** Bound in the widget blueprint. */
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UImage* AmmoIcon = nullptr;

	// UUserWidget lifecycle
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * @brief Validate the ammo icon widget reference.
	 * @param CallingFunction Name of the calling function for error reporting.
	 * @return true if valid, false otherwise (also logs).
	 */
	bool GetIsValidAmmoIcon(const TCHAR* CallingFunction) const;

	/**
	 * @brief Validate the ammo icon DMI reference.
	 * @param CallingFunction Name of the calling function for error reporting.
	 * @return true if valid, false otherwise (also logs).
	 */
	bool GetIsValidAmmoDMI(const TCHAR* CallingFunction) const;

	// ---- DMI setters ----
	void SetDMIParam_MaxCapacity(const int32 MaxMagCapacity) const;
	void SetDMIParam_CurrentMagAmount(const int32 CurrentAmount) const;
	void SetDMIParam_VerticalUVRatio(const float Ratio) const;

private:
	UPROPERTY()
	/** Weak ref so UI does not keep weapon alive. */
	UWeaponState* M_TrackedWeapon;

	/** Handle to unbind the delegate cleanly. */
	FDelegateHandle M_MagConsumedHandle;

	// ---- Validation (centralized error reporting via ReportErrorVariableNotInitialised) ----
	/**
	 * @brief Validate tracked weapon pointer (weak).
	 * @param CallingFunction Name of the calling function for error reporting.
	 * @return true if valid, false otherwise (also logs).
	 */
	bool GetIsValidTrackedWeapon(const TCHAR* CallingFunction) const;


	// ---- Internal helpers ----
	void ApplyInitialDMIParamsFromWeapon() const;
	void UnbindFromCurrentWeapon();

	// ---- Delegate sink ----
	void OnMagConsumed(const int32 BulletsLeft);

	// ---- Logging ----
	void ReportError(const FString& ErrorMessage) const;
	void Debug(const FString& DebugMessage, const FColor Color) const;
};
