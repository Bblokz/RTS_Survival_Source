#include "HealthComponent.h"

#include "RTSComponent.h"
#include "SelectionComponent.h"
#include "Components/WidgetComponent.h"
#include "HealthInterface/HealthBarOwner.h"
#include "HealthInterface/HealthLevels/HealthLevels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/GameUI/Healthbar/W_HealthBar.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

// Define the static member
TMap<int32, EHealthLevel> UHealthComponent::M_PercentageToHealthLevel;
TMap<EHealthLevel, int32> UHealthComponent::M_HealthLevelToThresholdValue;

UHealthComponent::UHealthComponent()
	: CurrentHealth(100),
	  MaxHealth(100),
	  M_HealthLevel(EHealthLevel::Level_100Percent)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	InitializeHealthLevelMap();
}

void UHealthComponent::MakeHealthBarInvisible() const
{
	if (Widget_GetIsValidHealthBarWidget())
	{
		M_HealthBarWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}


void UHealthComponent::OnHideAllGameUI(const bool bHide)
{
	// Always record the global state, even if the widget isn't created yet.
	bWasHiddenByAllGameUI = bHide;

	if (!M_HealthBarWidget.IsValid())
	{
		return;
	}

	if (bHide)
	{
		M_HealthBarWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// Unhide: recompute from policy and clear the flag.
	bWasHiddenByAllGameUI = false;
	ApplyHealthBarVisibilityPolicy(GetHealthPercentage());
}

void UHealthComponent::HideHealthBar()
{
	if (Widget_GetIsValidWidgetComponent())
	{
		M_HealthBarWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UHealthComponent::DebugHealthComponentAtLocation(const FVector Location) const
{
	FString DebugMessage = "MaxHealth / CurrentHealth = " + FString::SanitizeFloat(MaxHealth) + " / " +
		FString::SanitizeFloat(CurrentHealth);
	DebugMessage += "\n " + M_FireThresholdState.GetDebugString();
	DebugMessage += "Dmg reduction mlt: " + FString::SanitizeFloat(DamageReductionSettings.DamageReductionMlt) +
		" Dmg reduction flat: " + FString::SanitizeFloat(DamageReductionSettings.DamageReduction);
	DebugMessage += "\n Current Health Level : " + UEnum::GetValueAsString(M_HealthLevel);
	RTSFunctionLibrary::PrintString(Location, this, DebugMessage, FColor::Green, 25.f);
}

void UHealthComponent::SetHealthBarSelected(const bool bSetSelected, TObjectPtr<UActionUIManager> ActionUIManager)
{
	if (bSetSelected)
	{
		if (IsValid(ActionUIManager))
		{
			M_ActionUIManager = ActionUIManager;
			return;
		}
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this,
			"ActionUIManager",
			"UHealthComponent::SetHealthBarSelected");
		return;
	}
	// Reset the action UI manager.
	M_ActionUIManager = nullptr;
}

FVector UHealthComponent::GetLocalLocation() const
{
	if (not Widget_GetIsValidWidgetComponent())
	{
		return FVector::ZeroVector;
	}
	return M_OwnerHpWidgetComp->GetRelativeLocation();
}

void UHealthComponent::SetLocalLocation(const FVector& NewLocation) const
{
	if (not Widget_GetIsValidHealthBarWidget())
	{
		return;
	}
	M_OwnerHpWidgetComp->SetRelativeLocation(RelativeWidgetOffset);
}

UWidgetComponent* UHealthComponent::GetHealthBarWidgetComp() const
{
	if (M_OwnerHpWidgetComp.IsValid())
	{
		return M_OwnerHpWidgetComp.Get();
	}
	return nullptr;
}

void UHealthComponent::ChangeTargetIconType(const ETargetTypeIcon NewIconType)
{
	if (not Widget_GetIsValidHealthBarWidget())
	{
		return;
	}
	int32 OwningPLayer = 0;

	if (M_RTSComponent.IsValid())
	{
		OwningPLayer = M_RTSComponent->GetOwningPlayer();
	}
	M_HealthBarWidget->ChangeTargetTypeIcon(NewIconType, OwningPLayer);
}

bool UHealthComponent::TakeDamage(float& InOutDamage, const ERTSDamageType DamageType)
{
	// Apply reductions
	InOutDamage *= DamageReductionSettings.DamageReductionMlt;
	InOutDamage -= DamageReductionSettings.DamageReduction;

	if (InOutDamage <= 0.f)
	{
		InOutDamage = 0.f;
		return false;
	}

	const bool bIsFireDamage = DamageType == ERTSDamageType::Fire;
	if (bIsFireDamage && CanTolerateFireDamage(InOutDamage))
	{
		// Fire did not reach threshold to cause health damage.
		InOutDamage = 0.f;
		OnUnitInCombat();
		return false;
	}

	const float PreviousHealth = CurrentHealth;
	const float AppliedDamage = FMath::Min(InOutDamage, PreviousHealth);

	CurrentHealth = FMath::Clamp(PreviousHealth - AppliedDamage, 0.f, MaxHealth);
	InOutDamage = AppliedDamage;

	OnUnitInCombat();
	UpdateHealthBar();
	return CurrentHealth <= 0.f;
}

void UHealthComponent::SetCurrentHealth(const float NewCurrentHealth)
{
	CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.f, MaxHealth);
	UpdateHealthBar();
}

void UHealthComponent::SetMaxHealth(const float NewMaxHealth)
{
	const float ClampedNewMaxHealth = FMath::Max(0.f, NewMaxHealth);

	float NewCurrentHealth = ClampedNewMaxHealth;
	if (MaxHealth > 0.f)
	{
		const float OldPercentage = FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f);
		NewCurrentHealth = ClampedNewMaxHealth * OldPercentage;
	}

	MaxHealth = ClampedNewMaxHealth;
	CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.f, MaxHealth);

	if (M_HealthBarWidget.IsValid())
	{
		M_HealthBarWidget->UpdateMaxHealth(MaxHealth);
	}

	UpdateHealthBar();
}

