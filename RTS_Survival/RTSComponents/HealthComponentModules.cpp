// Copyright (C) Bas Blokzijl - All rights reserved.

#include "HealthComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "RTS_Survival/GameUI/Healthbar/W_HealthBar.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace HealthComponentModuleConstants
{
	// Re-entrant heals merged into deferred work are applied in at most this many follow-up transactions.
	constexpr int32 MaxDeferredHealingTransactions = 8;
	const FName ModuleBoxWidgetName = TEXT("ModuleBox");
}

// ----------------------------------------------------------------------------------------------------
// Healing transaction
// ----------------------------------------------------------------------------------------------------

bool UHealthComponent::ApplyHealingInternal(const float HealAmount)
{
	if (not M_TankRepairOwner.IsValid())
	{
		return ApplyHullOnlyHealing(HealAmount);
	}
	if (bM_IsApplyingHealingTransaction)
	{
		// Deferred until the current health/module transaction and its notifications finish.
		if (FMath::IsFinite(HealAmount) && HealAmount > 0.f)
		{
			M_DeferredHealingWork += HealAmount;
		}
		return GetIsFullyRepairedLivingOwner();
	}

	bool bIsFullyRepaired = ApplyTankHealingTransaction(HealAmount);
	for (int32 DeferredIndex = 0; DeferredIndex < HealthComponentModuleConstants::MaxDeferredHealingTransactions
	     && M_DeferredHealingWork > 0.f; ++DeferredIndex)
	{
		const float DeferredHealingWork = M_DeferredHealingWork;
		M_DeferredHealingWork = 0.f;
		bIsFullyRepaired = ApplyTankHealingTransaction(DeferredHealingWork);
	}
	return bIsFullyRepaired;
}

bool UHealthComponent::ApplyTankHealingTransaction(const float HealAmount)
{
	// Dead owners, rejected, zero and negative healing produce no receipt.
	if (CurrentHealth <= 0.f || not FMath::IsFinite(HealAmount) || HealAmount <= 0.f || MaxHealth <= 0.f)
	{
		return GetIsFullyRepairedLivingOwner();
	}

	TGuardValue<bool> TransactionGuard(bM_IsApplyingHealingTransaction, true);
	const float PreviousHealth = CurrentHealth;
	// Clamp to MaxHealth (no 99% snap) so work is never counted as both hull healing and finishing work.
	const float AppliedHullHealing = FMath::Min(HealAmount, FMath::Max(0.f, MaxHealth - CurrentHealth));
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + AppliedHullHealing);

	if (M_TankNonHealthyModuleCount > 0 && M_TankRepairOwner.IsValid())
	{
		FHealthHealingReceipt Receipt;
		Receipt.AcceptedHealingWork = HealAmount;
		Receipt.AppliedHullHealing = AppliedHullHealing;
		Receipt.HealthAfter = CurrentHealth;
		Receipt.MaxHealth = MaxHealth;
		// Sent even when no hull HP was missing; the tank decides the single module milestone for this heal.
		M_TankRepairOwner->OnHealthHealingApplied(Receipt);
	}

	if (CurrentHealth != PreviousHealth)
	{
		UpdateHealthBar();
	}
	return GetIsFullyRepairedLivingOwner();
}

bool UHealthComponent::GetIsFullyRepairedLivingOwner() const
{
	return CurrentHealth > 0.f && not GetHasDamageToRepair();
}

void UHealthComponent::InitializeTankRepairOwner(ATankMaster* Tank)
{
	if (not IsValid(Tank) || Tank != GetOwner())
	{
		RTSFunctionLibrary::ReportError(TEXT("InitializeTankRepairOwner: the tank must own this health component. ")
			+ GetNameSafe(GetOwner()));
		return;
	}
	M_TankRepairOwner = Tank;
}

