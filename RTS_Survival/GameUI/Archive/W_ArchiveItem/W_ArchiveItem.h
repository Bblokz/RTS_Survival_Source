#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "W_ArchiveItem.generated.h"


UCLASS()
class UW_ArchiveItem : public UUserWidget
{
	GENERATED_BODY()

public:
	ERTSArchiveType SetupArchiveItem(const ERTSArchiveItem ArchiveItem,
	                                 const FTrainingOption OptionalUnit = FTrainingOption());

	virtual void NativeOnInitialized() override;

	inline ERTSArchiveItem GetArchiveItem() const
	{
		return M_ArchiveItem;
	}

	inline FTrainingOption GetUnitTrainingOption() const
	{
		return M_OptionalUnit;
	}

protected:
	UFUNCTION(BlueprintNativeEvent)
	ERTSArchiveType OnArchiveItemSetup(const ERTSArchiveItem ArchiveItem,
	                                   const FTrainingOption OptionalUnit = FTrainingOption());

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetArchiveSortingType(const ERTSArchiveType ArchiveSortingType);

private:
	UPROPERTY()
	ERTSArchiveItem M_ArchiveItem;

	UPROPERTY()
	FTrainingOption M_OptionalUnit;

	UPROPERTY()
	ERTSArchiveType M_ActiveSortingType = ERTSArchiveType::None;
};
