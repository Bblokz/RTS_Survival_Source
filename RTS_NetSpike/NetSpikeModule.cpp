// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

#include "NetSpikeRunner.h"
#include "NetSpikeShared.h"

/**
 * @brief Loaded on demand by the spike's launch script (-ExecCmds="Module Load RTS_NetSpike"), after the startup
 * map is up. With -NetSpikeRole=Host or -NetSpikeRole=Client it hands the process to a rooted runner; without one
 * it does nothing.
 */
class FNetSpikeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FString RoleText;
		if (not FParse::Value(FCommandLine::Get(), TEXT("NetSpikeRole="), RoleText))
		{
			UE_LOG(LogRTSNetSpike, Display, TEXT("RTS_NETSPIKE module loaded without -NetSpikeRole, idle"));
			return;
		}
		const bool bIsHost = RoleText.Equals(TEXT("Host"), ESearchCase::IgnoreCase);
		if (not bIsHost && not RoleText.Equals(TEXT("Client"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogRTSNetSpike, Error, TEXT("RTS_NETSPIKE unknown role %s, idle"), *RoleText);
			return;
		}
		M_Runner = NewObject<UNetSpikeRunner>(GetTransientPackage(), TEXT("NetSpikeRunner"));
		M_Runner->AddToRoot();
		M_Runner->Start(bIsHost ? ENetSpikeRole::Host : ENetSpikeRole::Client);
	}

	virtual void ShutdownModule() override
	{
		if (M_Runner != nullptr && M_Runner->IsValidLowLevelFast())
		{
			M_Runner->RemoveFromRoot();
		}
		M_Runner = nullptr;
	}

private:
	// Rooted for the life of the process; the module owns no other state.
	UNetSpikeRunner* M_Runner = nullptr;
};

IMPLEMENT_MODULE(FNetSpikeModule, RTS_NetSpike)
