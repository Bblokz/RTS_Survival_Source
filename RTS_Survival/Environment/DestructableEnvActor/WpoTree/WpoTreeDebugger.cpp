#include "WpoTreeDebugger.h"

#include "WpoTree.h"
#include "Kismet/GameplayStatics.h"

void FWpoTreeDebugger::StartDebugging(AWpoTree* WpoTree, const float WpoDisableDist)
{
	if (not IsValid(WpoTree))
	{
		return;
	}
	UWorld* World = WpoTree->GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_MyTree = WpoTree;
	M_WpoDisableDist = WpoDisableDist;
	M_DebugLoc = WpoTree->GetActorLocation() + FVector(0, 0, DebugZOffset);
	const TWeakObjectPtr<AWpoTree> WeakMyTree = M_MyTree;
	const float WpoDisableDistance = M_WpoDisableDist;
	const FVector DebugLocation = M_DebugLoc;
	World->GetTimerManager().SetTimer(M_DebugTimerHandle, [WeakMyTree, WpoDisableDistance, DebugLocation]()
	{
		if (not WeakMyTree.IsValid())
		{
			return;
		}
		AWpoTree* StrongMyTree = WeakMyTree.Get();
		FString DebugString = "WPO disable at: " + FString::SanitizeFloat(WpoDisableDistance);
		// get distance from player camera.
		APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(StrongMyTree, 0);
		if(not CameraManager)
		{
			return;
		}
		const float distance = FVector::Distance(CameraManager->GetCameraLocation(), StrongMyTree->GetActorLocation());
		DebugString += "\nDistance to camera: " + FString::SanitizeFloat(distance);
		
		DrawDebugString(
			StrongMyTree->GetWorld(),
			DebugLocation,
			DebugString,
			nullptr,
			FColor::White,
			0.5f,
			false
		);
	}, 0.5f, true);
}

void FWpoTreeDebugger::StopDebugging()
{
	UWorld* World = M_MyTree.IsValid() ? M_MyTree->GetWorld() : nullptr;
	if (not World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_DebugTimerHandle);
}
