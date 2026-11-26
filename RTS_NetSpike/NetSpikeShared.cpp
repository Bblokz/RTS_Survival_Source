// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "NetSpikeShared.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY(LogRTSNetSpike);

namespace NetSpikeSharedPrivate
{
	bool bIsClientWorkDone = false;
}

void NetSpike::MarkClientWorkDone()
{
	NetSpikeSharedPrivate::bIsClientWorkDone = true;
}

bool NetSpike::GetIsClientWorkDone()
{
	return NetSpikeSharedPrivate::bIsClientWorkDone;
}

UWorld* NetSpike::GetGameWorld()
{
	if (GEngine == nullptr)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World() != nullptr)
		{
			return Context.World();
		}
	}
	return nullptr;
}

void NetSpike::LogRung(const TCHAR* Side, const int32 Rung, const FString& Text)
{
	UE_LOG(LogRTSNetSpike, Display, TEXT("RTS_NETSPIKE %s rung=%d %s"), Side, Rung, *Text);
}

void NetSpike::RequestProcessExit(const int32 Status, const float DelaySeconds)
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Status](float)
	{
		FPlatformMisc::RequestExitWithStatus(false, static_cast<uint8>(Status));
		return false;
	}), DelaySeconds);
}
