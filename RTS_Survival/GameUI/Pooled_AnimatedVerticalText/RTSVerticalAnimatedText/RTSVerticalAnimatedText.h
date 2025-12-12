#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GetTargetResourceThread/FGetAsyncResource.h"
#include "RTSVerticalAnimatedText.generated.h"

enum class ERTSResourceType : uint8;
/**
 * @brief Per-spawn animation settings for a vertical animated text.
 * Visible time + fade time together define the total duration across which the vertical delta is applied.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSVerticalAnimTextSettings
{
	GENERATED_BODY()

	/** How long (seconds) the text stays fully opaque before fading starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	float VisibleDuration = 0.75f;

	/** How long (seconds) the fade out takes after the visible time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	float FadeOutDuration = 0.5f;

	/** Total vertical offset (in screen-space units) moved upward over the whole lifetime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	float DeltaZ = 30.0f;
};

USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSVerticalSingleResourceTextSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	ERTSResourceType ResourceType = ERTSResourceType::Resource_Radixite;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	int32 AddOrSubtractAmount = 0;
	
};

USTRUCT(BlueprintType)
struct FRTSVerticalDoubleResourceTextSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	ERTSResourceType ResourceType1 = ERTSResourceType::Resource_Radixite;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	int32 AddOrSubtractAmount1 = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	ERTSResourceType ResourceType2 = ERTSResourceType::Resource_Radixite;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	int32 AddOrSubtractAmount2 = 0;
	
};

/**
 * @brief Minimal widget with a RichTextBlock; holds dormancy state and activation timestamp for pooling.
 * @note ActivateAnimatedText: call from the pool manager to (re)use this widget with text & layout settings.
 * @note SetDormant: called when the animation completes to return this widget to the pool.
 */
UCLASS()
class RTS_SURVIVAL_API UW_RTSVerticalAnimatedText : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_RTSVerticalAnimatedText(const FObjectInitializer& ObjectInitializer);

	/** BindWidget: must exist in the BP derived widget. */
	UPROPERTY(meta=(BindWidget))
	URichTextBlock* TextBlock_RichText = nullptr;

	/** True when not in use / pooled. False while actively animating. */
	UPROPERTY(BlueprintReadOnly, Category="Animated Text")
	bool bM_IsDormant = true;

	/**
	 * @brief Activate this widget with new text, line-break behavior, alignment, and animation settings.
	 * @param InText Text to place into the RichText block (rich markup allowed).
	 * @param bInAutoWrap Whether to auto-wrap text.
	 * @param InWrapAt Pixel width at which to wrap (only used if auto-wrap).
	 * @param InJustification Text justification/alignment within the text block.
	 * @param InSettings Per-instance animation settings (visible duration, fade, delta Z).
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	void ActivateAnimatedText(const FString& InText,
	                          const bool bInAutoWrap,
	                          const float InWrapAt,
	                          const TEnumAsByte<ETextJustify::Type> InJustification,
	                          const FRTSVerticalAnimTextSettings& InSettings);

	/** Set screen-space starting translation for this instance (manager will animate from here). */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	void ApplyInitialScreenOffset(const FVector2D InStartTranslation);

	/** Mark as dormant and hide; used by the pool when an animation completes. */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	void SetDormant(const bool bInDormant);

	/** Timestamp (seconds) this widget was last activated; used to select the oldest active when reusing. */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	float GetActivatedAtTimeSeconds() const { return M_ActivatedAtTimeSeconds; }

	/** Access last applied settings (if needed by BP for styling). */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	const FRTSVerticalAnimTextSettings& GetLastAnimSettings() const { return M_LastAnimSettings; }

	/** Get starting render translation as set by the manager. */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	FVector2D GetStartRenderTranslation() const { return M_StartRenderTranslation; }

protected:
	// Stored for reuse & querying from the manager.
	UPROPERTY()
	FRTSVerticalAnimTextSettings M_LastAnimSettings;

	// When this widget was last activated (world time seconds).
	UPROPERTY()
	float M_ActivatedAtTimeSeconds = -1.0f;

	// Screen-space starting translation (manager animates from here).
	UPROPERTY()
	FVector2D M_StartRenderTranslation = FVector2D::ZeroVector;

	/** Validity helper for the bound rich text block. */
	bool GetIsValidRichText() const;
};
