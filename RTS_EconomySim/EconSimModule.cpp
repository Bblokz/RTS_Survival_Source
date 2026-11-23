// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

#include "EconSimDirector.h"

DEFINE_LOG_CATEGORY_STATIC(LogEconSimModule, Log, All);

namespace EconSimModulePrivate
{
	// The map loads before -ExecCmds runs; give its BeginPlay a moment before the director joins the world.
	constexpr int32 TicksBeforeSpawn = 30;
}

/**
 * @brief Loaded on demand by run_economy_sim.ps1 (-ExecCmds="Module Load RTS_EconomySim") once the startup map is up.
 * With -EconSim on the command line it spawns one AEconSimDirector into the game world; without it it does nothing.
 */
class FEconSimModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (not FParse::Param(FCommandLine::Get(), TEXT("EconSim")))
		{
			UE_LOG(LogEconSimModule, Display, TEXT("ECONSIM_MODULE_LOADED without -EconSim, idle"));
			return;
		}
		M_TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FEconSimModule::TickSpawnDirector), 0.f);
	}

	virtual void ShutdownModule() override
	{
		if (M_TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(M_TickerHandle);
			M_TickerHandle.Reset();
		}
	}

private:
	bool TickSpawnDirector(float /*DeltaSeconds*/)
	{
		UWorld* const GameWorld = FindGameWorld();
		if (GameWorld == nullptr || not GameWorld->HasBegunPlay())
		{
			return true;
		}
		if (++M_TicksWithWorld < EconSimModulePrivate::TicksBeforeSpawn)
		{
			return true;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AEconSimDirector* const Director = GameWorld->SpawnActor<AEconSimDirector>(
			AEconSimDirector::StaticClass(), FTransform::Identity, SpawnParameters);
		UE_LOG(LogEconSimModule, Display, TEXT("ECONSIM_DIRECTOR_SPAWNED map=%s ok=%d"), *GameWorld->GetMapName(),
		       Director != nullptr ? 1 : 0);
		M_TickerHandle.Reset();
		return false;
	}

	static UWorld* FindGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* const World = Context.World();
			if (World != nullptr && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
			{
				return World;
			}
		}
		return nullptr;
	}

	FTSTicker::FDelegateHandle M_TickerHandle;
	int32 M_TicksWithWorld = 0;
};

IMPLEMENT_MODULE(FEconSimModule, RTS_EconomySim)
