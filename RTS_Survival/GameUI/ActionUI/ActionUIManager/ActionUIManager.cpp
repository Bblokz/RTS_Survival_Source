#include "ActionUIManager.h"
#include "UObject/Object.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/W_WeaponItem.h"
#include "Components/CanvasPanel.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/ActionUI/ItemActionUI/W_ItemActionUI.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/AmmoButton/W_AmmoPicker.h"
#include "RTS_Survival/GameUI/ActionUI/SelectedUnitInfo/W_SelectedUnitInfo.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/AmmoButton/W_AmmoButton.h"
#include "RTS_Survival/GameUI/ActionUI/SelectedUnitInfo/UnitDescriptionItem/W_SelectedUnitDescription.h"

#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

void UActionUIManager::InitActionUIManager(
	TArray<UW_WeaponItem*> TWeaponUIItemsInMenu,
	UMainGameUI* MainGameUI,
	const TArray<UW_ItemActionUI*>& ActionUIElementsInMenu,
	ACPPController* PlayerController,
	UW_SelectedUnitInfo* SelectedUnitInfo,
	const TObjectPtr<UW_AmmoPicker>& AmmoPicker,
	UW_WeaponDescription* WeaponDescription,
	FActionUIContainer
	ActionUIContainerWidgets,
	UW_SelectedUnitDescription* SelectedUnitDescriptionWidget,
	UUserWidget* ActionUIDescriptionWidget)
{
	if (TWeaponUIItemsInMenu.Num() == 0)
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this,
			"TrainingItemsInMenu.num == 0",
			"UWeaponUIManager::InitWeaponUIManger");
	}
	for (const auto EachWeaponUIElm : TWeaponUIItemsInMenu)
	{
		if (!IsValid(EachWeaponUIElm))
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(
				this,
				"WeaponUIElement",
				"InitWeaponUIManager");
		}
	}
	M_WeaponDescription = WeaponDescription;
	// start with weapon description hidden.
	if (GetIsValidWeaponDescription())
	{
		SetWeaponDescriptionVisibility(false);
	}
	M_TWeaponItems = TWeaponUIItemsInMenu;
	M_ActionUIContainer = ActionUIContainerWidgets;
	for (auto EachWeaponUIElm : M_TWeaponItems)
	{
		if (IsValid(EachWeaponUIElm))
		{
			EachWeaponUIElm->InitWeaponItem(AmmoPicker, WeaponDescription, this);
		}
	}
	if (IsValid(AmmoPicker))
	{
		M_AmmoPicker = AmmoPicker;
		M_AmmoPicker->InitActionUIMngr(this);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this,
		                                                  "AmmoPicker", "UWeaponUIManager::INitWeaponUIManger");
	}
	if (IsValid(MainGameUI))
	{
		M_MainGameUI = MainGameUI;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this,
		                                                  "MainGameUI", "UWeaponUIManager::INitWeaponUIManger");
	}
	if (ActionUIElementsInMenu.Num() == 0)
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this,
		                                                  "ActionUIElements", "InitActionUIManager");
	}
	M_TActionUI_Items = ActionUIElementsInMenu;
	for (int i = 0; i < M_TActionUI_Items.Num(); ++i)
	{
		M_TActionUI_Items[i]->InitActionUIElement(PlayerController, i, this);
	}
	if (!IsValid(SelectedUnitInfo))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this,
		                                                  "SelectedUnitInfo", "InitActionUIManager");
	}
	M_SelectedUnitDescription = SelectedUnitDescriptionWidget;
	M_SelectedUnitInfo = SelectedUnitInfo;
	if (GetIsValidSelectedUnitDescription() && GetIsValidSelectedUnitInfo())
	{
		M_SelectedUnitInfo->InitSelectedUnitInfo(SelectedUnitDescriptionWidget, this);
		SetSelectedUnitDescriptionVisibility(false);
	}
	M_ActionUIDescriptionWidget = ActionUIDescriptionWidget;
	if (GetIsValidActionUIDescriptionWidget())
	{
		SetActionUIDescriptionWidgetVisibility(false);
	}
	M_PlayerController = PlayerController;
}

void UActionUIManager::SetActionUIVisibility(const bool bShowActionUI) const
{
	M_ActionUIContainer.SetActionUIVisibility(bShowActionUI);
}

void UActionUIManager::HideAllHoverInfoWidgets() const
{
	SetWeaponDescriptionVisibility(false);
	SetSelectedUnitDescriptionVisibility(false);
	SetActionUIDescriptionWidgetVisibility(false);
}

void UActionUIManager::HideAmmoPicker() const
{
	if (not GetIsValidAmmoPicker())
	{
		return;
	}
	M_AmmoPicker->SetAmmoPickerVisibility(false);
}

