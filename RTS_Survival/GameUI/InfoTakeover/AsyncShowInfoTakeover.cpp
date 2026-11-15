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
		// The player turned the info takeovers off.
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

UAsyncShowDigInTakeover* UAsyncShowDigInTakeover::ShowDigInTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowDigInTakeover>(WorldContextObject);
}

int32 UAsyncShowDigInTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueDigInTakeover();
}

UAsyncShowAdrenalineShotTakeover* UAsyncShowAdrenalineShotTakeover::ShowAdrenalineShotTakeover(UObject* WorldContextObject)
{
	return CreateNode<UAsyncShowAdrenalineShotTakeover>(WorldContextObject);
}

int32 UAsyncShowAdrenalineShotTakeover::QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
{
	return InfoTakeoverSubsystem.QueueAdrenalineShotTakeover();
}
