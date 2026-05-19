#pragma once
#include "CoreMinimal.h"


#include "RTSGameInstMapToLoad.generated.h"

USTRUCT(BlueprintType)
struct FRTSGameInstMapToLoad
{
	GENERATED_BODY()
	FRTSGameInstMapToLoad();

public:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetMapToLoad( TSoftObjectPtr<UWorld> NewMapToLoad);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	TSoftObjectPtr<UWorld> GetMapToLoad() const ;

	// Resets the stored map reference to null.
	void Reset();

private:
	// Keeps track of the next map to load.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSoftObjectPtr<UWorld> NextMapToLoad = nullptr;

	// To check for map validation; was the map pointer supposed to be set at all?
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bIsInitialized = false;
};