void UActionUIManager::OnHoverActionUIItem(const bool bIsHover) const
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}
	M_MainGameUI->OnHoverActionUIItem(bIsHover);
}

void UActionUIManager::OnHoverSelectedUnitInfo(const bool bIsHover) const
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnHoverSelectedUnitInfo(bIsHover);
}

void UActionUIManager::OnHoverWeaponItem(const bool bIsHover) const
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}
	M_MainGameUI->OnHoverWeaponItem(bIsHover);
}

void UActionUIManager::SetWeaponDescriptionVisibility(const bool bVisible) const
{
	if (not GetIsValidWeaponDescription())
	{
		return;
	}
	ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	M_WeaponDescription->SetVisibility(NewVisibility);
}

bool UActionUIManager::GetIsValidSelectedUnitDescription() const
{
	if (not IsValid(M_SelectedUnitDescription))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "M_SelectedUnitDescription", "GetIsValidSelectedUnitDescription");
		return false;
	}
	return true;
}

void UActionUIManager::SetSelectedUnitDescriptionVisibility(const bool bVisible) const
{
	if (not GetIsValidSelectedUnitDescription())
	{
		return;
	}
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	M_SelectedUnitDescription->SetVisibility(NewVisibility);
}

bool UActionUIManager::GetIsValidActionUIDescriptionWidget() const
{
	if (not IsValid(M_ActionUIDescriptionWidget))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "M_ActionUIDescriptionWidget", "GetIsValidActionUIDescriptionWidget");
		return false;
	}
	return true;
}

void UActionUIManager::SetActionUIDescriptionWidgetVisibility(const bool bVisible) const
{
	if (not GetIsValidActionUIDescriptionWidget())
	{
		return;
	}
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	M_ActionUIDescriptionWidget->SetVisibility(NewVisibility);
}

bool UActionUIManager::GetIsValidAmmoPicker() const
{
	if (not IsValid(M_AmmoPicker))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "M_AmmoPicker", "GetIsValidAmmoPicker");
		return false;
	}
	return true;
}

void UActionUIManager::OnShellTypeSelected(const EWeaponShellType SelectedShellType) const
{
	if (not GetIsValidLastSelectedActor() || not GetIsValidPlayerController())
	{
		return;
	}
	M_PlayerController->PlayVoiceLine(M_LastSelectedActor.Get(), GetVoiceLineForShell(SelectedShellType), false, true);
}

void UActionUIManager::RequestUpdateAbilityUIForPrimary(ICommands* RequestingUnit)
{
        if (not M_PrimarySelectedUnit.IsValid() || not RequestingUnit)
        {
                return;
        }
        if (M_PrimarySelectedUnit.Get() != RequestingUnit)
        {
                RTSFunctionLibrary::ReportError(
                        "Requesting unit is not the primary selected unit."
                        "\n at function UActionUIManager::RequestUpdateAbilityUIForPrimary");
                return;
        }
        UpdateAbilitiesUI(RequestingUnit->GetUnitAbilityEntries());
}

void UActionUIManager::SetWeaponUIVisibility(const bool bVisible)
{
	const ESlateVisibility Visibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;

	for (auto EachWeaponUIElm : M_TWeaponItems)
	{
		if (IsValid(EachWeaponUIElm))
		{
			EachWeaponUIElm->SetVisibility(Visibility);
		}
	}
}

bool UActionUIManager::SetupWeaponUIForSelectedActor(AActor* SelectedActor)
{
	if (not IsValid(SelectedActor))
	{
		SetWeaponUIVisibility(false);
		// On UI reload make sure to hide the ammo picker.
		SetAmmoPickerVisiblity(false);
		return false;
	}
	TArray<UWeaponState*> Weapons;
	M_LastSelectedActor = SelectedActor;
	if (const AAircraftMaster* Aircraft = Cast<AAircraftMaster>(SelectedActor); IsValid(Aircraft))
	{
		UBombComponent* BombComp = nullptr;
		Weapons = GetWeaponsMountedOnAircraft(Aircraft, BombComp);
		return PropagateWeaponDataToUI(Weapons, BombComp);
	}

	if (const ATankMaster* Tank = Cast<ATankMaster>(SelectedActor); IsValid(Tank))
	{
		Weapons = FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
		return PropagateWeaponDataToUI(Weapons);
	}
	if (ASquadController* SquadController = Cast<ASquadController>(SelectedActor); IsValid(SquadController))
	{
		Weapons = GetWeaponsOfSquad(SquadController);
		return PropagateWeaponDataToUI(Weapons);
	}
	if (ABuildingExpansion* BuildingExpansion = Cast<ABuildingExpansion>(SelectedActor); IsValid(BuildingExpansion))
	{
		Weapons = BuildingExpansion->GetAllWeapons();
		return PropagateWeaponDataToUI(Weapons);
	}
	RTSFunctionLibrary::ReportError(
		"Selected actor is not a tank or squad controller or bxp."
		"\n at function UActionUIManager::SetupWeaponUIForSelectedActor");
	return false;
}

