// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"

#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "ResourceConverterComponent.generated.h"

class URTSComponent;
class UPlayerResourceManager;
class UAnimatedTextWidgetPoolManager;

/**
 * @brief Container for vertical text layout/styling to use with the pooled system.
 */
USTRUCT(BlueprintType)
struct FResourceVerticalTextLayout
{
	GENERATED_BODY()

	/** When true the text will auto-wrap at InWrapAt (set by the component). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	bool bAutoWrap = true;

	/** Width in px when auto-wrap is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	float InWrapAt = 320.f;

	/** Text justification for the rich text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	TEnumAsByte<ETextJustify::Type> InJustification = ETextJustify::Center;

	/** Animation timings and vertical motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	FRTSVerticalAnimTextSettings InSettings;

	/** Offset added to Owner->GetActorLocation() for the text spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	FVector TextOffset = FVector(0.f, 0.f, 120.f);
};

/**
 * @brief Tick settings and deltas for resource conversion.
 */
USTRUCT(BlueprintType)
struct FRTSResourceTickSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Tick")
	bool bStartEnabled = false;
	/** Interval in seconds between conversion ticks. Must be > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Tick")
	float TickRate = 1.0f;

	/**
	 * Resource type → delta per tick (negative = cost; positive = gain).
	 * Only the first 1–2 entries will be used for vertical text display.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Tick")
	TMap<ERTSResourceType, int32> ResourceDeltas;
};

/**
 * @brief Aggregate settings for the converter.
 */
USTRUCT(BlueprintType)
struct FResourceConverterSettings
{
	GENERATED_BODY()

	/** Vertical text layout used when a tick succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Converter")
	FResourceVerticalTextLayout VerticalText;

	/** Tick rate + resource deltas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Converter")
	FRTSResourceTickSettings Tick;

	/** Optional: sound played when a tick succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Converter|Audio")
	USoundBase* OnTickSound = nullptr;

	/** Optional attenuation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Converter|Audio")
	USoundAttenuation* OnTickAttenuation = nullptr;

	/** Optional concurrency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource Converter|Audio")
	USoundConcurrency* OnTickConcurrency = nullptr;

};


/**
 * @brief Converts player resources at a fixed interval, optionally showing vertical resource text and playing a sound.
 * The component does not tick; it manages its own timer for conversion.
 * @note InitResourceConverter: call in Blueprint once to configure & start.
 * @note SetResourceConversionEnabled: runtime toggle to pause/resume conversion without destroying state.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UResourceConverterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UResourceConverterComponent();


	/**
	 * @brief Initialize the component and start the timer-driven resource conversion.
	 * @param InSettings All converter settings (tick rate, resource map, optional audio, vertical text).
	 */
	UFUNCTION(BlueprintCallable, Category="Resource Converter")
	void InitResourceConverter(const FResourceConverterSettings& InSettings);

	/**
	 * @brief Enable/disable resource conversion without tearing down state/timers permanently.
	 * @param bEnabled true to enable conversion; false to pause conversion.
	 * @note Is idempotent.
	 */
	UFUNCTION(BlueprintCallable, Category="Resource Converter")
	void SetResourceConversionEnabled(const bool bEnabled);

protected:
	// Unreal component entry points
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ======= Private state (grouped for readability) =======

	/** Settings provided via InitResourceConverter. */
	UPROPERTY()
	FResourceConverterSettings M_Settings;

	/** Whether conversion is currently allowed to run. */
	UPROPERTY()
	bool bM_Enabled = false;

	/** Our conversion timer handle; we do not use actor ticking. */
	FTimerHandle M_TickTimerHandle;

	// --- Cached pointers with validity wrappers (rule 0.5) ---

	// Owner’s RTS component (used to read OwningPlayer).
	UPROPERTY()
	TObjectPtr<URTSComponent> M_RTSComponent = nullptr;

	// Player resource manager (owned elsewhere).
	UPROPERTY()
	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;

	// Animated text manager (owned elsewhere).
	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWidgetPoolManager> M_AnimatedTextManager;

	// ======= Helpers / flow =======

	/** Start/refresh the timer from current settings; return false if not possible. */
	bool StartResourceTimer();

	/** Clear the running timer. */
	void StopResourceTimer();

	/** Core tick: attempts to apply deltas; plays VFX/SFX on success. */
	void OnResourceTick();

	// ---- Validity helpers with logging (rule 0.5) ----
	bool GetIsValidRTSComponent() const;
	bool GetIsValidPlayerResourceManager() const;
	bool GetIsValidAnimatedTextManager() const;

	// ---- Adapter helpers to your PlayerResourceManager API ----
	/**
	 * @brief Return true if the player has at least Required (absolute positive) amount for a given resource.
	 * Assumption: change this body to match your API names.
	 */
	bool HasEnough(const uint8 Player, const ERTSResourceType Res, const int32 RequiredAbs) const;

	/**
	 * @brief Apply a delta for a single resource. Return true if applied.
	 * Assumption: change this body to match your API names.
	 */
	bool ApplyDelta(const uint8 Player, const ERTSResourceType Res, const int32 Delta) const;

	/**
	 * @brief Apply multiple deltas atomically (all-or-nothing). Default: naive two-phase apply.
	 * If your manager exposes an atomic API, replace this.
	 */
	bool TryApplyAllDeltasAtomic(const uint8 Player, const TMap<ERTSResourceType, int32>& Deltas) const;

	// ---- Animated text / audio helpers ----
	void ShowResourceTextAtOwner(const TMap<ERTSResourceType, int32>& AppliedDeltas) const;
	void PlayTickSoundAtOwner() const;

	// ---- Small utilities ----
	/** Clamp the map to max 2 entries for UI; logs and truncates deterministically. */
	static void ClampMapForDisplay(TMap<ERTSResourceType, int32>& InOutMap);

	bool Init_SetupResourceAndPoolManagers();

	/**
     * @brief Loads and caches the AnimatedTextWidgetPoolManager from the world subsystem.
     * @return true if successfully set the pointer, false otherwise (error already logged).
     */
    bool GetAnimTextMgrFromWorld();
};
