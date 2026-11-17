// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

#include "GMRRunner.h"

DEFINE_LOG_CATEGORY_STATIC(LogGMRModule, Log, All);

/**
 * @brief Loaded on demand by the render launcher (-ExecCmds="Module Load RTS_GameMechanicsRecs"), after the
 * startup map is up. With a -GMRJob=<file> argument it hands the job to a rooted runner; without one it does nothing.
 */
class FGMRModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FString JobPath;
		if (not FParse::Value(FCommandLine::Get(), TEXT("GMRJob="), JobPath))
		{
			UE_LOG(LogGMRModule, Display, TEXT("GMR_MODULE_LOADED without -GMRJob, idle"));
			return;
		}
		M_Runner = NewObject<UGMRRunner>(GetTransientPackage(), TEXT("GMRRunner"));
		M_Runner->AddToRoot();
		M_Runner->StartJob(JobPath.TrimQuotes());
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
	UGMRRunner* M_Runner = nullptr;
};

IMPLEMENT_MODULE(FGMRModule, RTS_GameMechanicsRecs)
