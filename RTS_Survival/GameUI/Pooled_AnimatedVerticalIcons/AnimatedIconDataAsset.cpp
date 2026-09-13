#include "AnimatedIconDataAsset.h"

#include "RTSVerticalAnimatedIcon.h"

UAnimatedIconDataAsset::UAnimatedIconDataAsset()
{
	WidgetClass = UW_RTSVerticalAnimatedIcon::StaticClass();
	IconDefinitions.Add(ERTSVerticalAnimatedIcon::CommanderBoost);
	IconDefinitions.Add(ERTSVerticalAnimatedIcon::RangeBoost);
	IconDefinitions.Add(ERTSVerticalAnimatedIcon::Healing);
}
