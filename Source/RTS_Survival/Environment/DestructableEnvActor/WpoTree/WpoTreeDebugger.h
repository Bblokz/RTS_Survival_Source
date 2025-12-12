#pragma once


#include "CoreMinimal.h"

#include "WpoTreeDebugger.generated.h"


class AWpoTree;

USTRUCT(Blueprintable)
struct FWpoTreeDebugger
{
	GENERATED_BODY()

	void StartDebugging(AWpoTree* WpoTree, const float WpoDisableDist);
	void StopDebugging();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debugging")
	float DebugZOffset = 300.f;
private:
	UPROPERTY()
	TWeakObjectPtr<AWpoTree> M_MyTree;

	UPROPERTY()
	FTimerHandle M_DebugTimerHandle;
	
	FVector M_DebugLoc = FVector::ZeroVector;
	float M_WpoDisableDist = 0.f;
	
};
