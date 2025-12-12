#include "RTSHidableInstancedStaticMeshComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void URTSHidableInstancedStaticMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URTSHidableInstancedStaticMeshComponent::PostInitProperties()
{
	Super::PostInitProperties();
	// initialize our hidden‐state map so that each instance starts as "visible."
	InitializeHiddenMap();
}

void URTSHidableInstancedStaticMeshComponent::InitializeHiddenMap()
{
	InstanceHiddenMap.Empty();

	const int32 Count = GetInstanceCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		InstanceHiddenMap.Add(Index, /*bHidden=*/ false);
	}
}

void URTSHidableInstancedStaticMeshComponent::UpdateHiddenMap()
{
	const int32 Count = GetInstanceCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!InstanceHiddenMap.Contains(Index))
		{
			InstanceHiddenMap.Add(Index, false);
		}
	}
}

void URTSHidableInstancedStaticMeshComponent::SetInstanceHidden(int32 InstanceIndex, bool bHide)
{
	// Validate that our map was initialized and contains this index
	if (!InstanceHiddenMap.Contains(InstanceIndex))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("SetInstanceHidden: Invalid or uninitialized InstanceIndex %d"), InstanceIndex)
		);
		return;
	}

	bool bCurrentlyHidden = InstanceHiddenMap[InstanceIndex];
	if (bHide)
	{
		if (bCurrentlyHidden)
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("Instance %d is already hidden."), InstanceIndex)
			);
			return;
		}
	}
	else
	{
		if (!bCurrentlyHidden)
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("Instance %d is not hidden."), InstanceIndex)
			);
			return;
		}
	}

	// Get the existing local‐space transform for this instance
	FTransform InstanceTransform;
	if (!GetInstanceTransform(InstanceIndex, InstanceTransform, /*bWorldSpace=*/ false))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("SetInstanceHidden: Failed to get transform for instance %d."), InstanceIndex)
		);
		return;
	}

	// Adjust the Z translation by ±4000
	FVector LocalLoc = InstanceTransform.GetLocation();
	const float OffsetZ = 4000.0f;
	LocalLoc.Z += (bHide ? -OffsetZ : +OffsetZ);
	InstanceTransform.SetLocation(LocalLoc);

	// Apply the new transform back to the instance (local‐space)
	if (!UpdateInstanceTransform(InstanceIndex, InstanceTransform, /*bWorldSpace=*/ false, /*bMarkRenderStateDirty=*/
	                             true, /*bTeleport=*/ false))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("SetInstanceHidden: Failed to update transform for instance %d."), InstanceIndex)
		);
		return;
	}

	InstanceHiddenMap[InstanceIndex] = bHide;
}

int32 URTSHidableInstancedStaticMeshComponent::GetNumVisibleInstances() const
{
	int32 VisibleCount = 0;
	for (const TPair<int32, bool>& Pair : InstanceHiddenMap)
	{
		if (!Pair.Value)
		{
			++VisibleCount;
		}
	}
	return VisibleCount;
}

void URTSHidableInstancedStaticMeshComponent::GetFirstNonHiddenInstance(FTransform& OutTransform,
                                                                        int32& OutInstanceIndex) const
{
	// Make sure our map is in sync with the instance count; if not, report an error.
	const int32 Count = GetInstanceCount();
	if (InstanceHiddenMap.Num() < Count)
	{
		RTSFunctionLibrary::ReportError(TEXT("GetFirstNonHiddenInstance: Hidden‐map is out of date."));
		return;
	}

	// Search from index 0 up to Count-1 for the first not-hidden instance
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const bool* bHiddenPtr = InstanceHiddenMap.Find(Index);
		if (bHiddenPtr && !(*bHiddenPtr))
		{
			// Found a visible instance; fetch its transform.
			if (GetInstanceTransform(Index, OutTransform, /*bWorldSpace=*/ false))
			{
				OutInstanceIndex = Index;
				return;
			}
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("GetFirstNonHiddenInstance: Failed to get transform for instance %d."), Index)
			);
			return;
		}
	}

	// No non-hidden instance found
	RTSFunctionLibrary::ReportError(TEXT("GetFirstNonHiddenInstance: No non-hidden instance found."));
}

int32 URTSHidableInstancedStaticMeshComponent::AddInstance(const FTransform& InstanceTransform, bool bWorldSpace)
{
	const int32 Index = Super::AddInstance(InstanceTransform, bWorldSpace);
	UpdateHiddenMap();
	return Index;
}

bool URTSHidableInstancedStaticMeshComponent::RemoveInstance(int32 InstanceIndex)
{
	const bool bResult = Super::RemoveInstance(InstanceIndex);
	// Rebuild or re-sync the hidden map after removal
	InitializeHiddenMap();
	return bResult;
}

void URTSHidableInstancedStaticMeshComponent::ClearInstances()
{
	Super::ClearInstances();
	InitializeHiddenMap();
}
