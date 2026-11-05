#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTSFirePoolSettings.generated.h"

class UNiagaraSystem;

USTRUCT(BlueprintType)
struct FFireSettingsForType
{
	GENERATED_BODY()

	/** Niagara system to pool for this fire type. */
	UPROPERTY(EditAnywhere, Config, Category="Fire Pool")
	TSoftObjectPtr<UNiagaraSystem> FireSystem;

	/** Initial pool size created for this type. */
	UPROPERTY(EditAnywhere, Config, Category="Fire Pool", meta=(ClampMin="1", UIMin="1"))
	int32 PoolSize = 1;
};

/**
 * @brief Project settings for pooled RTS fire visuals.
 * Configure the Niagara system and initial pool size for each fire type.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Fire Pool"))
class RTS_SURVIVAL_API URTSFirePoolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSFirePoolSettings();

	/** Per-type pool configuration used to build the fire pools on world init. */
	UPROPERTY(Config, EditAnywhere, Category="Fire Pool")
	TMap<ERTSFireType, FFireSettingsForType> FireSettingsByType;

	UPROPERTY(Config, EditAnywhere, Category="Fire Effect User Param Names")
	FName ScaleMltUserParamName = FName("Scale");

	UPROPERTY(Config, EditAnywhere, Category="Fire Effect User Param Names")
	FName Fire_RGB_MltUserParamName = FName("Fire_RGB_Mlt");

	UPROPERTY(Config, EditAnywhere, Category="Fire Effect User Param Names")
	FName Smoke_RGB_MltUserParamName = FName("Smoke_RGB_Mlt");

	/** Most ground fire areas burning at once; at the cap a new fire replaces the one closest to burning out.
	 * Keep the GroundFire pool size at least this large. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="1", UIMin="1"))
	int32 MaxActiveGroundFires = 24;

	/** A new ground fire of the same player whose centre lies within this fraction of the larger radius of a burning
	 * area refreshes that area instead of starting a second one. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.0", UIMin="0.0"))
	float GroundFireMergeDistanceFraction = 0.75f;

	/** Spawn radius (cm) of the GroundFire Niagara system at component scale 1; the component is scaled in X/Y by
	 * RadiusCm / this so the flames cover the burning area. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="1.0", UIMin="1.0"))
	float GroundFireVfxBaseRadiusCm = 100.f;

	/** Most upright flames (pool GroundFireFlame) one burning area gets: one in the centre, the rest on a ring whose
	 * flames end just inside the burning radius, so the flames show the area that does damage. The GroundFire carpet
	 * alone is low and nearly invisible from the RTS camera; 0 turns the upright flames off.
	 * Keep the GroundFireFlame pool size near MaxActiveGroundFires x this. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0", UIMin="0"))
	int32 MaxGroundFireFlamesPerArea = 6;

	/** Half width (cm) one upright flame covers on the ground, seen from above, at component scale 1 (measured with
	 * -FireFootprintStudy). */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="1.0", UIMin="1.0"))
	float GroundFireFlameFootprintCm = 55.f;

	/** How far (cm) inside the burning radius the outermost flames end. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.0", UIMin="0.0"))
	float GroundFireFlameEdgeMarginCm = 20.f;

	/** Distance between neighbouring ring flames in flame widths: 1 touches, more leaves gaps. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.5", UIMin="0.5"))
	float GroundFireFlameSpacing = 1.6f;

	/** Uniform component scale of an upright flame for each unit of the fire's VfxScale. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.01", UIMin="0.01"))
	float GroundFireFlameScalePerVfxScale = 0.6f;

	/** Clamp of an upright flame's uniform component scale; large areas raise it until their ring closes. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.01", UIMin="0.01"))
	float GroundFireFlameMinScale = 1.6f;

	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.01", UIMin="0.01"))
	float GroundFireFlameMaxScale = 2.2f;

	/** Whether an area also gets the low GroundFire carpet (embers, glow) under its upright flames. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire")
	bool bGroundFireCarpet = true;
};