bool UHealthComponent::Heal(const float HealAmount)
{
	bool bIsFullHealth = false;
	if (CurrentHealth + HealAmount >= MaxHealth)
	{
		CurrentHealth = MaxHealth;
		bIsFullHealth = true;
	}
	else
	{
		CurrentHealth += HealAmount;
		if (GetHealthPercentage() >= 0.99)
		{
			CurrentHealth = MaxHealth;
			bIsFullHealth = true;
		}
	}
	UpdateHealthBar();
	return bIsFullHealth;
}

bool UHealthComponent::GetHasDamageToRepair() const
{
	return CurrentHealth < MaxHealth;
}


void UHealthComponent::InitHealthAndResistance(const FResistanceAndDamageReductionData& ResistanceData,
                                               const float NewMaxHp)
{
	InitFireThresholdData(ResistanceData.FireResistanceData.MaxFireThreshold,
	                      ResistanceData.FireResistanceData.FireRecoveryPerSecond);
	InitDamageReductionSettings(ResistanceData.AllDamageReductionSettings);
	ChangeTargetIconType(ResistanceData.TargetTypeIcon);
	SetMaxHealth(NewMaxHp);
}

void UHealthComponent::ChangeVisibilitySettings(const FHealthBarVisibilitySettings& NewSettings)
{
	VisibilitySettings = NewSettings;
	UpdateSelectionComponentBindings();
	UpdateVisibilityAfterSettingsChange();
	UpdateHealthBar();
}

FHealthBarVisibilitySettings UHealthComponent::GetVisibilitySettings() const
{
	return VisibilitySettings;
}

void UHealthComponent::OnOverwiteHealthbarVisiblityPlayer(const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	if (Strategy == ERTSPlayerHealthBarVisibilityStrategy::NotInitialized ||
		Strategy == ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults)
	{
		RestoreUnitDefaultHealthBarVisibilitySettings();
		return;
	}

	FHealthBarVisibilitySettings NewSettings = VisibilitySettings;
	switch (Strategy)
	{
	case ERTSPlayerHealthBarVisibilityStrategy::AwaysVisible:
		NewSettings.bAlwaysDisplay = true;
		NewSettings.bDisplayOnSelected = false;
		NewSettings.bDisplayOnDamaged = false;
		break;
	case ERTSPlayerHealthBarVisibilityStrategy::VisibleWhenDamagedOnly:
		NewSettings.bAlwaysDisplay = false;
		NewSettings.bDisplayOnSelected = false;
		NewSettings.bDisplayOnDamaged = true;
		break;
	case ERTSPlayerHealthBarVisibilityStrategy::VisibleOnSelectionAndDamaged:
		NewSettings.bAlwaysDisplay = false;
		NewSettings.bDisplayOnSelected = true;
		NewSettings.bDisplayOnDamaged = true;
		break;
	case ERTSPlayerHealthBarVisibilityStrategy::VisibleOnSelectionOnly:
		NewSettings.bAlwaysDisplay = false;
		NewSettings.bDisplayOnSelected = true;
		NewSettings.bDisplayOnDamaged = false;
		break;
	case ERTSPlayerHealthBarVisibilityStrategy::NeverVisible:
		NewSettings.bAlwaysDisplay = false;
		NewSettings.bDisplayOnSelected = false;
		NewSettings.bDisplayOnDamaged = false;
		NewSettings.bDisplayOnHover = false;
		break;
	default:
		return;
	}

	VisibilitySettings = NewSettings;
	UpdateSelectionComponentBindings();
	UpdateVisibilityAfterSettingsChange();
}

