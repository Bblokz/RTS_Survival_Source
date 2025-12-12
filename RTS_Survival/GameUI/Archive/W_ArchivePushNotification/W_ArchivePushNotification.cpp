#include "W_ArchivePushNotification.h"

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
