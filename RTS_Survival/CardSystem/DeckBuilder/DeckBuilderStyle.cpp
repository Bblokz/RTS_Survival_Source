// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DeckBuilderStyle.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace DeckBuilderStylePrivate
{
	const TCHAR* const CardTablePath =
		TEXT("/Game/RTS_Survival/Blueprints/CardSystem/ProfileCardUI/Cards/DT_RTSCards.DT_RTSCards");
	// The art imported for the deck builder (Source/Tools/DeckBuilder/import_deck_builder_art.py).
	const TCHAR* const DeckArtFolder = TEXT("/Game/RTS_Survival/Textures/GameUI/DeckBuilder/");
	// The hangar variant screen's slice textures.
	const TCHAR* const HangarSliceFolder = TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/");
	// Texture name suffix per ERTSCardRarity, in enum order.
	const TCHAR* const RarityNames[] = {
		TEXT("Common"), TEXT("UnCommon"), TEXT("Rare"), TEXT("VeryRare"), TEXT("Legendary"), TEXT("Mythic")
	};
	// Texture name suffix per EDeckSection, in enum order.
	const TCHAR* const SectionNames[] = {
		TEXT("Technologies"), TEXT("Barracks"), TEXT("MechanicalDepot"), TEXT("Forge"), TEXT("T2Factory"),
		TEXT("Airbase"), TEXT("Experimental")
	};

	// Drawn size of the fixed-ratio card art, in UI units at the 1920x1080 reference.
	const FVector2D CardFrameSize(240.f, 315.f);
	const FVector2D SpineFrameSize(118.f, 268.f);
	const FVector2D SectionIconSize(42.f, 42.f);
	const FVector2D SelectedMarkerSize(22.f, 22.f);
	// Slate draws the corners of a box brush at texture size, whatever the brush's image size says. The box parts
	// are therefore imported at one texel per UI unit: T_Deck_Part_* are half-size copies of the hangar screen's
	// slice textures and T_Deck_Panel is the card holder plate scaled so its corner is 26 units.
	// Slice margins in texels (left, top, right, bottom): half of UISkins/_generator/variant_selection_spec.md.
	const FMargin PanelSlice(26.f, 26.f, 26.f, 26.f);
	const FMargin VariantRowSlice(28.f, 28.f, 28.f, 28.f);
	const FMargin RailEntrySlice(20.f, 40.f, 28.f, 40.f);
	const FMargin IconWellSlice(10.f, 10.f, 10.f, 10.f);
	const FMargin ButtonSlice(32.f, 24.f, 32.f, 24.f);
	const FMargin TabSlice(12.f, 12.f, 12.f, 12.f);
	// What a part draws as when its texture is missing: a flat dark plate instead of a white box.
	const FLinearColor MissingArtTint(0.05f, 0.055f, 0.06f, 0.95f);

	TSoftObjectPtr<UTexture2D> MakeTextureReference(const TCHAR* Folder, const FString& AssetName)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString(Folder) + AssetName + TEXT(".") + AssetName));
	}

	UTexture2D* LoadTexture(const TSoftObjectPtr<UTexture2D>& Reference, TArray<FString>& OutMissingTextures)
	{
		UTexture2D* Texture = Reference.LoadSynchronous();
		if (not IsValid(Texture))
		{
			OutMissingTextures.Add(Reference.ToString());
		}
		return Texture;
	}

	FSlateBrush MakeImageBrush(UTexture2D* Texture, const FVector2D& DrawSize)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = DrawSize;
		if (not IsValid(Texture))
		{
			Brush.TintColor = FSlateColor(MissingArtTint);
			return Brush;
		}
		Brush.SetResourceObject(Texture);
		return Brush;
	}

	FSlateBrush MakeBoxBrush(UTexture2D* Texture, const FMargin& SliceTexels)
	{
		FSlateBrush Brush;
		if (not IsValid(Texture))
		{
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.TintColor = FSlateColor(MissingArtTint);
			return Brush;
		}
#if WITH_EDITOR
		// A texture that is still compiling reports a placeholder size, which would turn the slice into nonsense.
		Texture->BlockOnAnyAsyncBuild();
#endif
		const FVector2D TextureSize(Texture->GetSizeX(), Texture->GetSizeY());
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.ImageSize = TextureSize;
		Brush.Margin = FMargin(SliceTexels.Left / TextureSize.X, SliceTexels.Top / TextureSize.Y,
		                       SliceTexels.Right / TextureSize.X, SliceTexels.Bottom / TextureSize.Y);
		return Brush;
	}

	FButtonStyle MakeButtonStyle(const FSlateBrush& Normal, const FSlateBrush& Hovered, const FSlateBrush& Pressed)
	{
		FButtonStyle ButtonStyle;
		ButtonStyle.SetNormal(Normal);
		ButtonStyle.SetHovered(Hovered);
		ButtonStyle.SetPressed(Pressed);
		ButtonStyle.SetDisabled(Normal);
		ButtonStyle.SetNormalPadding(FMargin(0.f));
		ButtonStyle.SetPressedPadding(FMargin(0.f));
		return ButtonStyle;
	}

	FSlateFontInfo ResolveFont(const FSlateFontInfo& StyleFont, const TCHAR* FallbackTypeface)
	{
		constexpr int32 PlaceholderSize = 12;
		return StyleFont.HasValidFont() ? StyleFont : FCoreStyle::GetDefaultFontStyle(FallbackTypeface, PlaceholderSize);
	}
}

