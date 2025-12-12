// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_Portrait.h"

#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Slate/SlateBrushAsset.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_Portrait::UpdatePortrait(const ERTSPortraitTypes NewType)
{
	if (NewType == ERTSPortraitTypes::None)
	{
		HidePortrait();
		return;
	}

	TSoftObjectPtr<USlateBrushAsset>* PortraitBrushPtr = PortraitBrushes.Find(NewType);
	if (PortraitBrushPtr == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::UpdatePortrait - No PortraitBrush configured for requested portrait type.");
		HidePortrait();
		return;
	}

	if (PortraitBrushPtr->IsNull())
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::UpdatePortrait - Portrait brush soft reference is null for requested portrait type.");
		HidePortrait();
		return;
	}

	// We will be showing this widget; opacity is controlled by the fade-in.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Brush already loaded → apply immediately and start fade-in.
	if (PortraitBrushPtr->IsValid())
	{
		USlateBrushAsset* LoadedAsset = PortraitBrushPtr->Get();
		OnBrushLoaded(LoadedAsset);
		return;
	}

	// Async load the brush.
	const FSoftObjectPath AssetPath = PortraitBrushPtr->ToSoftObjectPath();
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	TWeakObjectPtr<UW_Portrait> WeakThis(this);

	Streamable.RequestAsyncLoad(
		AssetPath,
		FStreamableDelegate::CreateLambda([WeakThis, AssetPath]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			UObject* ResolvedObject = AssetPath.ResolveObject();
			if (ResolvedObject == nullptr)
			{
				ResolvedObject = AssetPath.TryLoad();
			}

			USlateBrushAsset* LoadedAsset = Cast<USlateBrushAsset>(ResolvedObject);
			WeakThis->OnBrushLoaded(LoadedAsset);
		}));
}

void UW_Portrait::HidePortrait()
{
	// Stop any active fade-in.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);
	}
	bM_IsFadingIn = false;
	M_CurrentFadeTimeSeconds = 0.0f;

	// Hide the widget and clear the brush.
	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Hidden);

	if (PortraitImage != nullptr)
	{
		FSlateBrush EmptyBrush;
		PortraitImage->SetBrush(EmptyBrush);
	}
}

void UW_Portrait::OnBrushLoaded(USlateBrushAsset* LoadedAsset)
{
	if (not IsValid(LoadedAsset))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to load portrait brush asset in UW_Portrait::OnBrushLoaded().");
		HidePortrait();
		return;
	}

	if (PortraitImage == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::OnBrushLoaded - PortraitImage is null.");
		return;
	}

	PortraitImage->SetBrush(LoadedAsset->Brush);
	StartFadeIn();
}

void UW_Portrait::StartFadeIn()
{
	// Reset fade parameters and ensure we are visible but fully transparent.
	M_CurrentFadeTimeSeconds = 0.0f;
	bM_IsFadingIn = true;

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.0f);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::StartFadeIn - World is null, cannot start fade timer.");
		SetRenderOpacity(1.0f);
		bM_IsFadingIn = false;
		return;
	}

	// Restart the timer if it was already running.
	World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);

	if (M_FadeInDurationSeconds <= 0.0f || M_FadeInTickIntervalSeconds <= 0.0f)
	{
		// Invalid fade parameters: just show instantly.
		SetRenderOpacity(1.0f);
		bM_IsFadingIn = false;
		return;
	}

	World->GetTimerManager().SetTimer(
		M_FadeInTimerHandle,
		this,
		&UW_Portrait::TickFadeIn,
		M_FadeInTickIntervalSeconds,
		true);
}

void UW_Portrait::TickFadeIn()
{
	if (not bM_IsFadingIn)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);
		}
		return;
	}

	M_CurrentFadeTimeSeconds += M_FadeInTickIntervalSeconds;

	const float Alpha = FMath::Clamp(
		M_CurrentFadeTimeSeconds / M_FadeInDurationSeconds,
		0.0f,
		1.0f);

	SetRenderOpacity(Alpha);

	if (Alpha >= 1.0f)
	{
		bM_IsFadingIn = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);
		}
	}
}