void UHealthComponent::SetTankPendingModuleRepairCount(const int32 NonHealthyModuleCount)
{
	M_TankNonHealthyModuleCount = FMath::Max(0, NonHealthyModuleCount);
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Widget_ReleaseModuleIcons();
	M_OnMaxHealthChanged.Clear();
	M_TankRepairOwner.Reset();
	M_DeferredHealingWork = 0.f;
	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------------------------------------
// Tank module icons
// ----------------------------------------------------------------------------------------------------

void UHealthComponent::InitializeTankModulePresentation(ATankMaster* Tank)
{
	if (not IsValid(Tank) || Tank != GetOwner())
	{
		RTSFunctionLibrary::ReportError(TEXT("InitializeTankModulePresentation: only the owning tank can enable")
			TEXT(" module icons. Owner: ") + GetNameSafe(GetOwner()));
		return;
	}
	// Dedicated servers keep module state but skip presentation entirely.
	if (GetNetMode() == NM_DedicatedServer || M_ModuleIconState.bIsPresentationEnabled)
	{
		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	M_VehicleModuleSubsystem = IsValid(GameInstance) ? GameInstance->GetSubsystem<UVehicleModuleSubsystem>() : nullptr;
	M_ModuleIconState.bIsPresentationEnabled = true;
	if (bM_IsHealthBarWidgetInitialized)
	{
		Widget_OnModulePresentationReady();
	}
}

bool UHealthComponent::GetIsValidVehicleModuleSubsystem() const
{
	if (M_VehicleModuleSubsystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_VehicleModuleSubsystem",
		"GetIsValidVehicleModuleSubsystem",
		this
	);
	return false;
}

void UHealthComponent::ApplyModuleIconStateChanges(const FModuleIconDeltaBatch& Changes)
{
	if (Changes.IsEmpty())
	{
		return;
	}
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		if ((Changes.ChangedTypeMask & (1u << TypeIndex)) != 0)
		{
			SetDesiredModuleIconState(TypeIndex, Changes.IconStates.States[TypeIndex]);
		}
	}
	OnDesiredModuleIconsChanged();
}

void UHealthComponent::SynchronizeModuleIconSnapshot(const FModuleIconStates& Snapshot)
{
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		SetDesiredModuleIconState(TypeIndex, Snapshot.States[TypeIndex]);
	}
	OnDesiredModuleIconsChanged();
}

void UHealthComponent::SetDesiredModuleIconState(const int32 TypeIndex, const EVehicleModuleState State)
{
	// Duplicate states are suppressed here so the widget only sees actual changes.
	if (M_ModuleIconState.DesiredStates[TypeIndex] == State)
	{
		return;
	}
	M_ModuleIconState.DesiredStates[TypeIndex] = State;
	M_ModuleIconState.DirtyTypeMask |= 1u << TypeIndex;
}

void UHealthComponent::OnDesiredModuleIconsChanged()
{
	bool bHasAnyNonHealthyModule = false;
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		bHasAnyNonHealthyModule |= M_ModuleIconState.DesiredStates[TypeIndex] != EVehicleModuleState::Healthy;
	}
	if (bHasAnyNonHealthyModule != M_ModuleIconState.bHasAnyNonHealthyModule)
	{
		// Re-evaluate bDisplayOnDamaged only when aggregate module visibility changes; keeps selection rules.
		M_ModuleIconState.bHasAnyNonHealthyModule = bHasAnyNonHealthyModule;
		UpdateVisibilityAfterSettingsChange();
	}
	if (GetShouldFlushModuleIcons())
	{
		Widget_FlushModuleIconChanges();
	}
}

bool UHealthComponent::GetIsVisibleSlateVisibility(const ESlateVisibility Visibility)
{
	return Visibility == ESlateVisibility::Visible || Visibility == ESlateVisibility::HitTestInvisible
		|| Visibility == ESlateVisibility::SelfHitTestInvisible;
}

