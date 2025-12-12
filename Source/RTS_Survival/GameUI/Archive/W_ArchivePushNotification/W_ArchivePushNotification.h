#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"

#include "W_ArchivePushNotification.generated.h"

class UW_ArchiveNotificationHolder;
struct FTrainingOption;

UCLASS()
class UW_ArchivePushNotification : public UUserWidget 
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	void SetupNotification(
		const ERTSArchiveItem ItemType, const FTrainingOption OptionalUnit = FTrainingOption());

	void SetNotificationHolder(UW_ArchiveNotificationHolder* NotificationHolder);

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickedNotification();
private:
	TWeakObjectPtr<UW_ArchiveNotificationHolder> M_NotificationHolder;
	bool GetIsValidNotificationHolder() const;
};
