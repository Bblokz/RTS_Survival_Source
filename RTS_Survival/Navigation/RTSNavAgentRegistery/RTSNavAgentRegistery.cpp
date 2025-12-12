#include "RTSNavAgentRegistery.h"

#include "NavigationSystem.h"
#include "NavigationData.h"

const TArray<FNavDataConfig>& URTSNavAgentRegistry::GetEmptyAgents()
{
	static const TArray<FNavDataConfig> Empty;
	return Empty;
}

const TArray<FNavDataConfig>& URTSNavAgentRegistry::GetAllAgentConfigs(UWorld* World) const
{
	if (!World)
	{
		RTSFunctionLibrary::ReportError(TEXT("GetAllAgentConfigs: World is null."));
		return GetEmptyAgents();
	}
	const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		RTSFunctionLibrary::ReportError(TEXT("GetAllAgentConfigs: UNavigationSystemV1::GetCurrent returned null."));
		return GetEmptyAgents();
	}
	// World nav singleton exposes SupportedAgents (FNavDataConfig array).
	return NavSys->GetSupportedAgents(); 
}

const FNavDataConfig* URTSNavAgentRegistry::FindAgentConfig(UWorld* World, ERTSNavAgents Agent) const
{
	const FString AgentNameStr = Global_GetRTSNavAgentName(Agent);
	if (Agent == ERTSNavAgents::NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("FindAgentConfig called with RTSNavAgents::NONE."));
		return nullptr;
	}

	const TArray<FNavDataConfig>& Agents = GetAllAgentConfigs(World);
	if (Agents.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("FindAgentConfig: No Supported Agents are configured in Project Settings."));
		return nullptr;
	}

	const FName TargetName(*AgentNameStr);
	const FNavDataConfig* Found = Agents.FindByPredicate([&](const FNavDataConfig& C)
	{
		return C.Name == TargetName;
	});

	if (!Found)
	{
		RTSFunctionLibrary::ReportError(TEXT("FindAgentConfig: Could not find agent named: ") + AgentNameStr);
	}
	return Found; // pointer to element inside NavSys-owned array
}

const ANavigationData* URTSNavAgentRegistry::GetNavDataForAgent(
	UWorld* World,
	const FNavDataConfig& AgentCfg,
	const FVector* NearLocation,
	const FVector* Extent) const
{
	if (!World)
	{
		RTSFunctionLibrary::ReportError(TEXT("GetNavDataForAgent: World is null."));
		return nullptr;
	}
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		RTSFunctionLibrary::ReportError(TEXT("GetNavDataForAgent: UNavigationSystemV1::GetCurrent returned null."));
		return nullptr;
	}

	// FNavDataConfig derives from FNavAgentProperties
	const FNavAgentProperties& Props = static_cast<const FNavAgentProperties&>(AgentCfg);

	const FVector Loc = NearLocation ? *NearLocation : FVector::ZeroVector;
	const FVector Ext = Extent      ? *Extent      : FVector::ZeroVector;

	// Resolve the exact ANavigationData (e.g., ARecastNavMesh) for these properties. :contentReference[oaicite:4]{index=4}
	const ANavigationData* NavData = NavSys->GetNavDataForProps(Props, Loc, Ext);
	if (!NavData)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("GetNavDataForAgent: No NavData found for agent '%s'."), *AgentCfg.Name.ToString()));
	}
	return NavData;
}

FName URTSNavAgentRegistry::GetAgentNameForProps(UWorld* World, const FNavAgentProperties& Props, const float Tolerance) const
{
	const TArray<FNavDataConfig>& Agents = GetAllAgentConfigs(World);
	if (Agents.Num() == 0) return NAME_None;

	for (const FNavDataConfig& Cfg : Agents)
	{
		const FNavAgentProperties& AC = static_cast<const FNavAgentProperties&>(Cfg);

		const bool bRadiusOK = FMath::Abs(AC.AgentRadius - Props.AgentRadius) <= Tolerance;
		const bool bHeightOK = FMath::Abs(AC.AgentHeight - Props.AgentHeight) <= Tolerance;

		// If PreferredNavData is set on both, require a class match.
		bool bClassOK = true;
		if (!Props.PreferredNavData.IsNull() && !Cfg.PreferredNavData.IsNull())
		{
			bClassOK = (Props.PreferredNavData == Cfg.PreferredNavData);
		}

		if (bRadiusOK && bHeightOK && bClassOK)
		{
			return Cfg.Name;
		}
	}
	return NAME_None;
}
