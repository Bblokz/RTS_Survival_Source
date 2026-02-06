#include "W_Archive.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/Archive/W_ArchiveItem/W_ArchiveItem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"

void UW_Archive::AddArchiveItem(const ERTSArchiveItem NewItem, const FTrainingOption OptionalUnit,
                                const int32 SortingPriority)
{
	if (ERTSArchiveItem::None == NewItem)
	{
		OnItemInvalid();
		return;
	}
	bool bIsUnique = false;
	if (NewItem != ERTSArchiveItem::Unit)
	{
		bIsUnique = IsArchiveItemAlreadyInList(NewItem);
	}
	else
	{
		bIsUnique = IsUnitArchiveAlreadyInList(OptionalUnit);
	}
	if (bIsUnique)
	{
		OnItemAlreadyInList();
		return;
	}
	UW_ArchiveItem* NewArchiveItem = CreateNewArchiveItem();
	if (not NewArchiveItem)
	{
		OnFailedToCreateArchiveItem();
		return;
	}
	const ERTSArchiveType NewSortType = AddArchiveWidgetToScrollBox(NewArchiveItem, NewItem, OptionalUnit, SortingPriority);
	// Ensure the type of the new item is at the top of the list.
	SortForType(NewSortType);
}

void UW_Archive::SetMainGameUIReference(UMainGameUI* MainGameUI)
{
	bM_IsSetupWithMainMenu = true;
	M_MainGameUI = MainGameUI;
}

void UW_Archive::SetWorldMenuUIReference(UW_WorldMenu* WorldMenu)
{
	bM_IsSetupWithMainMenu = false;
	M_WorldMenu = WorldMenu;
};

void UW_Archive::OnOpenArchive()
{
	BP_OnOpenArchive();
}

void UW_Archive::InitArchive(const TSubclassOf<UW_ArchiveItem>& ArchiveItemClass)
{
	M_ArchiveItemClass = ArchiveItemClass;
	// Error check.
	(void)GetIsValidArchiveItemClass();
}

void UW_Archive::SortForType(const ERTSArchiveType SortingType)
{
	// Ensure that all items of the SortingType appear first in the scrollbox, where these item are sorted by their
	// priority. All remaining items of different sorting types are sorted below it by their priority.
	M_ActiveSortingType = SortingType;

	// Sort the backing array:
	//  - First all items whose .ItemSortingType == SortingType (in descending priority)
	//  - Then the rest (also in descending priority)
	M_ArchiveItems.Sort([SortingType](const FArchiveItemData& A, const FArchiveItemData& B)
	{
		const bool bAIsActive = (A.ItemSortingType == SortingType);
		const bool bBIsActive = (B.ItemSortingType == SortingType);

		// if only one is the active type, that one goes first
		if (bAIsActive != bBIsActive)
		{
			return bAIsActive;
		}

		// otherwise both are in the same group: highest-priority (largest int) first
		return A.SortingPriority > B.SortingPriority;
	});

	if (!ArchiveScrollBox)
	{
		RTSFunctionLibrary::ReportError(TEXT("SortForType: ArchiveScrollBox is null."));
		return;
	}

	ArchiveScrollBox->ClearChildren();
	for (const FArchiveItemData& Data : M_ArchiveItems)
	{
		if (Data.IsValid())
		{
			ArchiveScrollBox->AddChild(Data.ItemWidget);
		}
	}
}

void UW_Archive::ExitArchive() const
{
	if(bM_IsSetupWithMainMenu)
	{
		ExitArchive_MainMenu();
	}
	else
	{
		ExitArchive_WorldMenu();
	}
}

bool UW_Archive::GetIsValidMainMenu() const
{
	if(not bM_IsSetupWithMainMenu)
	{
		RTSFunctionLibrary::ReportError("Attempted to check validity of main menu ui on the archive but the archive"
			"has been setup with the world menu instead!");
		return false;
	}
	if(M_MainGameUI.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"Invalid main menu UI reference for archive manager,"
		"\n Ensure that the main menu reference is set when opening the archive!");
	return false;
}

