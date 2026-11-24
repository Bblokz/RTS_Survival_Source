// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/WorldBuildSiteHelpers.h"

#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildingSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"

#define LOCTEXT_NAMESPACE "WorldBuildSite"

namespace WorldBuildSiteHelpersPrivate
{
	// Code colours of the build cards in UI sketch sheet 2 A.
	const FLinearColor WatchPostCodeColor(FColor::FromHex(TEXT("E6DFC8")));
	const FLinearColor SupplyDepotCodeColor(FColor::FromHex(TEXT("B9C79A")));
	const FLinearColor AirfieldCodeColor(FColor::FromHex(TEXT("FFB347")));
	const FLinearColor BatteryCodeColor(FColor::FromHex(TEXT("FF6A4D")));

	constexpr int32 MaxCodeLetters = 2;
	// Two radii closer than this count as the same reach.
	constexpr float SameRadiusTolerance = 1.f;
	constexpr float PercentScale = 100.f;

	FText GetInitials(const FText& DisplayName)
	{
		TArray<FString> Words;
		DisplayName.ToString().ParseIntoArrayWS(Words);
		FString Code;
		if (Words.Num() == 1)
		{
			Code = Words[0].Left(MaxCodeLetters);
		}
		for (int32 WordIndex = 0; Words.Num() > 1 && WordIndex < Words.Num() && Code.Len() < MaxCodeLetters; ++WordIndex)
		{
			Code.AppendChar(Words[WordIndex][0]);
		}
		return FText::FromString(Code.ToUpper());
	}

	FText GetIncomeText(const FWorldMapResources& DepotIncome)
	{
		const FText RadixitePart = FText::Format(LOCTEXT("IncomeRadixite", "+{0} radixite"),
		                                         FText::AsNumber(DepotIncome.Radixite));
		const FText BlueprintPart = DepotIncome.Blueprints == 1
			                            ? LOCTEXT("IncomeOneBlueprint", "+1 blueprint")
			                            : FText::Format(LOCTEXT("IncomeBlueprints", "+{0} blueprints"),
			                                            FText::AsNumber(DepotIncome.Blueprints));
		if (DepotIncome.Radixite > 0 && DepotIncome.Blueprints > 0)
		{
			return FText::Format(LOCTEXT("IncomeBoth", "{0} and {1} each turn"), RadixitePart, BlueprintPart);
		}
		if (DepotIncome.Radixite > 0)
		{
			return FText::Format(LOCTEXT("IncomeOnly", "{0} each turn"), RadixitePart);
		}
		if (DepotIncome.Blueprints > 0)
		{
			return FText::Format(LOCTEXT("IncomeOnly", "{0} each turn"), BlueprintPart);
		}
		return FText::GetEmpty();
	}

	FText GetSupportText(const FWorldBuildingEffectDefinition& Effect, const TArray<float>& AllSupportRadii)
	{
		const FText StrengthPart = FText::Format(LOCTEXT("SupportStrength", "-{0} % enemy strength"),
		                                         FText::AsNumber(Effect.EnemyStrengthReductionPercent));
		switch (WorldBuildSiteHelpers::GetSupportRangeWord(Effect.EnemyStrengthReductionRadius, AllSupportRadii))
		{
		case EWorldBuildSiteRangeWord::Long:
			return FText::Format(LOCTEXT("SupportLongRange", "{0}, long range"), StrengthPart);
		case EWorldBuildSiteRangeWord::Short:
			return FText::Format(LOCTEXT("SupportShortRange", "{0}, short range"), StrengthPart);
		case EWorldBuildSiteRangeWord::None:
			break;
		}
		return StrengthPart;
	}
}

FText WorldBuildSiteHelpers::GetBuildingCode(const EWorldCampaignBuilding Building, const FText& DisplayName)
{
	switch (Building)
	{
	case EWorldCampaignBuilding::WatchPost:
		return LOCTEXT("CodeWatchPost", "WP");
	case EWorldCampaignBuilding::SupplyDepot:
		return LOCTEXT("CodeSupplyDepot", "SD");
	case EWorldCampaignBuilding::Airfield:
		return LOCTEXT("CodeAirfield", "AF");
	case EWorldCampaignBuilding::Battery:
		return LOCTEXT("CodeBattery", "BT");
	default:
		return WorldBuildSiteHelpersPrivate::GetInitials(DisplayName);
	}
}

FLinearColor WorldBuildSiteHelpers::GetBuildingCodeColor(const EWorldCampaignBuilding Building)
{
	using namespace WorldBuildSiteHelpersPrivate;
	switch (Building)
	{
	case EWorldCampaignBuilding::WatchPost:
		return WatchPostCodeColor;
	case EWorldCampaignBuilding::SupplyDepot:
		return SupplyDepotCodeColor;
	case EWorldCampaignBuilding::Airfield:
		return AirfieldCodeColor;
	case EWorldCampaignBuilding::Battery:
		return BatteryCodeColor;
	default:
		return WorldCampaignUIColors::Cream;
	}
}