bool UActionUIManager::SetUpActionUIForSelectedActor(
        const TArray<FUnitAbilityEntry>& TAbilities,
        const EAllUnitType PrimaryUnitType,
        const ENomadicSubtype NomadicSubtype,
        const ETankSubtype TankSubtype,
        const ESquadSubtype SquadSubtype,
        const EBuildingExpansionType BxpSubtype, AActor* SelectedActor)
{
	RegisterPrimarySelected(SelectedActor);
        if (not UpdateAbilitiesUI(TAbilities))
        {
                return false;
        }
	if (GetIsValidSelectedUnitInfo())
	{
		float MaxHp, CurrentHp;
		float HealthPercentage = SetupHealthComponent(SelectedActor, MaxHp, CurrentHp);
		M_SelectedUnitInfo->SetupUnitInfoForNewUnit(PrimaryUnitType, HealthPercentage, MaxHp, CurrentHp, NomadicSubtype,
		                                            TankSubtype, SquadSubtype, BxpSubtype);
		M_SelectedUnitInfo->SetupUnitDescriptionForNewUnit(SelectedActor,
		                                                   PrimaryUnitType,
		                                                   NomadicSubtype,
		                                                   TankSubtype,
		                                                   SquadSubtype, BxpSubtype);
		SetupExperienceComponent(SelectedActor);
	}
	// Whether there was any non ability EAbility ID to fill in into the UI.
        return ContainsAnyValidAbility(TAbilities);
}

void UActionUIManager::UpdateHealthBar(const float NewPercentage, const float MaxHp, const float CurrentHp) const
{
	if (IsValid(M_SelectedUnitInfo))
	{
		M_SelectedUnitInfo->UpdateHealthBar(NewPercentage, MaxHp, CurrentHp);
	}
}

void UActionUIManager::UpdateExperienceBar(
	const float ExperiencePercentage,
	const int32 CumulativeExp,
	const int32 ExpNeededForNextLevel,
	const int32 CurrentLevel, const int32 MaxLevel,
	const EVeterancyIconSet VeterancyIconSet) const
{
	if (IsValid(M_SelectedUnitInfo))
	{
		M_SelectedUnitInfo->UpdateExperienceBar(ExperiencePercentage, CumulativeExp, ExpNeededForNextLevel,
		                                        CurrentLevel, MaxLevel, VeterancyIconSet);
	}
}

bool UActionUIManager::GetIsValidSelectedUnitInfo() const
{
	if (not IsValid(M_SelectedUnitInfo))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "M_SelectedUnitInfo", "GetIsValidSelectedUnitInfo");
		return false;
	}
	return true;
}

bool UActionUIManager::GetIsValidMainGameUI() const
{
	if (not IsValid(M_MainGameUI))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "M_MainGameUI", "GetIsValidMainGameUI");
		return false;
	}
	return true;
}

bool UActionUIManager::GetIsValidWeaponDescription() const
{
	if (not IsValid(M_WeaponDescription))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "M_WeaponDescription", "GetIsValidWeaponDescription");
		return false;
	}
	return true;
}

bool UActionUIManager::GetIsValidLastSelectedActor() const
{
	if (M_LastSelectedActor.IsValid())
	{
		return true;
	}
	return false;
}

bool UActionUIManager::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_PlayerController", "GetIsValidPlayerController");
	return false;
}

