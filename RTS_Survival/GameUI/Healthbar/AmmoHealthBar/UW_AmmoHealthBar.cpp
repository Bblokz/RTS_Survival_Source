#include "UW_AmmoHealthBar.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

// -------------------- Public API --------------------

void UW_AmmoHealthBar::SetTrackedWeapon(UWeaponState* WeaponToTrack)
{
	UnbindFromCurrentWeapon();

	M_TrackedWeapon = WeaponToTrack;

	if (not GetIsValidTrackedWeapon(TEXT("SetTrackedWeapon")))
	{
		return;
	}

	M_MagConsumedHandle = M_TrackedWeapon->OnMagConsumed.AddUObject(
		this, &UW_AmmoHealthBar::OnMagConsumed);

	if (IsValid(M_DMI_AmmoIcon))
	{
		// If we already have a DMI, push initial values now
		ApplyInitialDMIParamsFromWeapon();
	}

	Debug(TEXT("SetTrackedWeapon: bound and initialized"), FColor::Green);
}

void UW_AmmoHealthBar::ConfigureAmmoIcon(UMaterialInterface* AmmoIconMaterial, float VerticalSliceUVRatio)
{
	// Avoid magic numbers: keep a local default value here.
	constexpr float DefaultUVRatio = 0.33f;

	if (FMath::IsNearlyZero(VerticalSliceUVRatio) || VerticalSliceUVRatio < 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("Invalid UVRatio supplied; falling back to default."));
		VerticalSliceUVRatio = DefaultUVRatio;
	}
	M_VerticalSliceUVRatio = VerticalSliceUVRatio;

	if (not GetIsValidAmmoIcon(TEXT("ConfigureAmmoIcon")))
	{
		return;
	}
	if (not IsValid(AmmoIconMaterial))
	{
		ReportError(TEXT("Invalid AmmoIconMaterial provided to ConfigureAmmoIcon"));
		return;
	}

	// Create/update the DMI and apply static params
	M_DMI_AmmoIcon = UMaterialInstanceDynamic::Create(AmmoIconMaterial, this);
	AmmoIcon->SetBrushFromMaterial(M_DMI_AmmoIcon);

	SetDMIParam_VerticalUVRatio(M_VerticalSliceUVRatio);

	if (IsValid(M_TrackedWeapon))
	{
		// If we already know the weapon, initialize capacity/current amounts now
		ApplyInitialDMIParamsFromWeapon();
	}
	Debug(TEXT("ConfigureAmmoIcon: DMI created and parameters applied"), FColor::Green);
}

// -------------------- Blueprint init --------------------

void UW_AmmoHealthBar::InitAmmoHealthBar(FHealthBarDMIAmmoParameter AmmoDMIParams)
{
	M_AmmoDMIParams = AmmoDMIParams;
}

// -------------------- UUserWidget lifecycle --------------------

void UW_AmmoHealthBar::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(M_TrackedWeapon) && not M_MagConsumedHandle.IsValid())
	{
		M_MagConsumedHandle = M_TrackedWeapon->OnMagConsumed.AddUObject(
			this, &UW_AmmoHealthBar::OnMagConsumed);
	}
}

void UW_AmmoHealthBar::NativeDestruct()
{
	UnbindFromCurrentWeapon();

	Super::NativeDestruct();
}

// -------------------- Validation helpers --------------------

