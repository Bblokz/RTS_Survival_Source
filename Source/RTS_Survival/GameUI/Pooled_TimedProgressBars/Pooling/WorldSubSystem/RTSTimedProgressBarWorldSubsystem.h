#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/RTSProgressBarType.h"
#include "RTSTimedProgressBarWorldSubsystem.generated.h"

class URTSTimedProgressBarManager;
class USceneComponent;

/**
 * @brief World subsystem that owns and ticks the timed progress bar widget pool manager.
 * Provides the requested public API that returns an ActivationID you can store.
 */
UCLASS()
class RTS_SURVIVAL_API URTSTimedProgressBarWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	/** Simple activation (non-attached), returns ActivationID. */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	int ActivateTimedProgressBar(float RatioStart,
	                             float TimeTillFull,
	                             bool bUsePercentageText,
	                             ERTSProgressBarType BarType,
	                             const bool bUseDescriptionText, const FString& InText,
	                             const FVector& Location,
	                             const float ScaleMlt);

	/** Activation with anchor, returns ActivationID. */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	int ActivateTimedProgressBarAnchored(USceneComponent* AnchorComponent,
	                                     const FVector AttachOffset,
	                                     float RatioStart,
	                                     float TimeTillFull,
	                                     bool bUsePercentageText,
	                                     ERTSProgressBarType BarType,
	                                     const bool bUseDescriptionText,
	                                     const FString& InText,
	                                     const float ScaleMlt);

	/**
	 * Update the world location of a non-attached bar; if the bar is attached,
	 * does nothing and reports an error.
	 */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	bool UpdateProgressBar(int ActivationID, const FVector& NewWorldLocation);

	/** Force a bar dormant by ActivationID. */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	void ForceProgressBarDormant(int ActivationID);

	/** Get the pool manager; available if you need lower-level access. */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	URTSTimedProgressBarManager* GetTimedProgressBarManager() const { return M_PoolManager; }

private:
	UPROPERTY()
	URTSTimedProgressBarManager* M_PoolManager = nullptr;

	bool GetIsValidPoolManager() const;
};