void UHealthComponent::OnOverwiteHealthbarVisiblityEnemy(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	if (Strategy == ERTSEnemyHealthBarVisibilityStrategy::NotInitialized ||
		Strategy == ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults)
	{
		RestoreUnitDefaultHealthBarVisibilitySettings();
		return;
	}

	FHealthBarVisibilitySettings NewSettings = VisibilitySettings;
	switch (Strategy)
	{
	case ERTSEnemyHealthBarVisibilityStrategy::AwaysVisible:
		NewSettings.bAlwaysDisplay = true;
		NewSettings.bDisplayOnSelected = false;
		NewSettings.bDisplayOnDamaged = false;
		break;
	case ERTSEnemyHealthBarVisibilityStrategy::VisibleWhenDamagedOnly:
		NewSettings.bAlwaysDisplay = false;
		NewSettings.bDisplayOnSelected = false;
		NewSettings.bDisplayOnDamaged = true;
		break;
	default:
		return;
	}

	VisibilitySettings = NewSettings;
	UpdateSelectionComponentBindings();
	UpdateVisibilityAfterSettingsChange();
}

UW_HealthBar* UHealthComponent::GetHealthBarWidget() const
{
	if (M_HealthBarWidget.IsValid())
	{
		return M_HealthBarWidget.Get();
	}
	return nullptr;
}

void UHealthComponent::InitFireThresholdData(const float NewMaxFireThreshold, const float NewFireRecovery)
{
	// Preserve percentage fill of the buffer
	const float OldMax = M_FireThresholdState.MaxFireThreshold;
	const float OldCur = M_FireThresholdState.CurrentFireThreshold;
	M_FireThresholdState.FireRecoveryPerSec = FMath::Max(0.f, NewFireRecovery);
	M_FireThresholdState.MaxFireThreshold = FMath::Max(0.f, NewMaxFireThreshold);

	if (OldMax > 0.f)
	{
		const float Pct = FMath::Clamp(OldCur / OldMax, 0.f, 1.f);
		M_FireThresholdState.CurrentFireThreshold = Pct * M_FireThresholdState.MaxFireThreshold;
	}
	else
	{
		M_FireThresholdState.CurrentFireThreshold = 0.f;
	}

	if (Widget_GetIsValidHealthBarWidget())
	{
		M_HealthBarWidget->UpdateMaxFireThreshold(M_FireThresholdState.MaxFireThreshold);
		// Also push the current % to keep visuals in sync
		const float Ratio = (M_FireThresholdState.MaxFireThreshold > 0.f)
			                    ? (M_FireThresholdState.CurrentFireThreshold / M_FireThresholdState.MaxFireThreshold)
			                    : 0.f;
		M_HealthBarWidget->OnFireDamage(Ratio);
	}
}

void UHealthComponent::InitTargetTypeIconData(const ETargetTypeIcon NewIconType)
{
	M_TargetTypeIcon = NewIconType;
	ChangeTargetIconType(NewIconType);
}

void UHealthComponent::InitDamageReductionSettings(const FDamageReductionSettings& NewSettings)
{
	DamageReductionSettings = NewSettings;
}


// Called when the game starts
void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	M_HealthBarWidgetCallbacks.SetOwningHealthComponent(this);

	BeginPlay_CheckDamageReductionSettings();
	BeginPlay_SetupAssociatedRTSComponent();
	M_UnitDefaultHealthBarVisibilitySettings = VisibilitySettings;
	BeginPlay_ApplyUserSettingsHealthBarVisibility();
	BeginPlay_BindHoverToSelectionComponent();

	M_OwnerActor = GetOwner();
	if (not IsValid(M_OwnerActor))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this,
			"OwnerActor",
			"UHealthComponent::BeginPlay");
	}
	M_IHealthOwner.SetObject(GetOwner());
	M_IHealthOwner.SetInterface(Cast<IHealthBarOwner>(GetOwner()));
	// error handling.
	(void)GetIsValidHeathBarOwner();
	// Fail silently as some actors may not use a health widget.
	if (HealthBarWidgetClass)
	{
		Widget_CreateHealthBar();
		OnWidgetInitialized();
	}
}

