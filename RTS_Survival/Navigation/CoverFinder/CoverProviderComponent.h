#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"
#include "CoverProviderComponent.generated.h"

/**
 * @brief Registers designer-authored local cover points for an actor whose transform remains static after BeginPlay.
 * Registration is removed automatically when the component or owning actor ends play.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent, DisplayName="RTS Cover Provider"))
class RTS_SURVIVAL_API URTSCoverProviderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSCoverProviderComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditAnywhere, Category="Cover", meta=(AllowPrivateAccess="true", TitleProperty="CoverType"))
	TArray<FRTSLocalCoverPoint> M_LocalCoverPoints;

	uint64 M_RegistrationId = 0;

	void RegisterCoverPoints();
	void UnregisterCoverPoints();
	TArray<FRTSCoverPoint> BuildWorldCoverPoints(const AActor& Owner) const;
};