bool UActionUIManager::PropagateWeaponDataToUI(TArray<UWeaponState*> WeaponStates, UBombComponent* BombComponent)
{
	bool bInitializeBombDescription = IsValid(BombComponent);
	int32 ItemsPopulated = 0;
	int32 WeaponStateIndex = 0;
	const int32 TotalWeaponsToDisplay = FMath::Min(WeaponStates.Num() + (bInitializeBombDescription ? 1 : 0),
	                                               M_TWeaponItems.Num());
	for (; ItemsPopulated < TotalWeaponsToDisplay; ++ItemsPopulated)
	{
		if (ItemsPopulated == TotalWeaponsToDisplay)
		{
			break;
		}
		if (IsValid(M_TWeaponItems[ItemsPopulated]))
		{
			if (bInitializeBombDescription)
			{
				M_TWeaponItems[ItemsPopulated]->SetupWidgetForBombComponent(BombComponent);
				M_TWeaponItems[ItemsPopulated]->SetVisibility(ESlateVisibility::Visible);
				// Only one bomb component can be displayed.
				bInitializeBombDescription = false;
				continue;
			}
			UWeaponState* EachWeaponData = WeaponStates[WeaponStateIndex];
			WeaponStateIndex++;
			M_TWeaponItems[ItemsPopulated]->SetupWidgetForWeapon(EachWeaponData);
			M_TWeaponItems[ItemsPopulated]->SetVisibility(ESlateVisibility::Visible);
		}
	}
	for (int i = ItemsPopulated; i < M_TWeaponItems.Num(); ++i)
	{
		if (IsValid(M_TWeaponItems[i]))
		{
			M_TWeaponItems[i]->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	return ItemsPopulated > 0;
}

bool UActionUIManager::ContainsAnyValidAbility(const TArray<FUnitAbilityEntry>& TAbilities) const
{
        for (const FUnitAbilityEntry& AbilityEntry : TAbilities)
        {
                if (AbilityEntry.AbilityId != EAbilityID::IdNoAbility)
                {
                        return true;
                }
        }
        return false;
}

float UActionUIManager::SetupHealthComponent(const AActor* PrimarySelectedActor, float& OutMaxHp, float& OutCurrentHp)
{
	if (IsValid(M_SelectedHealthComponent))
	{
		M_SelectedHealthComponent->SetHealthBarSelected(false, nullptr);
	}
	if (IsValid(PrimarySelectedActor))
	{
		UHealthComponent* HealthComponent = PrimarySelectedActor->FindComponentByClass<UHealthComponent>();
		if (IsValid(HealthComponent))
		{
			M_SelectedHealthComponent = HealthComponent;
			M_SelectedHealthComponent->SetHealthBarSelected(true, this);
			OutMaxHp = HealthComponent->GetMaxHealth();
			OutCurrentHp = HealthComponent->GetCurrentHealth();
			return HealthComponent->GetHealthPercentage();
		}
		return float();
	}
	return float();
}

void UActionUIManager::SetupExperienceComponent(const AActor* PrimarySelectedActor)
{
	// Reset previous registration.
	if (IsValid(M_SelectedExperienceComponent))
	{
		M_SelectedExperienceComponent->SetExperienceBarSelected(false, nullptr);
		M_SelectedExperienceComponent = nullptr;
	}

	if (IsValid(PrimarySelectedActor))
	{
		URTSExperienceComp* ExperienceComponent = PrimarySelectedActor->FindComponentByClass<URTSExperienceComp>();
		if (IsValid(ExperienceComponent))
		{
			M_SelectedExperienceComponent = ExperienceComponent;
			M_SelectedExperienceComponent->SetExperienceBarSelected(true, this);
			// Immediately update the experience UI.
			M_SelectedExperienceComponent->UpdateExperienceUI();
		}
	}
}


void UActionUIManager::SetAmmoPickerVisiblity(const bool bVisible) const
{
	if (IsValid(M_AmmoPicker))
	{
		M_AmmoPicker->SetAmmoPickerVisibility(bVisible);
		return;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_AmmoPicker", "SetAmmoPickerVisiblity");
}




TArray<UWeaponState*> UActionUIManager::GetWeaponsOfSquad(ASquadController* SquadController) const
{
	TArray<UWeaponState*> Weapons;
	if(IsValid(SquadController))
	{
		return SquadController->GetWeaponsOfSquad();
	}
	return Weapons;
}

bool UActionUIManager::UpdateAbilitiesUI(const TArray<FUnitAbilityEntry>& InAbilitiesOfPrimary)
{
        int32 ElmInit = 0;
        for (const FUnitAbilityEntry& AbilityEntry : InAbilitiesOfPrimary)
        {
                if (GetIndexInAbilityItemRange(ElmInit) && IsValid(M_TActionUI_Items[ElmInit]))
                {
                        // Update item with ability.
                        M_TActionUI_Items[ElmInit]->UpdateItemActionUI(AbilityEntry.AbilityId, AbilityEntry.CustomType);
                        ElmInit++;
                }
                else
		{
			RTSFunctionLibrary::ReportError(
				"Index out of bounds: more abilities than action UI items?"
				"\n at function UActionUIManager::SetupActionUIForSelectedActor");
			return false;
		}
	}
	return true;
}

void UActionUIManager::RegisterPrimarySelected(AActor* NewPrimarySelected)
{
	ICommands* NewCommands = Cast<ICommands>(NewPrimarySelected);

	ICommands* OldCommands = M_PrimarySelectedUnit.IsValid() ? M_PrimarySelectedUnit.Get() : nullptr;

	if (OldCommands == NewCommands)
	{
		return;
	}

	if (OldCommands)
	{
		OldCommands->SetPrimarySelected(nullptr);
		M_PrimarySelectedUnit.Reset();
	}

	if (NewCommands)
	{
		M_PrimarySelectedUnit = NewCommands;
		NewCommands->SetPrimarySelected(this);
	}
}


bool UActionUIManager::GetIsCurrentPrimarySelectedValid() const
{
	return M_PrimarySelectedUnit.IsValid();
}
