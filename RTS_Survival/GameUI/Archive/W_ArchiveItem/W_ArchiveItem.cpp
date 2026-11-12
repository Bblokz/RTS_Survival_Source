#include "W_ArchiveItem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ArchiveItemCustomContent
{
	// Widget names in W_BP_ArchiveItem.
	const FName ImageWidgetName(TEXT("ItemImage"));
	const FName TitleWidgetName(TEXT("RichTitleText"));
	const FName DescriptionWidgetName(TEXT("RichDescription"));
	// The size W_BP_ArchiveItem gives DT_ArchiveItem pictures.
	const FVector2D ImageSize(128.f, 128.f);
}

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

ERTSArchiveType UW_ArchiveItem::SetupCustomArchiveItem(const FArchiveCustomContent& Content)
{
	using namespace ArchiveItemCustomContent;
	M_ArchiveItem = ERTSArchiveItem::CustomContent;
	M_CustomContentKey = Content.Key;
	if (not IsValid(WidgetTree))
	{
		return ERTSArchiveType::None;
	}
	URichTextBlock* const TitleText = Cast<URichTextBlock>(WidgetTree->FindWidget(TitleWidgetName));
	URichTextBlock* const DescriptionText = Cast<URichTextBlock>(WidgetTree->FindWidget(DescriptionWidgetName));
	UImage* const ItemImage = Cast<UImage>(WidgetTree->FindWidget(ImageWidgetName));
	if (not IsValid(TitleText) || not IsValid(DescriptionText) || not IsValid(ItemImage))
	{
		RTSFunctionLibrary::ReportError(TEXT("W_BP_ArchiveItem lacks ItemImage, RichTitleText or RichDescription; "
			"custom archive entries cannot be shown."));
		return ERTSArchiveType::None;
	}
	TitleText->SetText(FText::Format(NSLOCTEXT("Archive", "CustomTitle", "<Text_Title>{0}</>"), Content.Title));
	DescriptionText->SetText(Content.Description);
	if (IsValid(Content.ImageTexture))
	{
		FSlateBrush TextureBrush;
		TextureBrush.SetResourceObject(Content.ImageTexture);
		TextureBrush.ImageSize = ImageSize;
		ItemImage->SetBrush(TextureBrush);
	}
	else if (Content.bHasImage)
	{
		ItemImage->SetBrush(Content.ImageBrush);
	}
	return Content.SortType;
}