void UHealthComponent::BeginPlay_ApplyUserSettingsHealthBarVisibility()
{
	UGameUnitManager* const GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RestoreUnitDefaultHealthBarVisibilitySettings();
		return;
	}

	if (not GameUnitManager->TryApplyHealthBarVisibilityFromUserSettings(this))
	{
		RestoreUnitDefaultHealthBarVisibilitySettings();
	}
}


void UHealthComponent::Widget_CreateHealthBar()
{
	if (not Widget_GetIsValidWidgetClass() || not GetOwner())
	{
		return;
	}
	AActor* Owner = GetOwner();
	M_HealthBarWidget = Cast<UW_HealthBar>(CreateWidget(GetWorld(), HealthBarWidgetClass));
	if (not Widget_GetIsValidHealthBarWidget())
	{
		return;
	}
	M_HealthBarWidget->SetRenderScale(WidgetXYScales);
	UWidgetComponent* HpBarWidgetComp = NewObject<UWidgetComponent>(
		Owner,
		UWidgetComponent::StaticClass(),
		NAME_None,
		RF_Transactional
	);
	if (!IsValid(HpBarWidgetComp))
	{
		Widget_OnFailedToCreateWidgetComponent();
		return;
	}
	HpBarWidgetComp->SetupAttachment(
		Owner->GetRootComponent()
	);
	HpBarWidgetComp->SetRelativeLocation(RelativeWidgetOffset);

	HpBarWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	HpBarWidgetComp->SetDrawAtDesiredSize(true);
	HpBarWidgetComp->SetWidget(M_HealthBarWidget.Get());
	HpBarWidgetComp->RegisterComponent();

	M_OwnerHpWidgetComp = HpBarWidgetComp;
	// Notify registered callbacks that the widget is ready.
	M_HealthBarWidgetCallbacks.OnHealthBarWidgetReady.Broadcast(this);
}

void UHealthComponent::OnWidgetInitialized()
{
	if (not Widget_GetIsValidHealthBarWidget() || not Widget_GetIsValidWidgetComponent())
	{
		return;
	}

	int8 OwningPlayer = 0;
	if (M_RTSComponent.IsValid())
	{
		OwningPlayer = M_RTSComponent->GetOwningPlayer();
		RegisterCallBackForUnitName(M_RTSComponent.Get());
	}
	M_HealthBarWidget->SetSettingsFromHealthComponent(this, CustomizationSettings, OwningPlayer);

	M_OwnerHpWidgetComp->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	M_OwnerHpWidgetComp->SetCanEverAffectNavigation(false);

	// Respect global hide at init time, otherwise fall back to policy.
	ApplyHealthBarVisibilityPolicy(1.f);
}


bool UHealthComponent::CanTolerateFireDamage(const float Damage)
{
	const float PrevThreshold = M_FireThresholdState.CurrentFireThreshold;
	// Apply incoming “heat” into the buffer, clamp to the cap
	const float NewThreshold = FMath::Min(PrevThreshold + Damage, M_FireThresholdState.MaxFireThreshold);
	M_FireThresholdState.CurrentFireThreshold = NewThreshold;

	// Start recovery ticking if this is the first time we moved above 0
	if (PrevThreshold <= 0.f && M_FireThresholdState.CurrentFireThreshold > 0.f)
	{
		StartFireRecoveryIfNeeded();
	}

	// Drive the UI: fill 0..1 as threshold accumulates
	if (M_HealthBarWidget.IsValid())
	{
		const float Ratio = (M_FireThresholdState.MaxFireThreshold > 0.f)
			                    ? (M_FireThresholdState.CurrentFireThreshold / M_FireThresholdState.MaxFireThreshold)
			                    : 0.f;
		M_HealthBarWidget->OnFireDamage(Ratio);
	}

	// If we reached (or exceeded by clamp) the cap, caller should apply REAL damage as well.
	const bool bCanStillTolerate = (M_FireThresholdState.CurrentFireThreshold < M_FireThresholdState.MaxFireThreshold);
	return bCanStillTolerate;
}

void UHealthComponent::Widget_OnFailedToCreateWidgetComponent() const
{
	const FString Name = GetName();
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NullPtr";
	RTSFunctionLibrary::ReportError("Failed to create HealthBarWidget on component: " + Name +
		"\n Of owner: " + OwnerName);
}

