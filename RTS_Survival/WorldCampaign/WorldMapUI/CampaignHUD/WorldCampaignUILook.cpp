// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"

#include "Engine/Texture2D.h"

namespace WorldCampaignUILookPrivate
{
	const TCHAR* const RadixiteIconPath = TEXT("/Game/RTS_Survival/Textures/Resources/T_Radixite_Icon.T_Radixite_Icon");
	const TCHAR* const BlueprintIconPath =
		TEXT("/Game/RTS_Survival/Textures/Resources/T_ConstructionBlueprint_Icon.T_ConstructionBlueprint_Icon");
	const FVector2D ResourceIconSize(24.f, 24.f);
	const TCHAR* const SkinSliceFolder = TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/");
	const TCHAR* const MetalCellName = TEXT("T9_Custom_ResourceCounter");
	// The campaign's own art (Source/Tools/WorldCampaign/make_campaign_hud_art.py and import_campaign_hud_art.py).
	const TCHAR* const CampaignArtFolder = TEXT("/Game/RTS_Survival/Textures/GameUI/WorldCampaign/");
	const TCHAR* const BoltedPanelName = TEXT("T_Campaign_BoltedPanel");
	const TCHAR* const RoundBezelName = TEXT("T_Campaign_GlassBezel");
	const TCHAR* const GlassDomeName = TEXT("T_Campaign_GlassDome");
	// Slice margins in texels, from the skin's slices.json.
	const FMargin MetalCellSlice(12.f, 12.f, 12.f, 12.f);
	const FMargin BoltedPanelSlice(37.f, 37.f, 37.f, 37.f);
	const FVector2D RoundBezelSize(104.f, 104.f);
	const FVector2D GlassDomeSize(74.f, 74.f);

	UTexture2D* LoadSkinPart(const TCHAR* Folder, const TCHAR* Name, TArray<FString>& OutMissingTextures)
	{
		const FString Path = FString(Folder) + Name + TEXT(".") + Name;
		UTexture2D* Texture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous();
		if (not IsValid(Texture))
		{
			OutMissingTextures.Add(Path);
		}
		return Texture;
	}
	// What an icon draws as when its texture is missing: a flat dark plate instead of a white box.
	const FLinearColor MissingArtTint(0.05f, 0.055f, 0.06f, 0.95f);

	UTexture2D* LoadIcon(const TCHAR* Path, TArray<FString>& OutMissingTextures)
	{
		UTexture2D* Texture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous();
		if (not IsValid(Texture))
		{
			OutMissingTextures.Add(Path);
		}
		return Texture;
	}
}

void FWorldCampaignUILook::Build(TArray<FString>& OutMissingTextures)
{
	using namespace WorldCampaignUILookPrivate;
	Deck.BuildFromStyle(FDeckBuilderStyle(), OutMissingTextures);
	RadixiteIcon = MakeIconBrush(LoadIcon(RadixiteIconPath, OutMissingTextures), ResourceIconSize);
	BlueprintIcon = MakeIconBrush(LoadIcon(BlueprintIconPath, OutMissingTextures), ResourceIconSize);
	MetalCell = MakeBoxBrush(LoadSkinPart(SkinSliceFolder, MetalCellName, OutMissingTextures), MetalCellSlice);
	BoltedPanel = MakeBoxBrush(LoadSkinPart(CampaignArtFolder, BoltedPanelName, OutMissingTextures), BoltedPanelSlice);
	RoundBezel = MakeIconBrush(LoadSkinPart(CampaignArtFolder, RoundBezelName, OutMissingTextures), RoundBezelSize);
	GlassDome = MakeIconBrush(LoadSkinPart(CampaignArtFolder, GlassDomeName, OutMissingTextures), GlassDomeSize);
}

FSlateBrush FWorldCampaignUILook::MakeBoxBrush(UTexture2D* Texture, const FMargin& SliceTexels)
{
	FSlateBrush Brush;
	if (not IsValid(Texture))
	{
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(WorldCampaignUILookPrivate::MissingArtTint);
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

FSlateBrush FWorldCampaignUILook::MakeIconBrush(UTexture2D* Texture, const FVector2D& DrawSize)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = DrawSize;
	if (not IsValid(Texture))
	{
		Brush.TintColor = FSlateColor(WorldCampaignUILookPrivate::MissingArtTint);
		return Brush;
	}

	Brush.SetResourceObject(Texture);
	return Brush;
}

FSlateBrush FWorldCampaignUILook::MakeFlatBrush(const FLinearColor& Color)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.TintColor = FSlateColor(Color);
	return Brush;
}
