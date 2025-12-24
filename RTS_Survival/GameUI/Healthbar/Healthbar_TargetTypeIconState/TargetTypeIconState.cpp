#include "TargetTypeIconState.h"

#include "Components/Image.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarIcons/HealthBarIcons.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"

bool FTargetTypeIconState::GetImplementsTargetTypeIcon() const
{
	if (TargetTypeIconImage.IsValid())
	{
		return true;
	}
	return false;
}

void FTargetTypeIconState::SetNewTargetTypeIcon(const int8 OwningPlayer, const ETargetTypeIcon NewTargetTypeIcon, const AActor* ActorContext)
{
	if (not TargetTypeIconImage.IsValid())
	{
		return;
	}
	const ESlateVisibility NewVisibility = NewTargetTypeIcon == ETargetTypeIcon::None
		                                       ? ESlateVisibility::Collapsed
		                                       : ESlateVisibility::Visible;
	TargetTypeIconImage->SetVisibility(NewVisibility);
	if (not IsTypeContainedAndValid(NewTargetTypeIcon, ActorContext))
	{
		return;
	}

	TargetTypeIconImage->SetBrushFromAsset(OwningPlayer == 1
		                                         ? TypeToBrush[NewTargetTypeIcon].PlayerBrush
		                                         : TypeToBrush[NewTargetTypeIcon].EnemyBrush);
}

bool FTargetTypeIconState::IsTypeContainedAndValid(const ETargetTypeIcon Type, const AActor* ActorContext)
{
	if (TypeToBrush.Contains(Type) && IsValid(TypeToBrush[Type].PlayerBrush) && IsValid(TypeToBrush[Type].EnemyBrush))
	{
		return true;
	}
	FString OwnerName =  ActorContext ? ActorContext->GetName() : TEXT("Unknown Actor");
	OwnerName = "Actor context: "  + OwnerName;
	RTSFunctionLibrary::ReportError("The target type icon brushes are not set correctly"
		"\n For Type: " + UEnum::GetValueAsString(Type));
	return false;
}
