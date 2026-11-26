// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AsyncShowInfoTakeover.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InfoTakeoverSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UAsyncShowInfoTakeoverBase::Activate()
{
	UInfoTakeoverSubsystem* const InfoTakeoverSubsystem = FindInfoTakeoverSubsystem();
	if (InfoTakeoverSubsystem == nullptr)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("%s: no info takeover subsystem in this world; the mission continues without the takeover."),
			*GetNodeName()));
		Finish(true, false);
		return;
	}
	M_InfoTakeoverSubsystem = InfoTakeoverSubsystem;
	// Bound before queueing: the subsystem can open and finish the request inside the queue call.
	M_RequestFinishedHandle = InfoTakeoverSubsystem->OnTakeoverRequestFinished.AddUObject(
		this, &UAsyncShowInfoTakeoverBase::HandleRequestFinished);
	M_RequestId = QueueTakeover(*InfoTakeoverSubsystem);
	if (M_RequestId == INDEX_NONE && not bM_HasFinished)
	{
		// Nothing was queued; the mission continues at once.
		Finish(true, false);
	}
}

void UAsyncShowInfoTakeoverBase::HandleRequestFinished(const int32 RequestId, const EInfoTakeoverRequestResult Result)
{
	if (RequestId != M_RequestId || bM_HasFinished)
	{
		return;
	}
	switch (Result)
	{
	case EInfoTakeoverRequestResult::Closed:
		Finish(true, true);
		break;
	case EInfoTakeoverRequestResult::Skipped:
		Finish(true, false);
		break;
	case EInfoTakeoverRequestResult::Cancelled:
		// The world is ending; the mission must not continue into it.
		Finish(false, false);
		break;
	}
}

void UAsyncShowInfoTakeoverBase::Finish(const bool bBroadcastClosed, const bool bWasShown)
{
	if (bM_HasFinished)
	{
		return;
	}
	bM_HasFinished = true;
	UnbindFromSubsystem();
	if (bBroadcastClosed)
	{
		OnClosed.Broadcast(bWasShown);
	}
	SetReadyToDestroy();
}

void UAsyncShowInfoTakeoverBase::UnbindFromSubsystem()
{
	if (not M_RequestFinishedHandle.IsValid())
	{
		return;
	}
	if (M_InfoTakeoverSubsystem.IsValid())
	{
		M_InfoTakeoverSubsystem->OnTakeoverRequestFinished.Remove(M_RequestFinishedHandle);
	}
	M_RequestFinishedHandle.Reset();
}

UInfoTakeoverSubsystem* UAsyncShowInfoTakeoverBase::FindInfoTakeoverSubsystem() const
{
	if (not M_WorldContextObject.IsValid() || GEngine == nullptr)
	{
		return nullptr;
	}
	const UWorld* const World = GEngine->GetWorldFromContextObject(M_WorldContextObject.Get(),
	                                                               EGetWorldErrorMode::LogAndReturnNull);
	return World != nullptr ? World->GetSubsystem<UInfoTakeoverSubsystem>() : nullptr;
}

UAsyncShowCameraControlsTakeover* UAsyncShowCameraControlsTakeover::ShowCameraControlsTakeover(
	UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowCameraControlsTakeover>(WorldContextObject);
}

int32 UAsyncShowCameraControlsTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueCameraControlsTakeover();
}

UAsyncShowCoverTakeover* UAsyncShowCoverTakeover::ShowCoverTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowCoverTakeover>(WorldContextObject);
}

int32 UAsyncShowCoverTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueCoverTakeover();
}

UAsyncShowSpacedArmourTakeover* UAsyncShowSpacedArmourTakeover::ShowSpacedArmourTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowSpacedArmourTakeover>(WorldContextObject);
}

int32 UAsyncShowSpacedArmourTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueSpacedArmourTakeover();
}

UAsyncShowNomadicConversionTakeover* UAsyncShowNomadicConversionTakeover::ShowNomadicConversionTakeover(
	UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowNomadicConversionTakeover>(WorldContextObject);
}

int32 UAsyncShowNomadicConversionTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueNomadicConversionTakeover();
}

UAsyncShowBxpPackUpTakeover* UAsyncShowBxpPackUpTakeover::ShowBxpPackUpTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowBxpPackUpTakeover>(WorldContextObject);
}

int32 UAsyncShowBxpPackUpTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueBxpPackUpTakeover();
}

UAsyncShowVulturesAtTakeover* UAsyncShowVulturesAtTakeover::ShowVulturesAtTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowVulturesAtTakeover>(WorldContextObject);
}

int32 UAsyncShowVulturesAtTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueVulturesAtTakeover();
}

UAsyncShowRattleFeedTakeover* UAsyncShowRattleFeedTakeover::ShowRattleFeedTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowRattleFeedTakeover>(WorldContextObject);
}

int32 UAsyncShowRattleFeedTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueRattleFeedTakeover();
}

UAsyncShowWespeBarrageTakeover* UAsyncShowWespeBarrageTakeover::ShowWespeBarrageTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowWespeBarrageTakeover>(WorldContextObject);
}

int32 UAsyncShowWespeBarrageTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueWespeBarrageTakeover();
}

