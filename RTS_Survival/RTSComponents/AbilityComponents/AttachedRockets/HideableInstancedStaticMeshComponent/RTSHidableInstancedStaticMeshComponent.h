#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "RTSHidableInstancedStaticMeshComponent.generated.h"

/**
 * An InstancedStaticMeshComponent that supports "hiding" individual instances
 * by moving them down or up along the local Z-axis by 4000 units.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSHidableInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	/**
	 * Hide or unhide the instance at the given index.
	 *
	 * @param InstanceIndex	The index of the instance to modify.
	 * @param bHide			If true, moves the instance down by 4000 units (hides it).
	 *                      If false, moves the instance up by 4000 units (unhides it).
	 */
	UFUNCTION(BlueprintCallable, Category = "Instances")
	void SetInstanceHidden(int32 InstanceIndex, bool bHide);

	/** Returns how many instances are currently visible (i.e. not "hidden"). */
	UFUNCTION(BlueprintCallable, Category = "Instances")
	int32 GetNumVisibleInstances() const;

	void GetFirstNonHiddenInstance(FTransform& OutTransform, int32& OutInstanceIndex) const;

	virtual int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace) override;
	virtual bool RemoveInstance(int32 InstanceIndex);

	virtual void ClearInstances() override;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitProperties() override;

private:
	/** Maps each instance index → whether it is hidden (true) or visible (false). */
	TMap<int32, bool> InstanceHiddenMap;

	/** Initializes the map so that every instance is marked as "not hidden." */
	void InitializeHiddenMap();

	void UpdateHiddenMap();
};