bool UHealthComponent::GetShouldFlushModuleIcons() const
{
	if (not M_ModuleIconState.bIsPresentationEnabled || M_ModuleIconState.bIsModuleBoxUnavailable
		|| M_ModuleIconState.DirtyTypeMask == 0 || not bM_IsHealthBarWidgetInitialized)
	{
		return false;
	}
	const UW_HealthBar* HealthBarWidget = M_HealthBarWidget.Get();
	return IsValid(HealthBarWidget) && GetIsVisibleSlateVisibility(HealthBarWidget->GetVisibility());
}

void UHealthComponent::Widget_OnModulePresentationReady()
{
	// A new widget instance has no icon children yet; replay the cached desired snapshot into it.
	M_ModuleIconState.bIsModuleBoxUnavailable = false;
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		M_ModuleIconState.DisplayedStates[TypeIndex] = EVehicleModuleState::Healthy;
		if (M_ModuleIconState.DesiredStates[TypeIndex] != EVehicleModuleState::Healthy)
		{
			M_ModuleIconState.DirtyTypeMask |= 1u << TypeIndex;
		}
	}
	if (not Widget_FindAndCacheModuleBox())
	{
		M_ModuleIconState.bIsModuleBoxUnavailable = true;
		return;
	}
	if (GetShouldFlushModuleIcons())
	{
		Widget_FlushModuleIconChanges();
	}
}

bool UHealthComponent::Widget_FindAndCacheModuleBox()
{
	if (not Widget_GetIsValidHealthBarWidget())
	{
		return false;
	}
	M_ModuleBox = Cast<UHorizontalBox>(
		M_HealthBarWidget->GetWidgetFromName(HealthComponentModuleConstants::ModuleBoxWidgetName));
	return Widget_GetIsValidModuleBox();
}

bool UHealthComponent::Widget_GetIsValidModuleBox() const
{
	if (M_ModuleBox.IsValid())
	{
		return true;
	}
	const FString WidgetClassName = M_HealthBarWidget.IsValid() ? M_HealthBarWidget->GetClass()->GetName() : "None";
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"ModuleBox (UHorizontalBox named ModuleBox in tank healthbar widget " + WidgetClassName + ")",
		"Widget_GetIsValidModuleBox",
		this
	);
	return false;
}

void UHealthComponent::Widget_ReleaseModuleIcons() const
{
	UW_HealthBar* HealthBarWidget = M_HealthBarWidget.Get();
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->ReleaseModuleIconImages();
	}
	M_ModuleBox.Reset();
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		M_ModuleIconState.DisplayedStates[TypeIndex] = EVehicleModuleState::Healthy;
	}
}

void UHealthComponent::Widget_FlushModuleIconChanges() const
{
	if (not GetShouldFlushModuleIcons() || not M_VehicleModuleSubsystem.IsValid())
	{
		return;
	}
	if (not Widget_GetIsValidModuleBox())
	{
		// Report once; retry only after the widget is reinitialized.
		M_ModuleIconState.bIsModuleBoxUnavailable = true;
		return;
	}

	bool bMembershipChanged = false;
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		if ((M_ModuleIconState.DirtyTypeMask & (1u << TypeIndex)) == 0)
		{
			continue;
		}
		if (Widget_FlushModuleIconType(TypeIndex, bMembershipChanged))
		{
			M_ModuleIconState.DirtyTypeMask &= ~(1u << TypeIndex);
		}
	}
	// A full child-order rebuild only when icon membership changed; yellow <-> red swaps stay in place.
	if (bMembershipChanged)
	{
		Widget_RebuildModuleIconChildren();
	}
}

bool UHealthComponent::Widget_FlushModuleIconType(const int32 TypeIndex, bool& bOutMembershipChanged) const
{
	const EVehicleModuleTypes Type = static_cast<EVehicleModuleTypes>(TypeIndex);
	const EVehicleModuleState DesiredState = M_ModuleIconState.DesiredStates[TypeIndex];
	const EVehicleModuleState DisplayedState = M_ModuleIconState.DisplayedStates[TypeIndex];
	if (DesiredState == DisplayedState)
	{
		return true;
	}

	bool bUpdated = true;
	if (DesiredState == EVehicleModuleState::Healthy || DisplayedState == EVehicleModuleState::Healthy)
	{
		// Add and remove change membership; the rebuild attaches or detaches the cached image.
		bUpdated = DesiredState == EVehicleModuleState::Healthy || Widget_AddModuleIcon(Type, DesiredState);
		bOutMembershipChanged = true;
	}
	else
	{
		bUpdated = Widget_ChangeModuleIcon(Type, DesiredState);
	}
	if (bUpdated)
	{
		// Record the displayed state only after a successful update.
		M_ModuleIconState.DisplayedStates[TypeIndex] = DesiredState;
	}
	return bUpdated;
}