bool UHealthComponent::Widget_GetIsValidHealthBarWidget() const
{
	if (M_HealthBarWidget.IsValid())
	{
		return true;
	}
	const FString Name = GetName();
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NullPtr";
	RTSFunctionLibrary::ReportError("Invalid M_HealthBarWidget on component: " + Name +
		"\n Of owner: " + OwnerName);
	return false;
}

bool UHealthComponent::Widget_GetIsValidWidgetComponent() const
{
	if (M_OwnerHpWidgetComp.IsValid())
	{
		return true;
	}
	const FString Name = GetName();
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NullPtr";
	RTSFunctionLibrary::ReportError("Invalid M_OwnerHpWidgetComp on component: " + Name +
		"\n Of owner: " + OwnerName +
		"\n Likely cause: failed to create the Healthbar widget and therefore the "
		"widget component that holds it in screen space on the owner.");
	return false;
}

bool UHealthComponent::Widget_GetIsValidWidgetClass() const
{
	if (IsValid(HealthBarWidgetClass))
	{
		return true;
	}
	const FString Name = GetName();
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NullPtr";
	RTSFunctionLibrary::ReportError("Invalid HealthBarWidgetClass on component: " + Name +
		"\n Of owner: " + OwnerName);
	return false;
}

void UHealthComponent::UpdateHealthBar()
{
	const float Percentage = (MaxHealth > 0.f) ? (CurrentHealth / MaxHealth) : 0.f;
	NotifyHealthLevelChange(Percentage);

	// Fail silently as some actors may not use health widgets.
	if (not HealthBarWidgetClass)
	{
		return;
	}

	if (IsValid(M_ActionUIManager))
	{
		M_ActionUIManager->UpdateHealthBar(Percentage, MaxHealth, CurrentHealth);
	}

	if (Widget_GetIsValidHealthBarWidget())
	{
		M_HealthBarWidget->OnDamaged(Percentage);
		UpdateVisibilityOnHealthChange(Percentage);
	}
}

void UHealthComponent::UpdateVisibilityOnHealthChange(const float NewPercentage) const
{
	ApplyHealthBarVisibilityPolicy(NewPercentage);
}

bool UHealthComponent::ShouldDisplayHealthForPercentage(const float NewPercentage) const
{
	if (VisibilitySettings.bAlwaysDisplay)
	{
		if constexpr (DeveloperSettings::Debugging::GHealthComponent_Compile_DebugSymbols)
		{
			Debug("Display HP bar as always display is set.", FColor::Green);
		}
		return true;
	}
	if (VisibilitySettings.bDisplayOnDamaged && NewPercentage < 1.0f)
	{
		if constexpr (DeveloperSettings::Debugging::GHealthComponent_Compile_DebugSymbols)
		{
			Debug("Display HP bar as display on damaged!.", FColor::Green);
		}
		return true;
	}
	return false;
}

void UHealthComponent::NotifyHealthLevelChange(const float Percentage)
{
	if (HealthLevelsToNotifyOn.IsEmpty())
	{
		return;
	}

	const EHealthLevel CurrentComputedLevel = CalculateCurrentComputedLevel(Percentage);

	// If the computed level hasn't changed, do nothing.
	if (CurrentComputedLevel == M_HealthLevel)
	{
		return;
	}

	// Check if health is decreasing (damage) or increasing (healing)
	if (IsHealthDecreasing(CurrentComputedLevel))
	{
		EHealthLevel OvershotLevel;
		if (FindOvershotNotifyLevel(M_HealthLevel, CurrentComputedLevel, OvershotLevel))
		{
			M_HealthLevel = OvershotLevel;
			NotifyOwner(OvershotLevel, false);
		}
	}
	else // Health is increasing (healing)
	{
		if (HealthLevelsToNotifyOn.Contains(CurrentComputedLevel))
		{
			M_HealthLevel = CurrentComputedLevel;
			NotifyOwner(CurrentComputedLevel, true);
		}
	}
}

EHealthLevel UHealthComponent::CalculateCurrentComputedLevel(const float Percentage) const
{
	const int32 RoundedPercentage = FMath::Clamp(FMath::RoundToInt(Percentage * 100.0f), 0, 100);
	return M_PercentageToHealthLevel[RoundedPercentage];
}

bool UHealthComponent::IsHealthDecreasing(const EHealthLevel NewComputedLevel) const
{
	return GetThresholdValue(NewComputedLevel) < GetThresholdValue(M_HealthLevel);
}