FDeckBuilderStyle::FDeckBuilderStyle()
{
	using namespace DeckBuilderStylePrivate;
	CardTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(CardTablePath));
	PanelTexture = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Panel"));
	VacantCardFrame = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_CardFrame_Vacant"));
	for (const TCHAR* const RarityName : RarityNames)
	{
		CardFrames.Add(MakeTextureReference(DeckArtFolder, FString(TEXT("T_Deck_CardFrame_")) + RarityName));
		SpineFrames.Add(MakeTextureReference(DeckArtFolder, FString(TEXT("T_Deck_Spine_")) + RarityName));
	}
	for (const TCHAR* const SectionName : SectionNames)
	{
		SectionIcons.Add(MakeTextureReference(DeckArtFolder, FString(TEXT("T_Deck_Section_")) + SectionName));
	}
	VariantRow = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Row_n"));
	VariantRowHovered = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Row_h"));
	VariantRowSelected = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Row_s"));
	IconWell = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Well"));
	SelectedMarker = MakeTextureReference(HangarSliceFolder, TEXT("T9_Custom_VariantSelectedMarker"));
	RailEntry = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Entry_n"));
	RailEntryHovered = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Entry_h"));
	RailEntrySelected = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Entry_s"));
	Button = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Button_n"));
	ButtonHovered = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Button_h"));
	ButtonPressed = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Button_p"));
	Tab = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Tab_n"));
	TabSelected = MakeTextureReference(DeckArtFolder, TEXT("T_Deck_Part_Tab_s"));
}

const FSlateBrush& FDeckBuilderLook::GetCardFrame(const ERTSCardRarity Rarity) const
{
	const int32 RarityIndex = static_cast<int32>(Rarity);
	return CardFrames.IsValidIndex(RarityIndex) ? CardFrames[RarityIndex] : VacantCardFrame;
}

const FSlateBrush& FDeckBuilderLook::GetSpineFrame(const ERTSCardRarity Rarity) const
{
	const int32 RarityIndex = static_cast<int32>(Rarity);
	return SpineFrames.IsValidIndex(RarityIndex) ? SpineFrames[RarityIndex] : VacantCardFrame;
}

const FSlateBrush& FDeckBuilderLook::GetSectionIcon(const EDeckSection Section) const
{
	const int32 SectionIndex = static_cast<int32>(Section);
	return SectionIcons.IsValidIndex(SectionIndex) ? SectionIcons[SectionIndex] : IconWell;
}

bool FDeckBuilderLook::GetIsDarkPaint(const ERTSCardRarity Rarity)
{
	return Rarity == ERTSCardRarity::Rarity_Legendary || Rarity == ERTSCardRarity::Rarity_Mythic;
}

