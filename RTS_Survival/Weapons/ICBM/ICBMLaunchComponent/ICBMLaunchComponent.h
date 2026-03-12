#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Weapons/WeaponTargetingData/WeaponTargetingData.h"
#include "RTS_Survival/Weapons/ICBM/ICBMTypes.h"
#include "ICBMLaunchComponent.generated.h"

class AICBMActor;
class URTSComponent;
class UMeshComponent;

USTRUCT()
struct FICBMSocketState
{
	GENERATED_BODY()

	UPROPERTY()
	FName SocketName = NAME_None;

	UPROPERTY()
	TObjectPtr<AICBMActor> M_Rocket = nullptr;

	UPROPERTY()
	bool bM_IsReloading = false;
};

USTRUCT()
struct FICBMPendingLaunchRequest
{
	GENERATED_BODY()

	UPROPERTY()
	int32 M_RemainingRocketsToLaunch = 0;

	UPROPERTY()
	FWeaponTargetingData M_TargetingData;
};

/**
 * @brief Handles staged reload + launch orchestration for attached ICBM actors on socketed meshes.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UICBMLaunchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UICBMLaunchComponent();

	void InitICBMComp(UMeshComponent* MeshComponent);

	void LaunchICBMsAtLocation(int32 AmountToLaunch, const FVector& TargetLocation);
	void LaunchICBMsAtActor(int32 AmountToLaunch, AActor* TargetActor);

	void OnICBMReachedLaunchReady(AICBMActor* Rocket);
	FICBMLaunchSettings GetLaunchSettings() const { return M_LaunchSettings; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ICBM")
	FICBMLaunchSettings M_LaunchSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ICBM")
	TSubclassOf<AICBMActor> M_ICBMActorClass;

private:
	void StartReloadingICBMs();
	void BeginReloadForSocket(const int32 SocketIndex);
	void OnReloadTimerFinished(const int32 SocketIndex);

	AICBMActor* SpawnRocketAtSocket(const int32 SocketIndex) const;
	bool TryConsumePendingLaunches();
	int32 LaunchReadyRockets(const int32 DesiredAmount);
	void RegisterPendingLaunch(const int32 RemainingAmountToLaunch);

	bool GetIsValidOwnerMeshComponent() const;
	bool GetIsValidOwningRTSComponent() const;

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> M_OwnerMeshComponent = nullptr;

	UPROPERTY()
	TWeakObjectPtr<URTSComponent> M_OwningRTSComponent = nullptr;

	UPROPERTY()
	int32 M_OwningPlayer = INDEX_NONE;

	UPROPERTY()
	TArray<FICBMSocketState> M_SocketStates;

	UPROPERTY()
	FICBMPendingLaunchRequest M_PendingLaunchRequest;

	TMap<int32, FTimerHandle> M_SocketReloadTimers;
};
