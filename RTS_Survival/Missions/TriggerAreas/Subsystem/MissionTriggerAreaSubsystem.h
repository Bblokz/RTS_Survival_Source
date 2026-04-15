#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MissionTriggerAreaSubsystem.generated.h"

class ATriggerArea;

/**
 * @brief World subsystem that caches mission trigger area classes from developer settings for runtime spawning.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionTriggerAreaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	TSubclassOf<ATriggerArea> GetRectangleTriggerAreaClass() const;
	TSubclassOf<ATriggerArea> GetSphereTriggerAreaClass() const;

private:
	UPROPERTY()
	TSubclassOf<ATriggerArea> M_RectangleTriggerAreaClass;

	UPROPERTY()
	TSubclassOf<ATriggerArea> M_SphereTriggerAreaClass;
};
