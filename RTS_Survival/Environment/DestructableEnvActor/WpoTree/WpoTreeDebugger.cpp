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
	TWeakObjectPtr<AWpoTree> WeakMyTree = M_MyTree;
	World->GetTimerManager().SetTimer(M_DebugTimerHandle, [this, WeakMyTree]()
	{
		if (not WeakMyTree.IsValid())
		{
			StopDebugging();
			return;
		}
		AWpoTree* StrongMyTree = WeakMyTree.Get();
		FString DebugString = "WPO disable at: " + FString::SanitizeFloat(M_WpoDisableDist);
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
			M_DebugLoc,
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
