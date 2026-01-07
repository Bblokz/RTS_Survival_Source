// Copyright (C) Bas Blokzijl

#include "UW_BombHealthBar.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"

// -------------------- Public API --------------------

void UW_BombHealthBar::SetTrackedBombComponent(UBombComponent* BombComponent)
{
	UnbindFromCurrentBombComp();

	if (not IsValid(BombComponent))
	{
		ReportErrorBomb(TEXT("SetTrackedBombComponent: Invalid BombComponent."));
		return;
	}

	M_TrackedBombComponent = BombComponent;
	M_BombMagHandle = M_TrackedBombComponent->OnMagConsumed.AddUObject(
		this, &UW_BombHealthBar::OnBombsChanged);

	// We do not know capacity yet; it will be provided on the next broadcast.
	// ConfigureAmmoIcon (from base) should have created the DMI already; nothing else to push now.
	DebugBomb(TEXT("SetTrackedBombComponent: bound to OnMagConsumed"), FColor::Green);
}

// -------------------- UUserWidget lifecycle --------------------

void UW_BombHealthBar::NativeConstruct()
{
	Super::NativeConstruct();

	// Rebind if the widget rebuilt while tracking a valid bomb component.
	if (M_TrackedBombComponent.IsValid() && not M_BombMagHandle.IsValid())
	{
		M_BombMagHandle = M_TrackedBombComponent->OnMagConsumed.AddUObject(
			this, &UW_BombHealthBar::OnBombsChanged);
	}
}

void UW_BombHealthBar::NativeDestruct()
{
	UnbindFromCurrentBombComp();

	Super::NativeDestruct();
}

// -------------------- Validation --------------------

bool UW_BombHealthBar::GetIsValidTrackedBombComp(const TCHAR* CallingFunction) const
{
	if (M_TrackedBombComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, TEXT("M_TrackedBombComponent"), CallingFunction, nullptr);
	return false;
}

// -------------------- Internal helpers --------------------

void UW_BombHealthBar::UnbindFromCurrentBombComp()
{
	if (M_TrackedBombComponent.IsValid() && M_BombMagHandle.IsValid())
	{
		M_TrackedBombComponent->OnMagConsumed.Remove(M_BombMagHandle);
		M_BombMagHandle.Reset();
	}
	M_TrackedBombComponent = nullptr;
	// Do not reset M_MaxCapacityCached; we keep last known capacity until a new source provides a value.
}

// -------------------- Delegate sink --------------------

void UW_BombHealthBar::OnBombsChanged(const int32 BombsLeft)
{
	// Make sure our DMI exists before writing.
	if (not GetIsValidAmmoDMI(TEXT("OnBombsChanged")))
	{
		return;
	}

	// Lazy capacity discovery:
	// - ForceReloadAllBombsInstantly and OnInit_SetupAllBombEntries broadcast full capacity.
	// - If we bind mid-mission, we may see a smaller number first; we keep the max observed as capacity.
	if (M_MaxCapacityCached == INDEX_NONE || BombsLeft > M_MaxCapacityCached)
	{
		M_MaxCapacityCached = BombsLeft;
		SetDMIParam_MaxCapacity(M_MaxCapacityCached);
	}

	SetDMIParam_CurrentMagAmount(BombsLeft);
	// Ratio is static; ConfigureAmmoIcon already set it. No need to spam SetDMIParam_VerticalUVRatio here.

	DebugBomb(FString::Printf(TEXT("OnBombsChanged: bombs=%d cap=%d"), BombsLeft, M_MaxCapacityCached), FColor::Silver);
}

// -------------------- Logging --------------------

void UW_BombHealthBar::ReportErrorBomb(const FString& ErrorMessage) const
{
	const FString OwnerName = IsValid(GetOuter()) ? GetOuter()->GetName() : TEXT("Unknown Owner");
	RTSFunctionLibrary::ReportError(ErrorMessage + TEXT("\n")
		+ TEXT(" On UW_BombHealthBar: ") + GetName()
		+ TEXT("\nWith owner: ") + OwnerName);
}

void UW_BombHealthBar::DebugBomb(const FString& DebugMessage, const FColor Color) const
{
	if constexpr (DeveloperSettings::Debugging::GAmmoTracking_Compile_DebugSymbols)
	{
		const FString OwnerName = IsValid(GetOuter()) ? GetOuter()->GetName() : TEXT("Unknown Owner");
		const FString SourceName = M_TrackedBombComponent.IsValid() ? M_TrackedBombComponent->GetName() : TEXT("No BombComp");
		RTSFunctionLibrary::PrintString(DebugMessage + TEXT("\n") + OwnerName + TEXT("\n") + SourceName, Color);
	}
}
