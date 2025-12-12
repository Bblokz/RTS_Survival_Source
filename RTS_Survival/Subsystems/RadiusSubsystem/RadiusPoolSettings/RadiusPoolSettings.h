#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "RadiusPoolSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * @brief Project settings for the pooled RTS radius system.
 * Configures pool size, base mesh, default scaling rules and materials per type.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Radius Pool"))
class RTS_SURVIVAL_API URadiusPoolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URadiusPoolSettings();

	/** Amount of pooled radius actors created on world init. */
	UPROPERTY(Config, EditAnywhere, Category="Pool", meta=(ClampMin="1", UIMin="1"))
	int32 DefaultPoolSize = 4;

	/** The mesh used to visualize radii (scaled uniformly in X/Y). */
	UPROPERTY(Config, EditAnywhere, Category="Rendering")
	TSoftObjectPtr<UStaticMesh> RadiusMesh;

	/** Initial radius used when the pool actors are initialized. */
	UPROPERTY(Config, EditAnywhere, Category="Rendering", meta=(ClampMin="0.0"))
	float StartingRadius = 2000.0f;

	/** Units per 1.0 scale on the mesh in X/Y. */
	UPROPERTY(Config, EditAnywhere, Category="Rendering", meta=(ClampMin="1.0"))
	float UnitsPerScale = 4300.0f;

	/** Fixed Z scale for the radius mesh. */
	UPROPERTY(Config, EditAnywhere, Category="Rendering", meta=(ClampMin="0.001"))
	float ZScale = 1.0f;

	/** Offset above ground at which the mesh renders. */
	UPROPERTY(Config, EditAnywhere, Category="Rendering")
	float RenderHeight = 100.0f;

	/** Material overrides per radius type. Leave unset to use the mesh’s default material. */
	UPROPERTY(Config, EditAnywhere, Category="Rendering")
	TMap<ERTSRadiusType, TSoftObjectPtr<UMaterialInterface>> TypeToMaterial;
};