void UW_Archive::ExitArchive_MainMenu() const
{
	if(not GetIsValidMainMenu())
	{
		return;
	}
	M_MainGameUI->OnCloseArchive();
	
}

bool UW_Archive::GetIsValidWorldMenu() const
{
	if(bM_IsSetupWithMainMenu)
	{
		RTSFunctionLibrary::ReportError("Attempted to check validity of world menu ui on the archive but the archive"
			"has been setup with the main menu instead!");
		return false;
	}
	if(M_WorldMenu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"Invalid world menu UI reference for archive manager,"
		"\n Ensure that the world menu reference is set when opening the archive!");
	return false;
}

void UW_Archive::ExitArchive_WorldMenu() const
{
	if(not GetIsValidWorldMenu())
	{
		return;
	}
	M_WorldMenu->OnPlayerExitsArchive();
}

UW_ArchiveItem* UW_Archive::CreateNewArchiveItem()
{
	if (not GetIsValidArchiveItemClass())
	{
		return nullptr;
	}
	UW_ArchiveItem* NewArchiveItem = CreateWidget<UW_ArchiveItem>(this, M_ArchiveItemClass);
	return NewArchiveItem;
}

ERTSArchiveType UW_Archive::AddArchiveWidgetToScrollBox(
    UW_ArchiveItem* ArchiveItemWidget,
    const ERTSArchiveItem ItemType,
    const FTrainingOption UnitType,
    const int32 SortingPriority)
{
    const ERTSArchiveType ArchiveSortingType = ArchiveItemWidget->SetupArchiveItem(ItemType, UnitType);

    const FArchiveItemData NewData = { ArchiveItemWidget, ArchiveSortingType, SortingPriority };
    M_ArchiveItems.Add(NewData);

    // Add to scroll box
    ArchiveScrollBox->AddChild(ArchiveItemWidget);

    // Get the slot and apply layout settings
    if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(ArchiveItemWidget->Slot))
    {
        ScrollSlot->SetHorizontalAlignment(HAlign_Center);
        ScrollSlot->SetVerticalAlignment(VAlign_Center);
        ScrollSlot->SetPadding(FMargin(0.0f, 25.0f, 0.0f, 0.0f));
    }

    return ArchiveSortingType;
}


bool UW_Archive::GetIsValidArchiveItemClass() const
{
	if (IsValid(M_ArchiveItemClass))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"Invalid ArchiveItemClass for archive manager,"
		"\n Ensure that the class is set in derived blueprint defaults!");
	return false;
}

bool UW_Archive::IsUnitArchiveAlreadyInList(const FTrainingOption UnitType)
{
	for (const auto& EachItem : M_ArchiveItems)
	{
		if (EachItem.IsValid() && EachItem.ItemWidget->GetUnitTrainingOption() == UnitType)
		{
			return true;
		}
	}
	return false;
}

bool UW_Archive::IsArchiveItemAlreadyInList(const ERTSArchiveItem Item)
{
	if (Item == ERTSArchiveItem::None || Item == ERTSArchiveItem::Unit)
	{
		RTSFunctionLibrary::ReportError("Attempted to check archive item in list but provided item"
			"is null or unit (ambiguous).");
		return false;
	}
	for (const auto& EachItem : M_ArchiveItems)
	{
		if (EachItem.IsValid() && EachItem.ItemWidget->GetArchiveItem() == Item)
		{
			return true;
		}
	}
	return false;
}

void UW_Archive::OnItemAlreadyInList() const
{
	RTSFunctionLibrary::ReportError("Attempted to add archive item to list but item is already in list."
		"\n Skipping!");
}

void UW_Archive::OnItemInvalid() const
{
	RTSFunctionLibrary::ReportError("Will not add archive item to archive as it is invalid (set to None)");
}

void UW_Archive::OnFailedToCreateArchiveItem() const
{
	RTSFunctionLibrary::ReportError("Failed to create archive item, will not add to archive.");
}
