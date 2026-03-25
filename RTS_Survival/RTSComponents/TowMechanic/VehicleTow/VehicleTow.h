#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 PreferredAbilityIndex = 10;
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
	                        const ETowedActorTarget TowSubtype);
	void SwapAbilityToDetachTow() const;
	void SwapAbilityToTow() const;
	void ClearTowRelationship();

	const FVehicleTowSettings& GetTowSettings() const { return M_TowSettings; }
	USceneComponent* GetTowMeshComponent() const;
	AActor* GetTowedActor() const;
	UTowedActorComponent* GetTowedActorComp() const;
	ETowedActorTarget GetCurrentTowSubtype() const { return M_CurrentTowSubtype; }

protected:
	virtual void BeginPlay() override;

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
	ETowedActorTarget M_CurrentTowSubtype = ETowedActorTarget::None;

	void BeginPlay_SetupCommandsInterface();
	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;
	[[nodiscard]] bool GetIsValidICommands() const;

	void AddAbilityToCommands() const;
};
