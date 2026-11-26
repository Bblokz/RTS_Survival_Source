// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireRules.h"
#include "WorldBatteryFireSettings.generated.h"

class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

/**
 * @brief Project settings page (Config/DefaultGame.ini) for the battery fire mission on the world map: the rule
 * numbers, the timing of the salvo, the look of tank divisions and every asset the show uses. Every asset may be
 * unset or missing: that part of the show is then skipped and logged once.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Battery Fire"))
class RTS_SURVIVAL_API UWorldBatteryFireSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldBatteryFireSettings();

	static const UWorldBatteryFireSettings* Get()
	{
		return GetDefault<UWorldBatteryFireSettings>();
	}

	/** @return Range, missions per turn and entries destroyed, for WorldBatteryFireRules. */
	FWorldBatteryFireNumbers GetNumbers() const;

	/**
	 * @brief Loads an optional asset of the show. An unset path or a path without a package returns nullptr and is
	 * logged once per path, so a missing effect never spams the log or stops the show.
	 * @param AssetPath Soft path from this page.
	 * @param SettingName Name of the setting, for the log line.
	 * @return The loaded asset or nullptr.
	 */
	static UObject* LoadOptionalAsset(const FSoftObjectPath& AssetPath, const TCHAR* SettingName);

	template <typename AssetType>
	static AssetType* LoadOptional(const TSoftObjectPtr<AssetType>& Asset, const TCHAR* SettingName)
	{
		return Cast<AssetType>(LoadOptionalAsset(Asset.ToSoftObjectPath(), SettingName));
	}

	// --- Rules ---

	/** XY range of a fire mission, in map units. Same as the battery's support radius by default. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0"))
	float Range;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0"))
	int32 MissionsPerTurn;

	/** Whole roster entries (tanks, squads, trucks) one mission destroys. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
	int32 EntriesDestroyedPerMission;

	// --- Salvo ---

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo", meta = (ClampMin = "1", ClampMax = "24"))
	int32 ShellCount;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo", meta = (ClampMin = "0.1"))
	float FlightSeconds;

	/** Seconds between two shells leaving the gun. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo", meta = (ClampMin = "0"))
	float ShellStaggerSeconds;

	/** Seconds before the first shell, while the camera frames gun and target. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo", meta = (ClampMin = "0"))
	float LeadInSeconds;

	/** Seconds the camera stays on the result after the last impact. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo", meta = (ClampMin = "0"))
	float LingerSeconds;

	/** Peak height of the arc as a share of the XY distance from gun to target. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo", meta = (ClampMin = "0"))
	float ArcHeightFactor;

	/** Shells that kill nothing land within this XY distance of the division. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo", meta = (ClampMin = "0"))
	float ImpactScatterRadius;

	/** Socket on the battery piece mesh the shells leave from; without it the height offset is used. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo")
	FName MuzzleSocketName;

	/** Height above the build node the shells leave from when the piece has no muzzle socket. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Salvo")
	float MuzzleHeightOffset;

	// --- Camera ---

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Camera")
	bool bFrameGunAndTarget;

	/** Puts the camera back where it was once the show ends. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Camera")
	bool bReturnCameraAfterShow;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0"))
	float CameraMoveSeconds;

	/** Camera distance per unit of gun-to-target distance; the camera's own zoom limits still apply. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0"))
	float CameraZoomPerDistance;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0"))
	float CameraZoomPadding;

	// --- Tank division formation ---

	/** Extra scale on every division tank, on top of the fit to Tank Piece Target Length. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks", meta = (ClampMin = "0.01"))
	float TankPieceScale;

	/**
	 * Each tank mesh is scaled so its longest horizontal side is this long in map units, whatever size it was
	 * authored at. 0 = use the mesh at Tank Piece Scale only.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks", meta = (ClampMin = "0"))
	float TankPieceTargetLength;

	/** Yaw added to every tank, for meshes whose nose does not point along +X. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks")
	float TankPieceYawOffset;

	/** Distance between two tanks side by side. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks", meta = (ClampMin = "1"))
	float FormationColumnSpacing;

	/** Distance between two rows. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks", meta = (ClampMin = "1"))
	float FormationRowSpacing;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks", meta = (ClampMin = "1"))
	int32 FormationMaxPerRow;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks")
	TMap<ETankSubtype, TSoftObjectPtr<UStaticMesh>> TankPieceMeshes;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks")
	TMap<ETankSubtype, TSoftObjectPtr<UStaticMesh>> TankWreckMeshes;

	/** Used for a subtype without its own mesh, or whose mesh is missing. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks")
	TSoftObjectPtr<UStaticMesh> FallbackTankPieceMesh;

	// A dark plate under the tanks, so a division reads as one shape against the country colour. The mesh is a
	// 100 x 100 x 100 unit block with its base at height 0; it is stretched to the formation footprint.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks")
	TSoftObjectPtr<UStaticMesh> FormationPlateMesh;

	/** How far the plate reaches past the outermost tank centres, along both sides. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks", meta = (ClampMin = "0"))
	float FormationPlatePadding;

	/** Height of the plate; the tanks stand on top of it. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Division Tanks", meta = (ClampMin = "0"))
	float FormationPlateHeight;

	// --- Effects ---

	/** Optional: the game's shells are a trail only. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<UStaticMesh> ShellMesh;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0.001"))
	float ShellMeshScale;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<UNiagaraSystem> MuzzleEffect;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0.001"))
	float MuzzleEffectScale;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<UNiagaraSystem> TrailEffect;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0.001"))
	float TrailEffectScale;

	/** Value of the trail's ShellWidth user parameter. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0"))
	float TrailShellWidth;

	/** Value of the trail's ShellColor user parameter. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects")
	FLinearColor TrailShellColor;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0.001"))
	float ImpactEffectScale;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<UNiagaraSystem> TankExplosionEffect;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0.001"))
	float TankExplosionEffectScale;

	/** Optional fire left burning on a wreck until the wrecks are cleared. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<UNiagaraSystem> WreckFireEffect;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0.001"))
	float WreckFireEffectScale;

	// --- Sounds ---

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> LaunchSound;

	/** Optional whistle, played once when the first shell starts to fall. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> DescentSound;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> TankExplosionSound;
};
