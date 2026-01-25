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
};
