#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "ScavRewardComponent.generated.h"

class UCameraComponent;
class UScavengeRewardWidget;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UScavRewardComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UScavRewardComponent();

	void StartDisplay();

	/** Sets the reward text on the widget */
	void SetRewardText(const FText& InText);

	// set how long the fading away effect should take.
	void SetFadeDuration(float InFadeDuration);

	// Set how long the regular visible duration should be.
	void SetVisibleDuration(float InVisibleDuration);

protected:
	virtual void BeginPlay() override;
	// virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	//                            FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<UScavengeRewardWidget> M_RewardWidget;

	/** Timer handle for destroy */
	FTimerHandle DestroyTimerHandle;

	/** Timer handle to start fade out */
	FTimerHandle FadeOutTimerHandle;

	/** Destroys the component */
	void DestroySelf();

	/** Starts the fade-out process */
	void StartFadeOut();

	// UPROPERTY()
	// UCameraComponent* M_PlayerCameraPawn;

	UPROPERTY()
	float M_VisibleDuration;

	/** Duration over which the widget fades out */
	UPROPERTY()
	float M_FadeDuration;

	/** Total lifetime of the component */
	UPROPERTY()
	float M_TotalLifetime;
	
};
