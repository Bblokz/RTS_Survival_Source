#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnimatedTextPoolActor.generated.h"

/**
 * @brief Invisible owner actor that contains the screen-space UWidgetComponents for pooled animated texts.
 */
UCLASS()
class RTS_SURVIVAL_API AAnimatedTextPoolActor : public AActor
{
	GENERATED_BODY()

public:
	AAnimatedTextPoolActor();

protected:
	virtual bool ShouldTickIfViewportsOnly() const override { return false; }
};
