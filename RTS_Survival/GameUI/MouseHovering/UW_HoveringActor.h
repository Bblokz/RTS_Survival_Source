#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "UW_HoveringActor.generated.h"

class UResourceComponent;
class URichTextBlock;
class UResourceDropOff;
class UCargo;
class ICaptureInterface;

/**
 * @brief Widget that shows actor info when the player hovers over actors in the world.
 */
UCLASS()
class RTS_SURVIVAL_API UW_HoveringActor : public UUserWidget, public IRTSUIElement
{
	GENERATED_BODY()

public:
	/** Sets the actor this widget should display info about. */
	UFUNCTION(BlueprintCallable)
	void SetHoveredActor(AActor* HoveredActor);

	/** Hides the widget (e.g. when hover is cancelled). */
	UFUNCTION(BlueprintCallable)
	void HideWidget();

	/** Global UI hide toggle (IRTSUIElement). */
	virtual void OnHideAllGameUI(const bool bHide) override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_ActorInfoText;

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> M_CurrentHoveredActor;

	// Tracks whether this widget was hidden by the global "hide all UI".
	UPROPERTY()
	bool bM_WasHiddenByAllGameUI = false;

	/**
	 * @brief Resolves the text that should be shown for the hovered actor.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when a meaningful text was produced.
	 * @param OutPadding Optional padding override for the text widget.
	 * @return Text that should be shown for the hovered actor.
	 */
	FString GetActorText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding) const;

	/**
	 * @brief Attempts to build hover text for generic RTS actors, including harvesters and drop-offs.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when RTS specific text was produced.
	 * @param OutPadding Optional padding override for the text widget.
	 * @param OutActorText Resulting text when this returns true.
	 * @return True if the actor was handled as an RTS actor.
	 */
	bool TryBuildRTSActorHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
	                               FString& OutActorText) const;

	/**
	 * @brief Attempts to build hover text for actors that expose cargo slots.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when cargo text was produced.
	 * @param OutPadding Optional padding override for the text widget.
	 * @param OutActorText Resulting text when this returns true.
	 * @return True if the actor had a UCargo component and was handled.
	 */
	bool TryBuildCargoHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
	                            FString& OutActorText) const;

	/**
	 * @brief Attempts to build hover text for actors that can be captured.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when capture text was produced.
	 * @param OutPadding Optional padding override for the text widget.
	 * @param OutActorText Resulting text when this returns true.
	 * @return True if the actor implements ICaptureInterface and was handled.
	 */
	bool TryBuildCaptureHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
	                              FString& OutActorText) const;

	/**
	 * @brief Attempts to build hover text for actors that are pure resources.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when resource text was produced.
	 * @param OutPadding Optional padding override for the text widget.
	 * @param OutActorText Resulting text when this returns true.
	 * @return True if the actor had a UResourceComponent and was handled.
	 */
	bool TryBuildResourceHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
	                               FString& OutActorText) const;

	bool TryBuildScavengeHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
	                              FString& OutActorText) const;

	/**
	 * @brief Attempts to build hover text for drop-off actors that have no RTS component.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when drop-off text was produced.
	 * @param OutPadding Optional padding override for the text widget.
	 * @param OutActorText Resulting text when this returns true.
	 * @return True if the actor was handled as a test drop-off actor.
	 */
	bool TryBuildDropOffHoverText_NoRTS(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
	                                    FString& OutActorText) const;

	/**
	 * @brief Attempts to build hover text for landscape actors.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when landscape text was produced.
	 * @param OutActorText Resulting text when this returns true.
	 * @return True if the actor is terrain.
	 */
	bool TryBuildLandscapeHoverText(AActor* HoveredActor, bool& bOutIsValidText, FString& OutActorText) const;

	/**
	 * @brief Attempts to build hover text for static world geometry.
	 * @param HoveredActor Actor currently under the cursor.
	 * @param bOutIsValidText True when static mesh text was produced.
	 * @param OutActorText Resulting text when this returns true.
	 * @return True if the actor is a plain static mesh actor.
	 */
	bool TryBuildStaticMeshHoverText(AActor* HoveredActor, bool& bOutIsValidText, FString& OutActorText) const;

	/**
	 * @brief Builds the fallback text when no specific category could be resolved.
	 * @param bOutIsValidText Always set to false by this function.
	 * @return Fallback text to display when the actor type is unknown.
	 */
	FString GetUnknownActorText(bool& bOutIsValidText) const;

	/**
	 * @brief Builds rich text for a resource node (type + amount remaining).
	 * @param ResourceComponent Resource component on the hovered actor.
	 * @return Rich text describing the resource.
	 */
	FString GetResourceText(const UResourceComponent* ResourceComponent) const;

	/**
	 * @brief Builds rich text for a resource drop-off structure.
	 * @param DropOffComponent Drop-off component on the hovered actor.
	 * @param RTSComponent Optional RTS component to resolve a display name from.
	 * @return Rich text describing the drop-off and its capacities.
	 */
	FString GetDropOffText(const UResourceDropOff* DropOffComponent, const URTSComponent* RTSComponent) const;

	/**
	 * @brief Builds rich text for a harvester RTS unit.
	 * @param RTSComp RTS component used to obtain the display name.
	 * @param HarvesterComp Harvester component providing capacity and resource info.
	 * @param bOutIsValidString True when a meaningful string was produced.
	 * @return Rich text describing the harvester state.
	 */
	FString GetHarvesterText(const URTSComponent* RTSComp, const UHarvester* HarvesterComp,
	                         bool& bOutIsValidString) const;

	/**
	 * @brief Applies optional padding to the rich text border slot.
	 * @param PaddingText Padding value in pixels.
	 */
	void SetPaddingForText(const int32 PaddingText) const;
};
