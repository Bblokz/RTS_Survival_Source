#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/RTSComponents/TowMechanic/TowAbilityTypes/TowAbilityTypes.h"
#include "VehicleTow.generated.h"

class UTowedActorComponent;

USTRUCT(BlueprintType)
struct FVehicleTowSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName TowSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AcceptanceRadiusToTow = 500.0f;
};

/**
 * @brief Holds towing settings and runtime tow state for a towing vehicle.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UVehicleTowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleTowComponent();

	// Will be called with the mesh component it needs in the derived blueprint.
	UFUNCTION(BlueprintCallable)
	void InitTowMesh(USceneComponent* TowMeshComponent);

	bool IsTowFree() const;
	bool GetIsValidTowMeshComponent() const;
	bool GetIsValidTowedActor() const;
	bool GetIsValidTowedActorComp() const;

	void SetTowRelationship(AActor* TowedActor, UTowedActorComponent* TowedActorComp,
	                        const ETowActorAbilitySubtypes TowSubtype);
	void ClearTowRelationship();

	const FVehicleTowSettings& GetTowSettings() const { return M_TowSettings; }
	USceneComponent* GetTowMeshComponent() const;
	AActor* GetTowedActor() const;
	UTowedActorComponent* GetTowedActorComp() const;
	ETowActorAbilitySubtypes GetCurrentTowSubtype() const { return M_CurrentTowSubtype; }

private:
	UPROPERTY(EditDefaultsOnly, Category="Tow")
	FVehicleTowSettings M_TowSettings;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_TowMeshComponent;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_TowedActor;

	UPROPERTY()
	TWeakObjectPtr<UTowedActorComponent> M_TowedActorComp;

	UPROPERTY()
	ETowActorAbilitySubtypes M_CurrentTowSubtype = ETowActorAbilitySubtypes::None;
};
