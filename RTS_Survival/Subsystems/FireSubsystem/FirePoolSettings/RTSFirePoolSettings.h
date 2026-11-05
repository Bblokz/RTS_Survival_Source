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

	/** Most upright flames (pool GroundFireFlame) one burning area gets. The GroundFire carpet alone is low and reads
	 * as flat streaks from the RTS camera, so every area also gets standing flames spread over it; 0 turns them off.
	 * Keep the GroundFireFlame pool size near MaxActiveGroundFires x the flames of a typical area. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0", UIMin="0"))
	int32 MaxGroundFireFlamesPerArea = 4;

	/** One upright flame per circle of this radius (cm) of burning area, at least one per area. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="1.0", UIMin="1.0"))
	float GroundFireFlameCoverRadiusCm = 180.f;

	/** Uniform component scale of an upright flame for each unit of the fire's VfxScale. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.01", UIMin="0.01"))
	float GroundFireFlameScalePerVfxScale = 0.6f;

	/** Clamp of an upright flame's uniform component scale. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.01", UIMin="0.01"))
	float GroundFireFlameMinScale = 0.8f;

	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.01", UIMin="0.01"))
	float GroundFireFlameMaxScale = 2.2f;

	/** Fraction of the area radius at which the outer flames stand (the first flame stands in the centre). */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float GroundFireFlameRingFraction = 0.55f;

	/** Whether an area also gets the low GroundFire carpet (embers, glow) under its upright flames. */
	UPROPERTY(Config, EditAnywhere, Category="Ground Fire")
	bool bGroundFireCarpet = true;
};