FText WorldBuildSiteHelpers::GetBuildingDisplayName(const EWorldCampaignBuilding Building)
{
	const FWorldBuildingEffectDefinition* const Effect = UWorldBuildingSettings::FindBuildingEffect(Building);
	if (Effect != nullptr && not Effect->DisplayName.IsEmpty())
	{
		return Effect->DisplayName;
	}
	return StaticEnum<EWorldCampaignBuilding>()->GetDisplayNameTextByValue(static_cast<int64>(Building));
}

EWorldBuildSiteRangeWord WorldBuildSiteHelpers::GetSupportRangeWord(const float Radius,
                                                                    const TArray<float>& AllSupportRadii)
{
	using namespace WorldBuildSiteHelpersPrivate;
	if (AllSupportRadii.Num() < 2)
	{
		return EWorldBuildSiteRangeWord::None;
	}

	float LargestRadius = AllSupportRadii[0];
	float SmallestRadius = AllSupportRadii[0];
	for (const float SupportRadius : AllSupportRadii)
	{
		LargestRadius = FMath::Max(LargestRadius, SupportRadius);
		SmallestRadius = FMath::Min(SmallestRadius, SupportRadius);
	}
	if (LargestRadius - SmallestRadius <= SameRadiusTolerance)
	{
		return EWorldBuildSiteRangeWord::None;
	}
	if (Radius >= LargestRadius - SameRadiusTolerance)
	{
		return EWorldBuildSiteRangeWord::Long;
	}
	if (Radius <= SmallestRadius + SameRadiusTolerance)
	{
		return EWorldBuildSiteRangeWord::Short;
	}
	return EWorldBuildSiteRangeWord::None;
}

FText WorldBuildSiteHelpers::GetBuildingEffectText(const FWorldBuildingEffectDefinition& Effect,
                                                   const FWorldMapResources& DepotIncome,
                                                   const TArray<float>& AllSupportRadii)
{
	using namespace WorldBuildSiteHelpersPrivate;
	TArray<FText> EffectParts;
	if (Effect.GetRevealsFow())
	{
		EffectParts.Add(LOCTEXT("RevealsFog", "Reveals fog around it"));
	}
	if (Effect.bPaysSupplyDepotIncome)
	{
		const FText IncomeText = GetIncomeText(DepotIncome);
		if (not IncomeText.IsEmpty())
		{
			EffectParts.Add(IncomeText);
		}
	}
	if (Effect.GetReducesEnemyStrength())
	{
		EffectParts.Add(GetSupportText(Effect, AllSupportRadii));
	}

	if (EffectParts.Num() == 0)
	{
		return Effect.Description;
	}
	return FText::Join(LOCTEXT("EffectSeparator", "; "), EffectParts);
}

TArray<float> WorldBuildSiteHelpers::GatherSupportRadii()
{
	TArray<float> SupportRadii;
	const UWorldBuildingSettings* const BuildingSettings = UWorldBuildingSettings::Get();
	if (BuildingSettings == nullptr)
	{
		return SupportRadii;
	}

	for (const TPair<EWorldCampaignBuilding, FWorldBuildingEffectDefinition>& BuildingEffect :
	     BuildingSettings->BuildingEffects)
	{
		if (BuildingEffect.Value.GetReducesEnemyStrength())
		{
			SupportRadii.Add(BuildingEffect.Value.EnemyStrengthReductionRadius);
		}
	}
	return SupportRadii;
}

float WorldBuildSiteHelpers::GetUnlockLiberation(const int32 UnlockRank, const TArray<float>& UnlockLiberationThresholds)
{
	if (UnlockRank <= 0)
	{
		return 0.f;
	}

	// The rules count thresholds in order and stop at the first one not reached, so a later node needs all earlier ones.
	if (UnlockRank > UnlockLiberationThresholds.Num())
	{
		// Past the configured thresholds only a fully liberated country opens the node.
		return 1.f;
	}

	float LiberationNeeded = 0.f;
	for (int32 ThresholdIndex = 0; ThresholdIndex < UnlockRank; ++ThresholdIndex)
	{
		LiberationNeeded = FMath::Max(LiberationNeeded, UnlockLiberationThresholds[ThresholdIndex]);
	}
	return FMath::Clamp(LiberationNeeded, 0.f, 1.f);
}

