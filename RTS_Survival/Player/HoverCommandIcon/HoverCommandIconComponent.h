#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconTypes.h"
#include "HoverCommandIconComponent.generated.h"

class UAnimatedIconWidgetPoolManager;
class UPlayerCommandTypeDecoder;
class UW_RTSVerticalAnimatedIcon;

/** @brief Remembers what the last prediction was based on so it only re-runs when its inputs can have changed. */
USTRUCT()
struct FHoverCommandIconPredictionCache
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> HoveredActor;

	UPROPERTY()
	TWeakObjectPtr<AActor> PrimarySelectedUnit;

	// Real time, so a paused game still refreshes target state such as repair eligibility.
	double EvaluatedAtRealTimeSeconds = 0.0;

	ERTSVerticalAnimatedIcon PredictedIcon = ERTSVerticalAnimatedIcon::None;

	bool bHasPrediction = false;
};

/**
 * @brief Owned by ACPPController, which calls UpdateCommandIcon each tick while a secondary click would issue a
 * targeted command, and HideCommandIcon otherwise. Shows the command's catalog icon next to the cursor straight away,
 * unlike the delayed hovering-actor description. Commands without artwork in the icon data asset are never shown.
 */
UCLASS(ClassGroup=(Player))
class RTS_SURVIVAL_API UHoverCommandIconComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHoverCommandIconComponent();

	/**
	 * @brief Keeps the cursor icon in step with the command a secondary click on the hovered actor would issue.
	 * @param CommandTypeDecoder Decoder the controller also uses to execute the click.
	 * @param HoveredActor Actor under the cursor after the click's child-actor redirect.
	 * @param PrimarySelectedUnit Unit whose abilities decide the command.
	 * @param MouseViewportPosition Cursor position in viewport pixels.
	 */
	void UpdateCommandIcon(
		const UPlayerCommandTypeDecoder& CommandTypeDecoder,
		AActor* HoveredActor,
		AActor* PrimarySelectedUnit,
		const FVector2D& MouseViewportPosition);

	void HideCommandIcon();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UW_RTSVerticalAnimatedIcon> M_CommandIconWidget;

	UPROPERTY()
	FHoverCommandIconPredictionCache M_PredictionCache;

	// The icon currently drawn; differs from the prediction when the catalog has no artwork for it.
	ERTSVerticalAnimatedIcon M_DisplayedIcon = ERTSVerticalAnimatedIcon::None;

	void BeginPlay_CreateCommandIconWidget();
	bool GetShouldRefreshPrediction(const AActor* HoveredActor, const AActor* PrimarySelectedUnit,
	                                const double NowRealTimeSeconds) const;
	void RefreshPrediction(const UPlayerCommandTypeDecoder& CommandTypeDecoder, AActor* HoveredActor,
	                       AActor* PrimarySelectedUnit, const double NowRealTimeSeconds);
	void DisplayPredictedIcon();
	void PositionIconAtCursor(const FVector2D& MouseViewportPosition) const;
	UAnimatedIconWidgetPoolManager* GetIconCatalog() const;
	bool GetIsValidCommandIconWidget() const;
};