UAsyncShowDirectionFindingTakeover* UAsyncShowDirectionFindingTakeover::ShowDirectionFindingTakeover(
	UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowDirectionFindingTakeover>(WorldContextObject);
}

int32 UAsyncShowDirectionFindingTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueDirectionFindingTakeover();
}

UAsyncShowDigInTakeover* UAsyncShowDigInTakeover::ShowDigInTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowDigInTakeover>(WorldContextObject);
}

int32 UAsyncShowDigInTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueDigInTakeover();
}

UAsyncShowWallBreachTakeover* UAsyncShowWallBreachTakeover::ShowWallBreachTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowWallBreachTakeover>(WorldContextObject);
}

int32 UAsyncShowWallBreachTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueWallBreachTakeover();
}

UAsyncShowAdrenalineShotTakeover* UAsyncShowAdrenalineShotTakeover::ShowAdrenalineShotTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowAdrenalineShotTakeover>(WorldContextObject);
}

int32 UAsyncShowAdrenalineShotTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueAdrenalineShotTakeover();
}

UAsyncShowCapturingTrucksTakeover* UAsyncShowCapturingTrucksTakeover::ShowCapturingTrucksTakeover(
	UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowCapturingTrucksTakeover>(WorldContextObject);
}

int32 UAsyncShowCapturingTrucksTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueCapturingTrucksTakeover();
}

UAsyncShowDocumentTakeover* UAsyncShowDocumentTakeover::ShowDocumentTakeover(UObject* WorldContextObject,
                                                                             const FDocumentTakeoverContent& Document)
{
	UAsyncShowDocumentTakeover* const Node = CreateNode<UAsyncShowDocumentTakeover>(WorldContextObject);
	Node->M_Document = Document;
	return Node;
}

int32 UAsyncShowDocumentTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueDocumentTakeover(M_Document);
}

UAsyncShowOreManifestTakeover* UAsyncShowOreManifestTakeover::ShowOreManifestTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowOreManifestTakeover>(WorldContextObject);
}

int32 UAsyncShowOreManifestTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueDocumentTakeover(MakeOreManifest());
}

FDocumentTakeoverContent UAsyncShowOreManifestTakeover::MakeOreManifest()
{
	// The columns follow the convoy line-up (OreMountainsMission.md, "Convoy line-up" and "Flow"): fuel, the
	// weighbridge, the ammo split, the hunters, the disguised case convoy (5) and the heavy column to the knoll.
	// Lore rules: the sheet never says why Berlin, mentions no cryo and nothing about where Radixite comes from.
	FDocumentTakeoverContent Manifest;
	Manifest.KindLabel = NSLOCTEXT("OreManifestTakeover", "KindLabel", "RADIO OUTPOST  -  CAPTURED DOCUMENT");
	Manifest.Title = NSLOCTEXT("OreManifestTakeover", "Title", "Convoy Manifest");
	Manifest.Description = NSLOCTEXT("OreManifestTakeover", "Description",
	                                 "Found on the radio operator's desk, translated from the Russian. Most of it is "
	                                 "routine. One column is not.");
	Manifest.PaperHeading = NSLOCTEXT("OreManifestTakeover", "PaperHeading", "CONVOY MANIFEST");
	Manifest.PaperSubheading = NSLOCTEXT("OreManifestTakeover", "PaperSubheading",
	                                     "Wismut yard, valley road west. Columns for the western outposts.");
	const TArray<FText> RoutineColumns = {
		NSLOCTEXT("OreManifestTakeover", "Column1", "Column 1:  fuel. Light escort."),
		NSLOCTEXT("OreManifestTakeover", "Column2", "Column 2:  supply. Halt for inspection at the weighbridge."),
		NSLOCTEXT("OreManifestTakeover", "Column3", "Column 3:  shells. Ammunition trucks turn off for the battery."),
		NSLOCTEXT("OreManifestTakeover", "Column4", "Column 4:  supply, with rifle squads riding in the trucks."),
	};
	for (const FText& ColumnText : RoutineColumns)
	{
		FDocumentTakeoverLine& Line = Manifest.PaperLines.AddDefaulted_GetRef();
		Line.Text = ColumnText;
	}
	FDocumentTakeoverLine& SealedLine = Manifest.PaperLines.AddDefaulted_GetRef();
	SealedLine.Text = NSLOCTEXT("OreManifestTakeover", "Column5",
	                            "Column 5:  supply, and one armoured truck. Cargo SEALED. Do not inspect. Do not open. "
	                            "Destination: Berlin.");
	SealedLine.bIsMarked = true;
	FDocumentTakeoverLine& HeavyLine = Manifest.PaperLines.AddDefaulted_GetRef();
	HeavyLine.Text = NSLOCTEXT("OreManifestTakeover", "Column6", "Column 6:  heavy escort for the knoll.");
	Manifest.PaperFooter = NSLOCTEXT("OreManifestTakeover", "PaperFooter",
	                                 "Questions about column 5 are not to be sent over the radio.");
	Manifest.StampText = NSLOCTEXT("OreManifestTakeover", "Stamp", "BERLIN");
	Manifest.ArchiveKey = FName(TEXT("OreManifest"));
	return Manifest;
}
