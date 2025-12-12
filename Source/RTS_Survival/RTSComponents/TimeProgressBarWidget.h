// Copyright Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "TimeProgressBarState.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "TimeProgressBarWidget.generated.h"

class UWidgetComponent;
/**
 * @class UTimeProgressBarWidget
 * 
 * @brief A widget component to display progress over time with an optional text display of progress percentage.
 * The M_ProgressText and M_ProgressBar are bound to the respective widgets in the UMG editor automatically
 * by giving the elements the same name as the variables.
 */
UCLASS()
class RTS_SURVIVAL_API UTimeProgressBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * @brief Initializes the time progress component with the camera sphere to orient the progress bar towards.
     * @param WidgetComponentOfBar The reference to the bar widget as component on the owner of the TimeProgressBarWidget.
     */
    UFUNCTION(BlueprintCallable, Category= "Initialization")
    void InitTimeProgressComponent(
        UWidgetComponent* WidgetComponentOfBar);

    /** Starts or resumes the progress bar for the specified duration. */
    UFUNCTION(BlueprintCallable, Category= "Progress Bar Control")
    void StartProgressBar(float Time);

    /** Pauses the progress bar. */
    UFUNCTION(BlueprintCallable, Category= "Progress Bar Control")
    void PauseProgressBar(const bool bHideBarWhilePaused = false);

    /** Resets any pause, progress and time states but keeps the bar visible
     * to completely reset the progress bar, use StopProgressBar() */
    UFUNCTION(BlueprintCallable, Category= "Progress Bar Control")
    void ResetProgressBarKeepVisible();
    
    /** Stops the progress bar and hides it. */
    UFUNCTION(BlueprintCallable, Category= "Progress Bar Control")
    void StopProgressBar();

    /** @return How long the progress bar has been running. */
    float GetTimeElapsed() const;

    /** @brief Sets the local location of the progress bar. */
    inline void SetLocalLocation(const FVector& NewLocation) const ; 

    inline FVector GetLocalLocation() const;

protected:
    // Called when the game starts or the widget is created
    virtual void NativeConstruct() override;

private:
    void ResumeFromPause();
    void StartNewProgressBar(const float Time);
    // Bound automatically to the UMG widget by name.
    UPROPERTY(meta = (BindWidget))
    UProgressBar* M_ProgressBar;

    // Bound automatically to the UMG widget by name.
    UPROPERTY(meta = (BindWidget))
    UTextBlock* M_ProgressText;

    // Total time for the progress bar to complete
    float M_TotalTime;

    // Timer handle for progress bar update
    FTimerHandle M_ProgressUpdateHandle;

    // To update the progress bar's fill percentage and text display
    void UpdateProgressBar();

    // World spawned in
    UPROPERTY()
    UWorld* M_World;

    // Time when the progress bar started
    float M_StartTime;

    FPauseStateTimedProgressBar M_PauseState;

    // Used to set timer for the rotation of the progress bar.
    FTimerHandle M_BarRotationHandle;

    // Used to set timer for the rotation of the progress bar.
    FTimerDelegate M_BarRotationDel;

    // Reference to the bar widget as component on the owner of the TimeProgressBarWidget.
    // This is used for offsets.
    UPROPERTY()
    UWidgetComponent* M_WidgetComponent;

    bool GetIsValidWidgetComp()const;

};
