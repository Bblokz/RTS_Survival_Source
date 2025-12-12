#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "W_ArchiveNotificationHolder.generated.h"

class UMainGameUI;
class UScrollBox;
class UW_ArchivePushNotification;
struct FTrainingOption;

/**
 * @brief Lightweight notification rail for Archive updates; adds and times out push notices,
 *        and exposes a suppression switch that collapses it regardless of normal visibility rules.
 * @note SetMainMenuReference: call in blueprint to set the owning/main UI after creation.
 */
UCLASS()
class RTS_SURVIVAL_API UW_ArchiveNotificationHolder : public UUserWidget
{
	GENERATED_BODY()
public:
	/**
	 * @brief Adds a new archive notification and schedules its auto-removal.
	 * @param ItemType           Type/category of archive item to notify about.
	 * @param OptionalUnit       Optional training option payload for the notification.
	 * @param NotificationTime   Lifetime (seconds) before the notification auto-collapses.
	 */
	void AddNewArchiveNotification(ERTSArchiveItem ItemType,
		const FTrainingOption OptionalUnit = FTrainingOption(),
		const float NotificationTime = 0);

	/**
	 * @brief Collapses or un-collapses this holder independently from its normal visibility logic.
	 *        While suppressed, new items will NOT expand/show the holder. On un-suppress, if there
	 *        are items present, the holder becomes visible; otherwise remains collapsed.
	 * @param bSuppress  true to force-collapse; false to return control to normal visibility rules.
	 */
	void SetSuppressAchive(const bool bSuppress);

	void SetMainMenuReference(TObjectPtr<UMainGameUI> MainGameUI);

	void OnClickedNotification(UW_ArchivePushNotification* NotificationWidget);
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSubclassOf<UW_ArchivePushNotification>	NotificationClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> NotificationScrollBox;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnNewNotificationAdded();

private:
	bool EnsureNotificationClassIsValid() const;

	// Removes the notification from the scrollbox and destroys it.
	// If no notifications remain this widget will collapse.
	void RemoveNotification(UW_ArchivePushNotification* NotificationWidget);

	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	// Validates M_MainGameUI and reports once in a centralized spot.
	bool GetIsValidMainMenu() const;

	// Validates NotificationScrollBox and reports once in a centralized spot.
	bool GetIsValidNotificationScrollBox() const;

	// Shows the holder only if we are not suppressed and there is content to show.
	void IfNotVisibleSetVisibilityForNewNotification();

	// While true, the holder is force-collapsed regardless of content/normal rules.
	bool bM_IsSuppressed = false;
};