FText WorldBuildSiteHelpers::GetLockedReasonText(const int32 UnlockRank, const TArray<float>& UnlockLiberationThresholds,
                                                 const float CurrentLiberation)
{
	using namespace WorldBuildSiteHelpersPrivate;
	if (UnlockRank <= 0)
	{
		return LOCTEXT("LockedFirstNode", "Opens when the first target in this country is cleared");
	}

	const int32 PercentNeeded = FMath::RoundToInt(
		GetUnlockLiberation(UnlockRank, UnlockLiberationThresholds) * PercentScale);
	const int32 PercentNow = FMath::RoundToInt(FMath::Clamp(CurrentLiberation, 0.f, 1.f) * PercentScale);
	return FText::Format(LOCTEXT("LockedAtLiberation", "Opens at {0} % liberated \u00B7 now {1} %"),
	                     FText::AsNumber(PercentNeeded), FText::AsNumber(PercentNow));
}

FText WorldBuildSiteHelpers::GetCountryDisplayName(const FName CountryId)
{
	const UWorldCampaignMapContentSettings* const MapContentSettings = UWorldCampaignMapContentSettings::Get();
	if (MapContentSettings == nullptr)
	{
		return FText::FromName(CountryId);
	}

	for (const FWorldCountryDefinition& CountryDefinition : MapContentSettings->Countries)
	{
		if (CountryDefinition.CountryId == CountryId && not CountryDefinition.DisplayName.IsEmpty())
		{
			return CountryDefinition.DisplayName;
		}
	}
	return FText::FromName(CountryId);
}

FText WorldBuildSiteHelpers::GetSiteTitle(const FText& CountryName, const int32 SiteNumber)
{
	return FText::Format(LOCTEXT("SiteTitle", "BUILD SITE \u00B7 {0} {1}"), CountryName.ToUpper(),
	                     FText::FromString(FString::Printf(TEXT("%02d"), SiteNumber)));
}

float WorldBuildSiteHelpers::GetCardRowWidth(const int32 CardCount, const float CardWidth, const float CardGap)
{
	if (CardCount <= 0)
	{
		return 0.f;
	}
	return CardCount * CardWidth + (CardCount - 1) * CardGap;
}

FVector2D WorldBuildSiteHelpers::GetPanelTopLeft(const FVector2D& Anchor, const FVector2D& PanelSize,
                                                 const FVector2D& ViewSize, const float GapToAnchor,
                                                 const float ScreenMargin, const float TopBarHeight)
{
	const double MinTop = FMath::Max(0.f, TopBarHeight) + ScreenMargin;
	const double MaxLeft = FMath::Max<double>(ScreenMargin, ViewSize.X - PanelSize.X - ScreenMargin);
	const double MaxTop = FMath::Max<double>(MinTop, ViewSize.Y - PanelSize.Y - ScreenMargin);
	const double Left = FMath::Clamp<double>(Anchor.X - PanelSize.X * 0.5, ScreenMargin, MaxLeft);

	const double TopAbove = Anchor.Y - GapToAnchor - PanelSize.Y;
	const double TopWanted = TopAbove >= MinTop ? TopAbove : Anchor.Y + GapToAnchor;
	return FVector2D(Left, FMath::Clamp<double>(TopWanted, MinTop, MaxTop));
}

FWorldBuildSiteStem WorldBuildSiteHelpers::GetStemPlacement(const FVector2D& Anchor, const FVector2D& PanelTopLeft,
                                                            const FVector2D& PanelSize, const float CornerInset,
                                                            const float AnchorClearance)
{
	FWorldBuildSiteStem Stem;
	const FVector2D PanelBottomRight = PanelTopLeft + PanelSize;
	const bool bAnchorIsBelow = Anchor.Y > PanelBottomRight.Y;
	const bool bAnchorIsAbove = Anchor.Y < PanelTopLeft.Y;
	if (not bAnchorIsBelow && not bAnchorIsAbove)
	{
		// The site is beside or under the panel: there is no edge to hang the line from.
		return Stem;
	}

	// A panel narrower than both insets has no straight part of its edge left; the line then leaves its middle.
	const double MinStartX = PanelTopLeft.X + CornerInset;
	const double MaxStartX = PanelBottomRight.X - CornerInset;
	const double StartX = MinStartX <= MaxStartX
		                      ? FMath::Clamp<double>(Anchor.X, MinStartX, MaxStartX)
		                      : (PanelTopLeft.X + PanelBottomRight.X) * 0.5;
	Stem.Start = FVector2D(StartX, bAnchorIsBelow ? PanelBottomRight.Y : PanelTopLeft.Y);

	const FVector2D StartToAnchor = Anchor - Stem.Start;
	const double Length = StartToAnchor.Size() - AnchorClearance;
	if (Length <= 0.)
	{
		return Stem;
	}

	Stem.bIsShown = true;
	Stem.Length = static_cast<float>(Length);
	// Slate turns clockwise on screen: a line hanging down (0, 1) turned by an angle A points along (-sin A, cos A).
	Stem.AngleDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-StartToAnchor.X, StartToAnchor.Y)));
	return Stem;
}

#undef LOCTEXT_NAMESPACE
