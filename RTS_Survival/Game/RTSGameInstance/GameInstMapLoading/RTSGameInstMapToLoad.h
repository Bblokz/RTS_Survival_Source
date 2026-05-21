#pragma once
#include "CoreMinimal.h"


#include "RTSGameInstMapToLoad.generated.h"

USTRUCT(BlueprintType)
struct FRTSGameInstMapToLoad
{
	GENERATED_BODY()
	FRTSGameInstMapToLoad();

public:
	void SetMapToLoad( TSoftObjectPtr<UWorld> NewMapToLoad);

	TSoftObjectPtr<UWorld> GetMapToLoad() const ;

	// Resets the stored map reference to null.
	void Reset();

private:
	// Keeps track of the next map to load.
	UPROPERTY()
	TSoftObjectPtr<UWorld> NextMapToLoad = nullptr;

	// To check for map validation; was the map pointer supposed to be set at all?
	UPROPERTY()
	bool bIsInitialized = false;
};