bool UHealthComponent::FindOvershotNotifyLevel(const EHealthLevel PreviousLevel, const EHealthLevel NewComputedLevel,
                                               EHealthLevel& OutOvershotLevel) const
{
	bool bFound = false;
	const int32 PreviousThreshold = GetThresholdValue(PreviousLevel);
	const int32 NewThreshold = GetThresholdValue(NewComputedLevel);

	// Initialize with the new computed level.
	OutOvershotLevel = NewComputedLevel;

	// Iterate over the configured notify levels.
	for (EHealthLevel Level : HealthLevelsToNotifyOn)
	{
		const int32 LevelThreshold = GetThresholdValue(Level);
		if (LevelThreshold < PreviousThreshold && LevelThreshold >= NewThreshold)
		{
			if (!bFound || LevelThreshold > GetThresholdValue(OutOvershotLevel))
			{
				OutOvershotLevel = Level;
				bFound = true;
			}
		}
	}

	return bFound;
}

void UHealthComponent::NotifyOwner(const EHealthLevel NewLevel, const bool bIsHealing)
{
	if (GetIsValidHeathBarOwner())
	{
		M_IHealthOwner->OnHealthChanged(NewLevel, bIsHealing);
	}
}

void UHealthComponent::Debug(const FString& Message, const FColor& Color) const
{
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NullPtr";
	RTSFunctionLibrary::PrintString(Message +
		"\n Of owner: " + OwnerName);
}

void UHealthComponent::ApplyHealthBarVisibilityPolicy(const float CurrentPct) const
{
	if (!M_HealthBarWidget.IsValid())
	{
		return;
	}

	if (bWasHiddenByAllGameUI)
	{
		M_HealthBarWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	const ESlateVisibility Target =
		ShouldDisplayHealthForPercentage(CurrentPct)
			? ESlateVisibility::Visible
			: ESlateVisibility::Hidden;

	M_HealthBarWidget->SetVisibility(Target);
}


void UHealthComponent::StartFireRecoveryIfNeeded()
{
	if (!GetWorld())
	{
		return;
	}

	// Don’t reset/adjust the timer if it’s already running 
	if (GetWorld()->GetTimerManager().IsTimerActive(M_FireRecoveryTimerHandle))
	{
		return;
	}

	// 1 second cadence, repeating, no initial delay change
	GetWorld()->GetTimerManager().SetTimer(
		M_FireRecoveryTimerHandle,
		this,
		&UHealthComponent::OnFireRecoveryTick,
		1.0f,
		true
	);
}


void UHealthComponent::OnFireRecoveryTick()
{
	// Reduce “heat” by a fixed amount per second.
	M_FireThresholdState.CurrentFireThreshold = FMath::Max(
		0.f, M_FireThresholdState.CurrentFireThreshold - M_FireThresholdState.FireRecoveryPerSec);

	if (M_HealthBarWidget.IsValid())
	{
		const float Ratio = (M_FireThresholdState.MaxFireThreshold > 0.f)
			                    ? (M_FireThresholdState.CurrentFireThreshold / M_FireThresholdState.MaxFireThreshold)
			                    : 0.f;
		M_HealthBarWidget->OnFireRecovery(Ratio);
	}

	// Stop when fully recovered
	if (M_FireThresholdState.CurrentFireThreshold <= 0.f)
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(M_FireRecoveryTimerHandle);
		}
	}
}


void UHealthComponent::BeginPlay_CheckDamageReductionSettings()
{
	if (DamageReductionSettings.DamageReduction < 0)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "DamageReduction",
		                                                      "UHealthComponent::BeginPlay_CheckDamageReductionSettings");
		DamageReductionSettings.DamageReduction = 0;
	}
	if (DamageReductionSettings.DamageReductionMlt <= 0)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "DamageReductionMlt",
		                                                      "UHealthComponent::BeginPlay_CheckDamageReductionSettings");
		DamageReductionSettings.DamageReductionMlt = 1;
	}
}


void UHealthComponent::BeginPlay_SetupAssociatedRTSComponent()
{
	if (AActor* Owner = GetOwner(); not IsValid(Owner))
	{
		return;
	}
	if (URTSComponent* RTSComponent = Cast<
		URTSComponent>(GetOwner()->GetComponentByClass(URTSComponent::StaticClass())))
	{
		M_RTSComponent = RTSComponent;
	}
}

