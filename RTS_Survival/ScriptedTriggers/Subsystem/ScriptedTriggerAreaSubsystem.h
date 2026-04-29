#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScriptedTriggerAreaSubsystem.generated.h"

class ATriggerArea;

/**
 * @brief World subsystem that caches standalone trigger area classes for scripted trigger actors.
 */
UCLASS()
class RTS_SURVIVAL_API UScriptedTriggerAreaSubsystem : public UWorldSubsystem
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
