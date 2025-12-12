// W_ArchiveNotificationHolder.cpp

#include "W_ArchiveNotificationHolder.h"
#include "RTS_Survival/GameUI/Archive/W_ArchivePushNotification/W_ArchivePushNotification.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Components/ScrollBox.h"
#include "TimerManager.h"
#include "RTS_Survival/GameUI/MainGameUI.h"

void UW_ArchiveNotificationHolder::AddNewArchiveNotification(
	ERTSArchiveItem ItemType,
	const FTrainingOption OptionalUnit,
	const float NotificationTime)
{
	if (not EnsureNotificationClassIsValid())
	{
		return;
	}

	if (not GetIsValidNotificationScrollBox())
	{
		return;
	}

	if (NotificationTime <= 0.f)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("AddNewArchiveNotification: NotificationTime must be > 0."));
		return;
	}

	UW_ArchivePushNotification* NewNotification =
		CreateWidget<UW_ArchivePushNotification>(this, NotificationClass);

	if (not NewNotification)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("AddNewArchiveNotification: Failed to create notification widget."));
		return;
	}

	NewNotification->SetupNotification(ItemType, OptionalUnit);
	NewNotification->SetNotificationHolder(this);

	NotificationScrollBox->AddChild(NewNotification);

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("AddNewArchiveNotification: World is null; cannot set timer."));
		return;
	}

	// Bind a one-shot delegate that calls RemoveNotification with this widget pointer
	FTimerDelegate Delegate;
	TWeakObjectPtr<UW_ArchiveNotificationHolder> WeakThis(this);
	auto OnRemove = [WeakThis, NewNotification]() -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->RemoveNotification(NewNotification);
	};
	Delegate.BindLambda(OnRemove);

	// We don't need to keep the handle around—we just need the timer to fire once
	FTimerHandle Handle;
	World->GetTimerManager()
	     .SetTimer(Handle, Delegate, NotificationTime, /*bLoop=*/ false);

	// Only show if needed, respecting suppression.
	IfNotVisibleSetVisibilityForNewNotification();
	BP_OnNewNotificationAdded();
}

void UW_ArchiveNotificationHolder::SetSuppressAchive(const bool bSuppress)
{
	bM_IsSuppressed = bSuppress;

	// When suppressed: always collapse and ignore normal rules.
	if (bM_IsSuppressed)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// When un-suppressed: if we have items, show; otherwise stay collapsed.
	if (not GetIsValidNotificationScrollBox())
	{
		return;
	}

	const int32 CurrentChildren = NotificationScrollBox->GetChildrenCount();
	if (CurrentChildren > 0)
	{
		SetVisibility(ESlateVisibility::Visible);
		return;
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_ArchiveNotificationHolder::SetMainMenuReference(TObjectPtr<UMainGameUI> MainGameUI)
{
	M_MainGameUI = MainGameUI;
	// Error Check.
	(void)GetIsValidMainMenu();
}

void UW_ArchiveNotificationHolder::OnClickedNotification(UW_ArchivePushNotification* NotificationWidget)
{
	if (not GetIsValidMainMenu())
	{
		return;
	}

	// Hide the notification
	if (NotificationWidget)
	{
		NotificationWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	M_MainGameUI->OnOpenArchive();
}

bool UW_ArchiveNotificationHolder::EnsureNotificationClassIsValid() const
{
	if (IsValid(NotificationClass))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		TEXT("No valid NotificationClass on ArchiveNotificationHolder. ")
		TEXT("Ensure the class is set in derived blueprint defaults."));
	return false;
}

void UW_ArchiveNotificationHolder::RemoveNotification(UW_ArchivePushNotification* NotificationWidget)
{
	// Defensive checks
	if (not NotificationWidget)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("RemoveNotification: passed a null NotificationWidget."));
		return;
	}

	if (not GetIsValidNotificationScrollBox())
	{
		return;
	}

	// Try to remove it—RemoveChild also calls RemoveFromParent()
	const bool bRemoved = NotificationScrollBox->RemoveChild(NotificationWidget);
	if (not bRemoved)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("RemoveNotification: failed to RemoveChild the notification widget."));
	}

	const int32 AmountChildren = NotificationScrollBox->GetChildrenCount();
	if (AmountChildren <= 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UW_ArchiveNotificationHolder::GetIsValidMainMenu() const
{
	if (M_MainGameUI.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		TEXT("No valid MainMenu reference on ArchiveNotificationHolder. ")
		TEXT("\n ensure the reference is set after init on the MainGameUI!"));
	return false;
}

bool UW_ArchiveNotificationHolder::GetIsValidNotificationScrollBox() const
{
	if (IsValid(NotificationScrollBox))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		TEXT("ArchiveNotificationHolder: NotificationScrollBox is null. ")
		TEXT("Bind the widget or ensure it exists before use."));
	return false;
}

void UW_ArchiveNotificationHolder::IfNotVisibleSetVisibilityForNewNotification()
{
	// Respect suppression first.
	if (bM_IsSuppressed)
	{
		return;
	}

	if (not GetIsValidNotificationScrollBox())
	{
		return;
	}

	// Only show if there is at least one child to display.
	if (NotificationScrollBox->GetChildrenCount() <= 0)
	{
		return;
	}

	if (GetVisibility() != ESlateVisibility::Visible)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}