void UHealthComponent::BeginPlay_BindHoverToSelectionComponent()
{
	if (AActor* Owner = GetOwner(); not IsValid(Owner))
	{
		return;
	}
	if (USelectionComponent* SelectionComponent = Cast<
		USelectionComponent>(GetOwner()->GetComponentByClass(USelectionComponent::StaticClass())))
	{
		M_SelectionComponent = SelectionComponent;
		UpdateSelectionComponentBindings();
	}
}

void UHealthComponent::UpdateSelectionComponentBindings()
{
	AActor* const Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	if (not HealthBarWidgetClass)
	{
		return;
	}

	USelectionComponent* SelectionComponent = M_SelectionComponent.Get();
	if (not IsValid(SelectionComponent))
	{
		SelectionComponent = Cast<USelectionComponent>(Owner->GetComponentByClass(USelectionComponent::StaticClass()));
		if (not IsValid(SelectionComponent))
		{
			return;
		}
		M_SelectionComponent = SelectionComponent;
	}

	ClearSelectionComponentBindings();

	TWeakObjectPtr<UHealthComponent> WeakThis(this);
	if (VisibilitySettings.bDisplayOnHover)
	{
		auto HoverLambda = [WeakThis]()-> void
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnUnitHovered();
			}
		};
		auto UnHoverLambda = [WeakThis]()-> void
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnUnitUnhovered();
			}
		};
		M_SelectionDelegateHandles.M_OnUnitHoveredHandle = SelectionComponent->OnUnitHovered.AddLambda(HoverLambda);
		M_SelectionDelegateHandles.M_OnUnitUnhoveredHandle = SelectionComponent->OnUnitUnHovered.AddLambda(UnHoverLambda);
	}

	if (VisibilitySettings.bDisplayOnSelected)
	{
		auto SelectedLambda = [WeakThis]()-> void
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnUnitSelected();
			}
		};
		auto DeselectedLambda = [WeakThis]()-> void
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnUnitDeselected();
			}
		};
		M_SelectionDelegateHandles.M_OnUnitSelectedHandle = SelectionComponent->OnUnitSelected.AddLambda(SelectedLambda);
		M_SelectionDelegateHandles.M_OnUnitDeselectedHandle = SelectionComponent->OnUnitDeselected.AddLambda(DeselectedLambda);
	}
}

void UHealthComponent::ClearSelectionComponentBindings()
{
	USelectionComponent* const SelectionComponent = M_SelectionComponent.Get();
	if (not IsValid(SelectionComponent))
	{
		return;
	}

	if (M_SelectionDelegateHandles.M_OnUnitHoveredHandle.IsValid())
	{
		SelectionComponent->OnUnitHovered.Remove(M_SelectionDelegateHandles.M_OnUnitHoveredHandle);
		M_SelectionDelegateHandles.M_OnUnitHoveredHandle.Reset();
	}

	if (M_SelectionDelegateHandles.M_OnUnitUnhoveredHandle.IsValid())
	{
		SelectionComponent->OnUnitUnHovered.Remove(M_SelectionDelegateHandles.M_OnUnitUnhoveredHandle);
		M_SelectionDelegateHandles.M_OnUnitUnhoveredHandle.Reset();
	}

	if (M_SelectionDelegateHandles.M_OnUnitSelectedHandle.IsValid())
	{
		SelectionComponent->OnUnitSelected.Remove(M_SelectionDelegateHandles.M_OnUnitSelectedHandle);
		M_SelectionDelegateHandles.M_OnUnitSelectedHandle.Reset();
	}

	if (M_SelectionDelegateHandles.M_OnUnitDeselectedHandle.IsValid())
	{
		SelectionComponent->OnUnitDeselected.Remove(M_SelectionDelegateHandles.M_OnUnitDeselectedHandle);
		M_SelectionDelegateHandles.M_OnUnitDeselectedHandle.Reset();
	}
}

void UHealthComponent::UpdateVisibilityAfterSettingsChange() const
{
	if (not HealthBarWidgetClass)
	{
		return;
	}

	if (not Widget_GetIsValidHealthBarWidget())
	{
		return;
	}

	if (bWasHiddenByAllGameUI)
	{
		M_HealthBarWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	if (VisibilitySettings.bDisplayOnSelected)
	{
		const USelectionComponent* const SelectionComponent = M_SelectionComponent.Get();
		if (IsValid(SelectionComponent) && SelectionComponent->GetIsSelected())
		{
			M_HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
			return;
		}
	}

	ApplyHealthBarVisibilityPolicy(GetHealthPercentage());
}

void UHealthComponent::RestoreUnitDefaultHealthBarVisibilitySettings()
{
	VisibilitySettings = M_UnitDefaultHealthBarVisibilitySettings;
	UpdateSelectionComponentBindings();
	UpdateVisibilityAfterSettingsChange();
}

void UHealthComponent::RegisterCallBackForUnitName(URTSComponent* RTSComponent)
{
	TWeakObjectPtr<UHealthComponent> WeakThis(this);
	auto OnUnitName = [WeakThis]()-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		if (WeakThis->M_RTSComponent.IsValid() && WeakThis->Widget_GetIsValidHealthBarWidget())
		{
			bool bIsValidString = false;
			WeakThis->M_HealthBarWidget->SetupUnitName(WeakThis->M_RTSComponent->GetDisplayName(bIsValidString));
		}
	};
	M_RTSComponent->OnSubTypeInitialized.AddLambda(OnUnitName);
}


