#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "NavModifierComponent.h"
#include "RTSNavCollision.generated.h"
class UNavigationSystemV1;

USTRUCT(Blueprintable)
struct FRTSNavCollisionSettings
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSubclassOf<UNavArea> PlayerNavigationFilter = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSubclassOf<UNavArea> EnemyNavigationFilter = nullptr;

	bool IsValidNavFilters();
	
};

/**
 * A wrapper around the regular box component; this component is used to block the navigation mesh.
 * Has collision disabled at begin play. 
 * Set navrelevancy on actor componets to affect the nav modifier radius.
 * TODO using obstacle channel as nav modifier does not update in time!! use NULL channel instead.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSNavCollision : public UNavModifierComponent
{
	GENERATED_BODY()

public:
	URTSNavCollision();

	/** Enable or disable affecting the navmesh */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void EnableAffectNavmesh(bool bEnable);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, category = "Navigation")
	FRTSNavCollisionSettings NavCollisionSettings;

private:
	void RequestNavMeshRefresh();
	UPROPERTY()
	TWeakObjectPtr<UNavigationSystemV1> M_NavSystem = nullptr;

	bool EnsureIsValidNavSystem()const;

	void BeginPlay_SetupNavigationFilters();
};
