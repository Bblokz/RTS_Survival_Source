#include "W_ArchivePushNotification.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"

#include "RTS_Survival/GameUI/Archive/W_ArchiveNotificationHolder/W_ArchiveNotificationHolder.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_ArchivePushNotification::SetNotificationHolder(UW_ArchiveNotificationHolder* NotificationHolder)
{
	M_NotificationHolder = NotificationHolder;
	// Ensure that the notification holder is valid.
	(void)GetIsValidNotificationHolder();
}

void UW_ArchivePushNotification::OnClickedNotification()
{
	if(not GetIsValidNotificationHolder())
	{
		return;
	}
	M_NotificationHolder->OnClickedNotification(this);
}

bool UW_ArchivePushNotification::GetIsValidNotificationHolder() const
{
	if(M_NotificationHolder.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		TEXT("No valid notification holder for notfication widget. "));
	return false;
}

void UW_ArchivePushNotification::SetupCustomNotification(const FArchiveCustomContent& Content)
{
	// The button in W_BP_ArchivePushNotification whose images show the archived picture.
	const FName ArchiveButtonName(TEXT("ArchiveButton"));
	UButton* const ArchiveButton = IsValid(WidgetTree)
		                               ? Cast<UButton>(WidgetTree->FindWidget(ArchiveButtonName))
		                               : nullptr;
	if (not IsValid(ArchiveButton))
	{
		RTSFunctionLibrary::ReportError(TEXT("W_BP_ArchivePushNotification has no ArchiveButton; the custom archive "
			"pop-up shows no picture."));
		return;
	}
	if (not Content.bHasImage && not IsValid(Content.ImageTexture))
	{
		return;
	}
	// Keep each state's tint, size and margins; only the picture changes, as AdjustButtonWithImage does.
	FButtonStyle Style = ArchiveButton->GetStyle();
	const auto SetPicture = [&Content](FSlateBrush& StateBrush)
	{
		if (IsValid(Content.ImageTexture))
		{
			StateBrush.SetResourceObject(Content.ImageTexture);
			return;
		}
		StateBrush.SetResourceObject(Content.ImageBrush.GetResourceObject());
		const FBox2f BrushUVRegion = Content.ImageBrush.GetUVRegion();
		StateBrush.SetUVRegion(BrushUVRegion);
	};
	SetPicture(Style.Normal);
	SetPicture(Style.Hovered);
	SetPicture(Style.Pressed);
	ArchiveButton->SetStyle(Style);
}
