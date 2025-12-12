#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSTimedProgressBarPoolActor.generated.h"

/**
 * @brief Invisible owner actor that contains the screen-space UWidgetComponents for pooled progress bars.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSTimedProgressBarPoolActor : public AActor
{
	GENERATED_BODY()

public:
	ARTSTimedProgressBarPoolActor();

protected:
	virtual bool ShouldTickIfViewportsOnly() const override { return false; }
};