void FDeckBuilderLook::BuildFromStyle(const FDeckBuilderStyle& InStyle, TArray<FString>& OutMissingTextures)
{
	using namespace DeckBuilderStylePrivate;
	Style = InStyle;
	HeadingFont = ResolveFont(InStyle.HeadingFont, TEXT("Bold"));
	BodyFont = ResolveFont(InStyle.BodyFont, TEXT("Regular"));
	Panel = MakeBoxBrush(LoadTexture(InStyle.PanelTexture, OutMissingTextures), PanelSlice);
	VacantCardFrame = MakeImageBrush(LoadTexture(InStyle.VacantCardFrame, OutMissingTextures), CardFrameSize);
	CardFrames.Reset();
	for (const TSoftObjectPtr<UTexture2D>& Frame : InStyle.CardFrames)
	{
		CardFrames.Add(MakeImageBrush(LoadTexture(Frame, OutMissingTextures), CardFrameSize));
	}
	SpineFrames.Reset();
	for (const TSoftObjectPtr<UTexture2D>& Frame : InStyle.SpineFrames)
	{
		SpineFrames.Add(MakeImageBrush(LoadTexture(Frame, OutMissingTextures), SpineFrameSize));
	}
	SectionIcons.Reset();
	for (const TSoftObjectPtr<UTexture2D>& Icon : InStyle.SectionIcons)
	{
		SectionIcons.Add(MakeImageBrush(LoadTexture(Icon, OutMissingTextures), SectionIconSize));
	}
	IconWell = MakeBoxBrush(LoadTexture(InStyle.IconWell, OutMissingTextures), IconWellSlice);
	SelectedMarker = MakeImageBrush(LoadTexture(InStyle.SelectedMarker, OutMissingTextures), SelectedMarkerSize);

	const FSlateBrush RowNormal = MakeBoxBrush(LoadTexture(InStyle.VariantRow, OutMissingTextures), VariantRowSlice);
	const FSlateBrush RowHovered = MakeBoxBrush(LoadTexture(InStyle.VariantRowHovered, OutMissingTextures),
	                                            VariantRowSlice);
	const FSlateBrush RowSelected = MakeBoxBrush(LoadTexture(InStyle.VariantRowSelected, OutMissingTextures),
	                                             VariantRowSlice);
	VariantRow = MakeButtonStyle(RowNormal, RowHovered, RowHovered);
	VariantRowSelected = MakeButtonStyle(RowSelected, RowSelected, RowSelected);

	const FSlateBrush EntryNormal = MakeBoxBrush(LoadTexture(InStyle.RailEntry, OutMissingTextures), RailEntrySlice);
	const FSlateBrush EntryHovered = MakeBoxBrush(LoadTexture(InStyle.RailEntryHovered, OutMissingTextures),
	                                              RailEntrySlice);
	const FSlateBrush EntrySelected = MakeBoxBrush(LoadTexture(InStyle.RailEntrySelected, OutMissingTextures),
	                                               RailEntrySlice);
	RailEntry = MakeButtonStyle(EntryNormal, EntryHovered, EntryHovered);
	RailEntrySelected = MakeButtonStyle(EntrySelected, EntrySelected, EntrySelected);

	Button = MakeButtonStyle(
		MakeBoxBrush(LoadTexture(InStyle.Button, OutMissingTextures), ButtonSlice),
		MakeBoxBrush(LoadTexture(InStyle.ButtonHovered, OutMissingTextures), ButtonSlice),
		MakeBoxBrush(LoadTexture(InStyle.ButtonPressed, OutMissingTextures), ButtonSlice));
	const FSlateBrush TabNormal = MakeBoxBrush(LoadTexture(InStyle.Tab, OutMissingTextures), TabSlice);
	const FSlateBrush TabOn = MakeBoxBrush(LoadTexture(InStyle.TabSelected, OutMissingTextures), TabSlice);
	Tab = MakeButtonStyle(TabNormal, TabOn, TabOn);
	TabSelected = MakeButtonStyle(TabOn, TabOn, TabOn);
}
