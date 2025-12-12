#include "W_ArchiveItem.h"

ERTSArchiveType UW_ArchiveItem::SetupArchiveItem(const ERTSArchiveItem ArchiveItem, const FTrainingOption OptionalUnit)
{
	M_ArchiveItem = ArchiveItem;
	M_OptionalUnit = OptionalUnit;
	return OnArchiveItemSetup(M_ArchiveItem, M_OptionalUnit);
}

void UW_ArchiveItem::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
}


void UW_ArchiveItem::SetArchiveSortingType(const ERTSArchiveType ArchiveSortingType)
{
	M_ActiveSortingType = ArchiveSortingType;
}

ERTSArchiveType UW_ArchiveItem::OnArchiveItemSetup_Implementation(const ERTSArchiveItem ArchiveItem,
                                                                  const FTrainingOption OptionalUnit)
{
	return ERTSArchiveType::None;
}
