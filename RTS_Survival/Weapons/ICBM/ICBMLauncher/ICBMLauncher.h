#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ICBMLauncher.generated.h"

class UICBMLaunchComponent;

UINTERFACE(MinimalAPI, NotBlueprintable)
class UICBMLauncher : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Implement on actors that expose an ICBM launch component to gameplay systems.
 */
class RTS_SURVIVAL_API IICBMLauncher
{
	GENERATED_BODY()

public:
	virtual UICBMLaunchComponent* GetICBMLaunchComponent() const = 0;
};
