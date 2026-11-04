// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CommanderSelectionData.h"

#include "Engine/Texture2D.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/CommanderSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"

namespace CommanderSelectionDataPrivate
{
	const FString AbilityEnumPrefix(TEXT("GA_"));
	const FString GermanAbilityPrefix(TEXT("Ger"));

	/** Readable name for an ability without a template: GA_GerKingTiger105Drop gives "King Tiger 105 Drop". */
	FText MakePlaceholderTitle(const EGlobalAbility Ability)
	{
		FString AbilityName = UEnum::GetValueAsName(Ability).ToString();
		const int32 ScopeEnd = AbilityName.Find(TEXT("::"));
		if (ScopeEnd != INDEX_NONE)
		{
			AbilityName.RightChopInline(ScopeEnd + 2);
		}
		AbilityName.RemoveFromStart(AbilityEnumPrefix);
		AbilityName.RemoveFromStart(GermanAbilityPrefix);
		const FString SpacedName = FName::NameToDisplayString(AbilityName, false);
		// NameToDisplayString leaves a number glued to the next word ("105Drop").
		FString PlaceholderTitle;
		for (int32 CharIndex = 0; CharIndex < SpacedName.Len(); ++CharIndex)
		{
			const bool bIsWordAfterNumber = CharIndex > 0 && FChar::IsDigit(SpacedName[CharIndex - 1])
				&& FChar::IsUpper(SpacedName[CharIndex]);
			if (bIsWordAfterNumber)
			{
				PlaceholderTitle.AppendChar(TEXT(' '));
			}
			PlaceholderTitle.AppendChar(SpacedName[CharIndex]);
		}
		return FText::FromString(PlaceholderTitle);
	}

	/** Template titles are rich text ("<Text_Title>Flare Curtain</>"); the rows draw them in the heading font. */
	FText StripRichTextTags(const FText& RichText)
	{
		const FString Source = RichText.ToString();
		FString PlainText;
		PlainText.Reserve(Source.Len());
		bool bIsInsideTag = false;
		for (const TCHAR Character : Source)
		{
			if (Character == TEXT('<'))
			{
				bIsInsideTag = true;
				continue;
			}
			if (Character == TEXT('>') && bIsInsideTag)
			{
				bIsInsideTag = false;
				continue;
			}
			if (not bIsInsideTag)
			{
				PlainText.AppendChar(Character);
			}
		}
		return FText::FromString(PlainText.TrimStartAndEnd());
	}

	// Templates are only read: FindAbilityTemplate gives null without an error for abilities still being authored.
	FCommanderSelectionAbilitySlot MakeAbilitySlot(const EGlobalAbility Ability,
	                                               const UGlobalAbilitySetDataAsset* AbilityDataAsset)
	{
		FCommanderSelectionAbilitySlot AbilitySlot;
		AbilitySlot.Ability = Ability;
		const UGlobalAbility* const AbilityTemplate = IsValid(AbilityDataAsset)
			                                              ? AbilityDataAsset->FindAbilityTemplate(Ability)
			                                              : nullptr;
		if (not IsValid(AbilityTemplate))
		{
			// No description: the grey "?" icon and badge already mark the ability as not ready.
			AbilitySlot.Title = MakePlaceholderTitle(Ability);
			return AbilitySlot;
		}
		AbilitySlot.bHasTemplate = true;
		AbilitySlot.Tier = AbilityTemplate->GetAbilityTier();
		AbilitySlot.Title = StripRichTextTags(AbilityTemplate->GetUISettings().Title);
		AbilitySlot.Description = AbilityTemplate->GetUISettings().Description;
		AbilitySlot.Icon = AbilityTemplate->GetUISettings().AbilityIcon;
		return AbilitySlot;
	}

	FCommanderSelectionEntryData MakeEntry(const ERTSCommander Commander, const FRTSCommanderSettings& Settings,
	                                       const UGlobalAbilitySetDataAsset* AbilityDataAsset)
	{
		FCommanderSelectionEntryData Entry;
		Entry.Commander = Commander;
		Entry.Title = Settings.Title.IsEmpty() ? UEnum::GetDisplayValueAsText(Commander) : Settings.Title;
		Entry.Description = Settings.Description;
		Entry.Portrait = Settings.Portrait;
		for (const EGlobalAbility Ability : Settings.GlobalAbilities)
		{
			Entry.AbilitySlots.Add(MakeAbilitySlot(Ability, AbilityDataAsset));
		}
		return Entry;
	}
}

TArray<FCommanderSelectionEntryData> CommanderSelectionData::BuildCommanderEntries(
	const UGlobalAbilitySetDataAsset* AbilityDataAsset)
{
	using namespace CommanderSelectionDataPrivate;
	TArray<FCommanderSelectionEntryData> Entries;
	const UCommanderSettings* const CommanderSettings = UCommanderSettings::Get();
	const UEnum* const CommanderEnum = StaticEnum<ERTSCommander>();
	if (not IsValid(CommanderSettings) || not IsValid(CommanderEnum))
	{
		return Entries;
	}
	// NumEnums includes the generated _MAX entry.
	const int32 CommanderCount = CommanderEnum->NumEnums() - 1;
	for (int32 EnumIndex = 0; EnumIndex < CommanderCount; ++EnumIndex)
	{
		const ERTSCommander Commander = static_cast<ERTSCommander>(CommanderEnum->GetValueByIndex(EnumIndex));
		const FRTSCommanderSettings* const Settings = CommanderSettings->CommanderSettingsByType.Find(Commander);
		if (Commander == ERTSCommander::NoCommander || Settings == nullptr)
		{
			continue;
		}
		Entries.Add(MakeEntry(Commander, *Settings, AbilityDataAsset));
	}
	return Entries;
}

FText CommanderSelectionData::GetTierNumeral(const EGlobalAbilityTier Tier)
{
	switch (Tier)
	{
	case EGlobalAbilityTier::TierI:
		return NSLOCTEXT("CommanderSelection", "TierI", "I");
	case EGlobalAbilityTier::TierII:
		return NSLOCTEXT("CommanderSelection", "TierII", "II");
	case EGlobalAbilityTier::TierIII:
		return NSLOCTEXT("CommanderSelection", "TierIII", "III");
	case EGlobalAbilityTier::TierIV:
		return NSLOCTEXT("CommanderSelection", "TierIV", "IV");
	default:
		return NSLOCTEXT("CommanderSelection", "TierUnknown", "?");
	}
}
