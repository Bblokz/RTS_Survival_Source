// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "RTSAmbientFxSettings.generated.h"

class UNiagaraSystem;

/**
 * @brief Tuning for the ambient landscape effects (dust devils over sand), drawn by ULandscapeAmbientFxSubsystem
 * with one Niagara system for the whole map. Edit in Project Settings -> Game -> RTS Ambient Effects.
 * The player picks the level (Disabled / Low / Medium / Maximum) in the graphics settings.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Ambient Effects"))
class RTS_SURVIVAL_API URTSAmbientFxSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSAmbientFxSettings();

	static const URTSAmbientFxSettings* Get();

	// The baked sand mask asset of a map: <SandMaskFolder>/SandMask_<MapName>.
	FSoftObjectPath GetSandMaskPath(const FString& MapName) const;

	/**
	 * The one Niagara system that draws every dust devil; its emitters read the devils from user arrays.
	 * Rebuild its emitter scripts with the editor console command RTS.AmbientFx.PatchNiagaraSystem.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils")
	TSoftObjectPtr<UNiagaraSystem> DustDevilNiagaraSystem;

	// Most dust devils alive at the same time around the camera, per graphics setting (Low, Medium, Maximum).
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0", ClampMax="16"))
	int32 MaxDustDevilsLow = 1;

	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0", ClampMax="16"))
	int32 MaxDustDevilsMedium = 3;

	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0", ClampMax="16"))
	int32 MaxDustDevilsMaximum = 6;

	// Dust devils start within this distance of the point the camera looks at, in cm; the RTS camera sees about
	// 65 x 36 m, so most start in view or just outside it and drift in.
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="1000"))
	float SpawnRadius = 6000.f;

	// A dust devil further than this from the camera focus fades out, in cm.
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="1000"))
	float DespawnRadius = 10000.f;

	/**
	 * Seconds between two new dust devils, per graphics setting (Low, Medium, Maximum); this sets how often devils
	 * appear. With a devil living about 37 s, on average Low has 0.2, Medium 0.7 and Maximum 1.4 devils alive; the
	 * counts above only cap a lucky streak.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0.1"))
	FFloatInterval SpawnIntervalLow = FFloatInterval(120.f, 210.f);

	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0.1"))
	FFloatInterval SpawnIntervalMedium = FFloatInterval(40.f, 70.f);

	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0.1"))
	FFloatInterval SpawnIntervalMaximum = FFloatInterval(20.f, 35.f);

	// How long one dust devil lives, in seconds.
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="5"))
	FFloatInterval Lifetime = FFloatInterval(25.f, 50.f);

	// How fast a dust devil drifts over the ground, in cm/s.
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0"))
	FFloatInterval DriftSpeed = FFloatInterval(150.f, 400.f);

	// Size of a dust devil; 1 is a column about 14 m high.
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils", meta=(ClampMin="0.1"))
	FFloatInterval Scale = FFloatInterval(0.75f, 1.3f);

	// Direction the devils drift and lean towards, in degrees around up (0 = +X); each devil wanders around it.
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils")
	float WindYawDegrees = 35.f;

	// Colour of the dust: linear albedo of the lifted sand.
	UPROPERTY(EditAnywhere, Config, Category="Dust Devils")
	FLinearColor DustColor = FLinearColor(0.42f, 0.33f, 0.22f, 1.f);

	// Size of the grid cells the map is checked in for sand ground, in cm (baked masks and the trace fallback).
	UPROPERTY(EditAnywhere, Config, Category="Sand Ground", meta=(ClampMin="500"))
	float SandCellSize = 2500.f;

	/**
	 * Folder of the baked sand masks: SandMask_<MapName> data assets made with the editor console command
	 * RTS.AmbientFx.BakeSandMask while the map is open. A map with a mask gets devils only on its sand cells.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Sand Ground", meta=(ContentDir))
	FDirectoryPath SandMaskFolder;

	// Landscape paint layers that count as sand when a mask is baked; a cell is sand when they cover half of it.
	UPROPERTY(EditAnywhere, Config, Category="Sand Ground")
	TArray<FName> SandLayerNames = {
		TEXT("Sand"), TEXT("AridSoil"), TEXT("Desert"), TEXT("GraySand"), TEXT("Standard Sand"), TEXT("White Sand"),
		TEXT("RTSSand"), TEXT("Dunes")
	};

	/**
	 * Without a baked mask: physical surface types the landscape returns on ground where dust devils may form
	 * (SurfaceType2 is Sand in DefaultEngine.ini). The landscape layers carry no physical materials today, so maps
	 * need a baked mask.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Sand Ground")
	TArray<TEnumAsByte<EPhysicalSurface>> SandSurfaceTypes = {EPhysicalSurface::SurfaceType2};

	// Maps that should have no dust devils even though their ground is sand, for example indoor or night maps.
	UPROPERTY(EditAnywhere, Config, Category="Sand Ground")
	TArray<TSoftObjectPtr<UWorld>> ExcludedMaps;
};