bool UHealthComponent::Widget_AddModuleIcon(const EVehicleModuleTypes Type, const EVehicleModuleState State) const
{
	UW_HealthBar* HealthBarWidget = M_HealthBarWidget.Get();
	if (not IsValid(HealthBarWidget) || not IsValid(HealthBarWidget->WidgetTree))
	{
		return false;
	}
	const int32 TypeIndex = VehicleModuleBalance::GetModuleTypeIndex(Type);
	UImage* ModuleIconImage = HealthBarWidget->GetCachedModuleIconImage(TypeIndex);
	if (not IsValid(ModuleIconImage))
	{
		// Constructed once per type and widget instance, then reused for every later add.
		ModuleIconImage = HealthBarWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		HealthBarWidget->CacheModuleIconImage(TypeIndex, ModuleIconImage);
	}
	return Widget_ChangeModuleIcon(Type, State);
}

bool UHealthComponent::Widget_ChangeModuleIcon(const EVehicleModuleTypes Type, const EVehicleModuleState State) const
{
	const UW_HealthBar* HealthBarWidget = M_HealthBarWidget.Get();
	if (not IsValid(HealthBarWidget) || not GetIsValidVehicleModuleSubsystem())
	{
		return false;
	}
	UImage* ModuleIconImage = HealthBarWidget->GetCachedModuleIconImage(VehicleModuleBalance::GetModuleTypeIndex(Type));
	if (not IsValid(ModuleIconImage))
	{
		return false;
	}
	UTexture2D* IconTexture = nullptr;
	FVector2D IconSize = FVector2D::ZeroVector;
	if (not M_VehicleModuleSubsystem->GetModuleIconStyle(Type, State, IconTexture, IconSize))
	{
		// Missing textures were reported once on asset load; skip the icon without affecting gameplay.
		ModuleIconImage->SetVisibility(ESlateVisibility::Collapsed);
		return true;
	}
	// Store the size in the brush so the Auto slot can measure it before the Slate image is built.
	ModuleIconImage->SetBrushFromTexture(IconTexture, false);
	FSlateBrush IconBrush = ModuleIconImage->GetBrush();
	IconBrush.ImageSize = IconSize;
	ModuleIconImage->SetBrush(IconBrush);
	ModuleIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UHealthComponent::Widget_RebuildModuleIconChildren() const
{
	UHorizontalBox* ModuleBox = M_ModuleBox.Get();
	const UW_HealthBar* HealthBarWidget = M_HealthBarWidget.Get();
	if (not IsValid(ModuleBox) || not IsValid(HealthBarWidget))
	{
		return;
	}

	// Enum order, no duplicates: rebuild only runs when icon membership changed.
	ModuleBox->ClearChildren();
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		UImage* ModuleIconImage = HealthBarWidget->GetCachedModuleIconImage(TypeIndex);
		if (M_ModuleIconState.DisplayedStates[TypeIndex] == EVehicleModuleState::Healthy || not IsValid(ModuleIconImage))
		{
			continue;
		}
		UHorizontalBoxSlot* IconSlot = ModuleBox->AddChildToHorizontalBox(ModuleIconImage);
		if (IsValid(IconSlot))
		{
			IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
	ModuleBox->SetVisibility(ModuleBox->GetChildrenCount() > 0
		                         ? ESlateVisibility::SelfHitTestInvisible
		                         : ESlateVisibility::Collapsed);
}
