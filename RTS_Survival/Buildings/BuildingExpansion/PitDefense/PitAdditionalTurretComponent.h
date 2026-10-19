#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PitAdditionalTurretComponent.generated.h"

class UChildActorComponent;

/**
 * @brief Registers an extra child-actor turret owned by a pit and keeps it synchronized with the pit lifecycle.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPitAdditionalTurretComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPitAdditionalTurretComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UChildActorComponent* FindAdditionalTurretComponent() const;
	void HandlePitConstructed();
	void HandlePitPackingUp();
	void HandlePitPackingCancelled();
	void SetAdditionalTurretVisibility(bool bVisible) const;

	UPROPERTY(EditAnywhere, Category="Pit Turret")
	FName M_ChildActorComponentName = TEXT("MG42Right");
};
