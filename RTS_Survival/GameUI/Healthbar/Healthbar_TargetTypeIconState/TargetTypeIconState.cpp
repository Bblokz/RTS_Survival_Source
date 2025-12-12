#include "TargetTypeIconState.h"

#include "Components/Image.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarIcons/HealthBarIcons.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"

bool FTargetTypeIconState::GetImplementsTargetTypeIcon() const
{
	if (IsValid(TargetTypeIconImage))
	{
		return true;
	}
	return false;
}

void FTargetTypeIconState::SetNewTargetTypeIcon(const int8 OwningPlayer, const ETargetTypeIcon NewTargetTypeIcon)
{
	if(not IsValid(TargetTypeIconImage))
	{
		return;
	}
	const ESlateVisibility NewVisibility = NewTargetTypeIcon == ETargetTypeIcon::None
		                                       ? ESlateVisibility::Collapsed
		                                       : ESlateVisibility::Visible;
	TargetTypeIconImage->SetVisibility(NewVisibility);
	if (not IsTypeContainedAndValid(NewTargetTypeIcon))
	{
		return;
	}

	TargetTypeIconImage->SetBrushFromAsset(OwningPlayer == 1
		                                       ? TypeToBrush[NewTargetTypeIcon].PlayerBrush
		                                       : TypeToBrush[NewTargetTypeIcon].EnemyBrush);
	
	
}

bool FTargetTypeIconState::IsTypeContainedAndValid(const ETargetTypeIcon Type)
{
	if (TypeToBrush.Contains(Type) && IsValid(TypeToBrush[Type].PlayerBrush) && IsValid(TypeToBrush[Type].EnemyBrush))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("The target type icon brushes are not set correctly"
		"\n For Type: " + UEnum::GetValueAsString(Type));
	return false;
}
