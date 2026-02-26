#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CrewPositionType.h"
#include "CrewPosition.generated.h"

/**
 * @brief Place this component on TeamWeapon blueprints to define where crew should stand while deployed.
 * Lets designers author stable per-weapon operating spots without hardcoded offset logic.
 */
UCLASS(ClassGroup=(TeamWeapon), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UCrewPosition : public USceneComponent
{
	GENERATED_BODY()

public:
	ECrewPositionType GetCrewPositionType() const;
	FTransform GetCrewWorldTransform() const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew Position", meta = (AllowPrivateAccess = "true"))
	ECrewPositionType M_CrewPositionType = ECrewPositionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew Position", meta = (AllowPrivateAccess = "true"))
	float M_AcceptanceRadius = 75.0f;
};