bool UW_AmmoHealthBar::GetIsValidTrackedWeapon(const TCHAR* CallingFunction) const
{
	if (IsValid(M_TrackedWeapon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, TEXT("M_TrackedWeapon"), CallingFunction, nullptr);
	return false;
}

bool UW_AmmoHealthBar::GetIsValidAmmoIcon(const TCHAR* CallingFunction) const
{
	if (IsValid(AmmoIcon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, TEXT("AmmoIcon"), CallingFunction, nullptr);
	return false;
}

bool UW_AmmoHealthBar::GetIsValidAmmoDMI(const TCHAR* CallingFunction) const
{
	if (IsValid(M_DMI_AmmoIcon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, TEXT("M_DMI_AmmoIcon"), CallingFunction, nullptr);
	return false;
}

// -------------------- Internal helpers --------------------

void UW_AmmoHealthBar::ApplyInitialDMIParamsFromWeapon() const
{
	if (not GetIsValidAmmoDMI(TEXT("ApplyInitialDMIParamsFromWeapon")))
	{
		return;
	}
	if (not GetIsValidTrackedWeapon(TEXT("ApplyInitialDMIParamsFromWeapon")))
	{
		return;
	}

	const int32 MagCapacity = M_TrackedWeapon->GetRawWeaponData().MagCapacity;
	SetDMIParam_MaxCapacity(MagCapacity);
	SetDMIParam_CurrentMagAmount(MagCapacity);
	SetDMIParam_VerticalUVRatio(M_VerticalSliceUVRatio);
}

void UW_AmmoHealthBar::UnbindFromCurrentWeapon()
{
	if (IsValid(M_TrackedWeapon) && M_MagConsumedHandle.IsValid())
	{
		M_TrackedWeapon->OnMagConsumed.Remove(M_MagConsumedHandle);
		M_MagConsumedHandle.Reset();
	}
	M_TrackedWeapon = nullptr;
}

// -------------------- Delegate sink --------------------

void UW_AmmoHealthBar::OnMagConsumed(const int32 BulletsLeft)
{
	SetDMIParam_CurrentMagAmount(BulletsLeft);
}

// -------------------- DMI setters --------------------

void UW_AmmoHealthBar::SetDMIParam_MaxCapacity(const int32 MaxMagCapacity) const
{
	if (not GetIsValidAmmoDMI(TEXT("SetDMIParam_MaxCapacity")))
	{
		return;
	}
	M_DMI_AmmoIcon->SetScalarParameterValue(M_AmmoDMIParams.MaxMagCapacity, MaxMagCapacity);
}

void UW_AmmoHealthBar::SetDMIParam_CurrentMagAmount(const int32 CurrentAmount) const
{
	if (not GetIsValidAmmoDMI(TEXT("SetDMIParam_CurrentMagAmount")))
	{
		return;
	}
	M_DMI_AmmoIcon->SetScalarParameterValue(M_AmmoDMIParams.CurrentMagAmount, CurrentAmount);
}

void UW_AmmoHealthBar::SetDMIParam_VerticalUVRatio(const float Ratio) const
{
	if (not GetIsValidAmmoDMI(TEXT("SetDMIParam_VerticalUVRatio")))
	{
		return;
	}
	M_DMI_AmmoIcon->SetScalarParameterValue(M_AmmoDMIParams.VerticalSliceUVRatio, Ratio);
}

// -------------------- Logging --------------------

void UW_AmmoHealthBar::ReportError(const FString& ErrorMessage) const
{
	const FString OwnerName = IsValid(GetOuter()) ? GetOuter()->GetName() : TEXT("Unknown Owner");
	RTSFunctionLibrary::ReportError(ErrorMessage + TEXT("\n")
		+ TEXT(" On W_AmmoHealthBar: ") + GetName()
		+ TEXT("\nWith owner: ") + OwnerName);
}

void UW_AmmoHealthBar::Debug(const FString& DebugMessage, const FColor Color) const
{
	if constexpr (DeveloperSettings::Debugging::GAmmoTracking_Compile_DebugSymbols)
	{
		const FString OwnerName = IsValid(GetOuter()) ? GetOuter()->GetName() : TEXT("Unknown Owner");
		const FString WeaponName = IsValid(M_TrackedWeapon) ? M_TrackedWeapon->GetName() : TEXT("No Weapon Set");
		RTSFunctionLibrary::PrintString(DebugMessage + TEXT("\n") + OwnerName + TEXT("\n") + WeaponName, Color);
	}
}
