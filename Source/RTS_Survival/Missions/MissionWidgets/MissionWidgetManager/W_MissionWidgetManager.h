#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_MissionWidgetManager.generated.h"

class UW_Mission;

UCLASS()
class RTS_SURVIVAL_API UW_MissionWidgetManager : public UUserWidget
{
	GENERATED_BODY()
public:
	/** return A valid mission widget that has no mission assigned. Null if non available. */
	UW_Mission* GetFreeMissionWidget();

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitMissionWidgetReferences(TArray<UW_Mission*> MissionWidgets);
	

private:
	/**
	 * Widgets for missions that are currently active.
	 */
	UPROPERTY()
	TArray<UW_Mission*> M_MissionWidgets;

	bool EnsureMissionWidgetIsValid(UW_Mission* MissionWidget) const;

		
};
