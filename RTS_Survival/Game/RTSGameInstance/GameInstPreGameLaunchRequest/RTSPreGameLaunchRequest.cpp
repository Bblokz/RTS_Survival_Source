#include "RTSPreGameLaunchRequest.h"

FRTSPreGameLaunchRequest::FRTSPreGameLaunchRequest(
	const ERTSPreGameSetupFlow NewSetupFlow,
	const TSoftObjectPtr<UWorld> NewDestinationMap)
	: SetupFlow(NewSetupFlow),
	  DestinationMap(NewDestinationMap)
{
}

bool FRTSPreGameLaunchRequest::GetIsValid() const
{
	return SetupFlow != ERTSPreGameSetupFlow::NotInitialised && not DestinationMap.IsNull();
}

void FRTSPreGameLaunchRequest::Reset()
{
	SetupFlow = ERTSPreGameSetupFlow::NotInitialised;
	DestinationMap.Reset();
}