void UHealthComponent::OnUnitHovered() const
{
	if (not VisibilitySettings.bDisplayOnHover)
	{
		return;
	}
	if (not Widget_GetIsValidHealthBarWidget() || bWasHiddenByAllGameUI)
	{
		return;
	}
	M_HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
}

void UHealthComponent::OnUnitUnhovered() const
{
	if (not VisibilitySettings.bDisplayOnHover)
	{
		return;
	}
	if (not Widget_GetIsValidHealthBarWidget())
	{
		return;
	}
	ApplyHealthBarVisibilityPolicy(GetHealthPercentage());
}

void UHealthComponent::OnUnitSelected() const
{
	if (not VisibilitySettings.bDisplayOnSelected)
	{
		return;
	}

	if (not Widget_GetIsValidHealthBarWidget() || bWasHiddenByAllGameUI)
	{
		return;
	}

	M_HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
}

void UHealthComponent::OnUnitDeselected() const
{
	if (not VisibilitySettings.bDisplayOnSelected)
	{
		return;
	}

	if (not Widget_GetIsValidHealthBarWidget())
	{
		return;
	}

	ApplyHealthBarVisibilityPolicy(GetHealthPercentage());
}


void UHealthComponent::OnUnitInCombat() const
{
	if (not M_RTSComponent.IsValid())
	{
		// This health component has no associated RTS component.
		return;
	}
	M_RTSComponent->SetUnitInCombat(true);
}

bool UHealthComponent::GetIsValidHeathBarOwner() const
{
	if (!IsValid(M_IHealthOwner.GetObject()) || !M_IHealthOwner.GetInterface())
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this, "IHealthBarOwner",
			"UHealthComponent::GetIsValidHeathBarOwner");
		return false;
	}
	return true;
}

void UHealthComponent::InitializeHealthLevelMap()
{
	// Ensure that the map is initialized only once.
	if (M_PercentageToHealthLevel.Num() == 0)
	{
		for (int32 i = 0; i <= 100; ++i)
		{
			if (i > 75)
			{
				M_PercentageToHealthLevel.Add(i, EHealthLevel::Level_100Percent);
			}
			else if (i > 66)
			{
				M_PercentageToHealthLevel.Add(i, EHealthLevel::Level_75Percent);
			}
			else if (i > 50)
			{
				M_PercentageToHealthLevel.Add(i, EHealthLevel::Level_66Percent);
			}
			else if (i > 33)
			{
				M_PercentageToHealthLevel.Add(i, EHealthLevel::Level_50Percent);
			}
			else if (i > 25)
			{
				M_PercentageToHealthLevel.Add(i, EHealthLevel::Level_33Percent);
			}
			else
			{
				M_PercentageToHealthLevel.Add(i, EHealthLevel::Level_25Percent);
			}
		}
	}
	if (M_HealthLevelToThresholdValue.IsEmpty())
	{
		M_HealthLevelToThresholdValue.Add(EHealthLevel::Level_100Percent, 100);
		M_HealthLevelToThresholdValue.Add(EHealthLevel::Level_75Percent, 75);
		M_HealthLevelToThresholdValue.Add(EHealthLevel::Level_66Percent, 66);
		M_HealthLevelToThresholdValue.Add(EHealthLevel::Level_50Percent, 50);
		M_HealthLevelToThresholdValue.Add(EHealthLevel::Level_33Percent, 33);
		M_HealthLevelToThresholdValue.Add(EHealthLevel::Level_25Percent, 25);
		M_HealthLevelToThresholdValue.Add(EHealthLevel::Level_NoAction, 0);
	}
}

int32 UHealthComponent::GetThresholdValue(const EHealthLevel Level) const
{
	return M_HealthLevelToThresholdValue[Level];
}
