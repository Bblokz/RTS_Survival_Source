#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "W_Archive.generated.h"

class UW_WorldMenu;
class UMainGameUI;
class UScrollBox;
class UW_ArchiveItem;
struct FTrainingOption;

USTRUCT()
struct FArchiveItemData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UW_ArchiveItem> ItemWidget = nullptr;

	UPROPERTY()
	ERTSArchiveType ItemSortingType = ERTSArchiveType::None;

	UPROPERTY()
	int32 SortingPriority = 0;

	bool IsValid() const
	{
		return ItemWidget != nullptr;
	}
	
};

UCLASS()
class UW_Archive: public UUserWidget 
{
	GENERATED_BODY()

public:
	void AddArchiveItem(const ERTSArchiveItem NewItem, const FTrainingOption OptionalUnit = FTrainingOption(), const int32 SortingPriority = 0);
	void SetMainGameUIReference(UMainGameUI* MainGameUI);
	void SetWorldMenuUIReference(UW_WorldMenu* WorldMenu);
	void OnOpenArchive();
protected:
	UFUNCTION(BlueprintCallable)
	void InitArchive(
		const TSubclassOf<UW_ArchiveItem>& ArchiveItemClass);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ArchiveScrollBox;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SortForType(const ERTSArchiveType SortingType);

	UFUNCTION(BlueprintCallable)
	void ExitArchive() const;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnOpenArchive();

private:
	ERTSArchiveType M_ActiveSortingType = ERTSArchiveType::GameMechanics;
	
	UPROPERTY()
	TSubclassOf<UW_ArchiveItem> M_ArchiveItemClass;
	
	bool bM_IsSetupWithMainMenu = false;

	UPROPERTY()
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;
	bool GetIsValidMainMenu() const;
	void ExitArchive_MainMenu() const;
	
	UPROPERTY()
	TWeakObjectPtr<UW_WorldMenu> M_WorldMenu;
	bool GetIsValidWorldMenu() const;
	void ExitArchive_WorldMenu() const;

	UW_ArchiveItem* CreateNewArchiveItem();

	ERTSArchiveType AddArchiveWidgetToScrollBox(
		UW_ArchiveItem* ArchiveItemWidget, ERTSArchiveItem ItemType, const FTrainingOption UnitType, const int32
		SortingPriority);

	bool GetIsValidArchiveItemClass() const;

	// data of items currently stored.
	UPROPERTY()
	TArray<FArchiveItemData> M_ArchiveItems;

	bool IsUnitArchiveAlreadyInList(
		FTrainingOption UnitType);

	bool IsArchiveItemAlreadyInList(
		ERTSArchiveItem Item);

	void OnItemAlreadyInList() const;
	void OnItemInvalid()const;
	void OnFailedToCreateArchiveItem() const;
};
