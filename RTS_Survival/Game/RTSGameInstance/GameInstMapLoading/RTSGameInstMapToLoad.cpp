#include "RTSGameInstMapToLoad.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

FRTSGameInstMapToLoad::FRTSGameInstMapToLoad()
{
	NextMapToLoad = nullptr;
	bIsInitialized = false;
	
}

void FRTSGameInstMapToLoad::SetMapToLoad(TSoftObjectPtr<UWorld> NewMapToLoad)
{
	if(not NewMapToLoad.IsValid())
	{
		RTSFunctionLibrary::ReportError("FRTSGameInstance map to load: attempt to set with invalid map reference!");
		return;
	}
	NextMapToLoad = NewMapToLoad;
	bIsInitialized = true;
	
}

TSoftObjectPtr<UWorld> FRTSGameInstMapToLoad::GetMapToLoad() const
{
	if(not bIsInitialized)
	{
		RTSFunctionLibrary::ReportError("FRTSGameInstance map to load: attempt to get map to load when it was never set!");
		return nullptr;
	}
	if(not NextMapToLoad.IsValid())
	{
		RTSFunctionLibrary::ReportError("FRTSGameInstance map to load: attempt to get map to load when the set map reference is invalid!");
	}
	return NextMapToLoad;	
}

void FRTSGameInstMapToLoad::Reset()
{
	NextMapToLoad = nullptr;
	bIsInitialized = false;
}
