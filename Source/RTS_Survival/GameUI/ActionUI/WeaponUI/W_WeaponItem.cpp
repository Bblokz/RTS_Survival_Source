#include "W_WeaponItem.h"

#include "AmmoButton/W_AmmoPicker.h"
#include "Components/Border.h"
#include "OnHoverDescription/W_WeaponDescription.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_WeaponItem::InitWeaponItem(UW_AmmoPicker* AmmoPicker, UW_WeaponDescription* WeaponDescription,
                                   UActionUIManager* ActionUIManager)
{
	M_AmmoPicker = AmmoPicker;
	M_W_WeaponDescription = WeaponDescription;
	M_ActionUIManager = ActionUIManager;
}

void UW_WeaponItem::SetupWidgetForWeapon(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "WeaponState",
		                                             "SetupWidgetForWeapon");
		return;
	}
	M_LoadedBombComponent = nullptr;
	M_LoadedWeaponState = WeaponState;
	if (IsValid(M_W_WeaponDescription))
	{
		EWeaponDescriptionType WeaponDescriptionType = M_W_WeaponDescription->OnHoverWeaponItem(WeaponState);
		if (IsValid(WeaponState))
		{
			OnSetupWidgetForWeapon(WeaponState->GetRawWeaponData().WeaponName);
		}
		SetupAmmoIcon(WeaponDescriptionType, WeaponState->GetRawWeaponData().ShellType,
		              WeaponState->GetRawWeaponData().ShellTypes.Num());
	}
}

void UW_WeaponItem::SetupWidgetForBombComponent(UBombComponent* BombComp)
{
	if (not IsValid(BombComp))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "BombComp",
		                                             "SetupWidgetForBombComponent");
		return;
	}
	M_LoadedWeaponState = nullptr;
	M_LoadedBombComponent = BombComp;
	if (IsValid(M_W_WeaponDescription))
	{
		M_W_WeaponDescription->OnHoverBombItem(BombComp);
		OnSetupWidgetForWeapon(BombComp->GetBombBayWeaponData().WeaponName);
	}
}

void UW_WeaponItem::OnNewShellTypeSelected(const EWeaponShellType NewShellType)
{
	if (not IsValid(M_LoadedWeaponState))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "M_LoadedWeaponState",
		                                             "OnAmmoTypeChanged");
	}
	M_LoadedWeaponState->ChangeWeaponShellType(NewShellType);
	OnUpdateAmmoIconForWeapon(NewShellType);
}

void UW_WeaponItem::OnClickedWeaponItem()
{
	if (not IsValid(M_LoadedWeaponState))
	{
		// Fail silently as it is possible this weapon item is set up for a bomb component and thus no weapon state exists.
		return;
	}
	if (IsValid(M_AmmoPicker))
	{
		M_AmmoPicker->SetupAmmoPickerForWeapon(this, M_LoadedWeaponState->GetRawWeaponData().ShellTypes);
		return;
	}
	RTSFunctionLibrary::ReportError("Attempted to load ammo picker for weapon item, but the ammo picker"
		"reference is null."
		"\n At function UW_WeaponItem::OnClickedWeaponItem");
}

void UW_WeaponItem::OnHoverWeaponItem(const bool bIsHover)
{
	if (not EnsureIsValidWeaponDescription() || not GetIsValidActionUIManager())
	{
		return;
	}
	if (bIsHover)
	{
		if (IsValid(M_LoadedBombComponent))
		{
			M_W_WeaponDescription->OnHoverBombItem(M_LoadedBombComponent);
		}
		else
		{
			M_W_WeaponDescription->OnHoverWeaponItem(M_LoadedWeaponState);
		}
	}
	else
	{
		M_W_WeaponDescription->OnUnHover();
	}
	M_ActionUIManager->OnHoverWeaponItem(bIsHover);
}

bool UW_WeaponItem::EnsureIsValidWeaponDescription()
{
	if (not IsValid(M_W_WeaponDescription))
	{
		RTSFunctionLibrary::ReportError("Invalid weapon description for weapon item: " + this->GetName() +
			"\n At function UW_WeaponItem::EnsureIsValidWeaponDescription");
		return false;
	}
	return true;
}

bool UW_WeaponItem::EnsureIsValidAmmoBorder() const
{
	if (not IsValid(AmmoBorder))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this,
		                                                      "AmmoBorder(Border)",
		                                                      "UW_WeaponItem::EnsureIsValidAmmoBorder");
		return false;
	}
	return true;
}

void UW_WeaponItem::SetupAmmoIcon(const EWeaponDescriptionType WeaponDescriptionType,
                                  const EWeaponShellType WeaponShellType, const int32 AmountOfShellsAvailable)
{
	if (not EnsureIsValidAmmoBorder())
	{
		return;
	}
	ESlateVisibility NewVisibility = ESlateVisibility::Collapsed;
	switch (WeaponDescriptionType)
	{
	case EWeaponDescriptionType::Regular:
		if (AmountOfShellsAvailable > 1)
		{
			NewVisibility = ESlateVisibility::Visible;
		}
		break;
	case EWeaponDescriptionType::BombComponent:
		break;
	case EWeaponDescriptionType::FlameWeapon:
		break;
	case EWeaponDescriptionType::LaserWeapon:
		break;
	}
	AmmoBorder->SetVisibility(NewVisibility);
	OnUpdateAmmoIconForWeapon(WeaponShellType);
}

bool UW_WeaponItem::GetIsValidActionUIManager() const
{
	if (not IsValid(M_ActionUIManager))
	{
		RTSFunctionLibrary::ReportError("Action UI Manager reference invalid in UW_WeaponItem");
		return false;
	}
	return true;
}
