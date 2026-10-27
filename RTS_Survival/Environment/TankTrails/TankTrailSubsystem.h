// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TankTrailSubsystem.generated.h"

class ATrackedTankMaster;
class UMaterialParameterCollection;
class UTextureRenderTarget2D;
struct FCanvasUVTri;

/** Where the two tracks of one tank touched the ground when its trail was last drawn. */
struct FTankTrailVehicle
{
	TWeakObjectPtr<ATrackedTankMaster> Tank;
	FVector2D LastLeftTrack = FVector2D::ZeroVector;
	FVector2D LastRightTrack = FVector2D::ZeroVector;
	// Sideways distance from the actor origin to each track centre line, in cm; negative until measured.
	float HalfTrackSpacing = -1.f;
	float TrackWidth = 0.f;
	float TrackCentreOffset = 0.f;
	bool bHasLastTracks = false;
};

/** What the trail writer did so far; read by the trail benchmark. */
struct FTankTrailStats
{
	int32 Updates = 0;
	int32 SegmentsDrawn = 0;
	int32 FadePasses = 0;
	double UpdateSeconds = 0.0;
};

/**
 * @brief Draws the ruts of every tracked tank into one render target that covers the landscape, so the landscape
 * material can darken and dent the ground where tanks drove (Docs/Environment/TankTrails.md).
 * All tanks are gathered into a single batched canvas draw a few times per second; the landscape samples the target
 * behind a uniform branch, so nothing is fetched while the system is off.
 * Active when the landscape material has TankTrailStrength above zero; rts.TankTrails 0 turns it off at runtime.
 */
UCLASS()
class RTS_SURVIVAL_API UTankTrailSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	bool GetIsTrailRenderingActive() const { return bM_IsActive && bM_IsEnabledByConsole; }
	const FTankTrailStats& GetStats() const { return M_Stats; }
	UTextureRenderTarget2D* GetTrailRenderTarget() const { return M_RenderTarget; }

private:
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> M_ParameterCollection = nullptr;

	TArray<FTankTrailVehicle> M_Vehicles;
	FTankTrailStats M_Stats;
	FDelegateHandle M_ActorSpawnedHandle;

	FVector2D M_BoundsMin = FVector2D::ZeroVector;
	FVector2D M_BoundsSize = FVector2D::ZeroVector;
	FIntPoint M_TargetSize = FIntPoint::ZeroValue;
	float M_SecondsSinceUpdate = 0.f;
	float M_SecondsSinceFade = 0.f;
	float M_TrailStrength = 0.f;
	bool bM_IsActive = false;
	bool bM_IsEnabledByConsole = false;

	bool GetIsValidRenderTarget() const;
	bool GetIsValidParameterCollection() const;

	/**
	 * @brief Reads the landscape: its bounds become the trail area, its material decides whether trails are drawn.
	 * @param InWorld World that begins play.
	 * @return False when there is no landscape or its material does not show trails.
	 */
	bool BeginPlay_ReadLandscape(UWorld& InWorld);
	bool BeginPlay_SetupRenderTarget();
	void BeginPlay_RegisterTanks(UWorld& InWorld);

	void OnActorSpawned(AActor* SpawnedActor);
	void RegisterTank(ATrackedTankMaster* Tank);

	// Pushes the trail area and the on/off state to the parameter collection the landscape reads.
	void ApplyMaterialParameters(bool bShowTrails) const;
	void UpdateConsoleState();

	void UpdateTrails();
	static void MeasureTracks(FTankTrailVehicle& Vehicle, const ATrackedTankMaster& Tank);
	void AddTrackSegment(const FVector2D& From, const FVector2D& To, float TrackWidth,
	                     TArray<FCanvasUVTri>& OutTriangles) const;

	/**
	 * @brief One piece of rut with soft side edges whose depth can ramp along its length, so trail ends slope out.
	 * @param Start World XY where the strip begins.
	 * @param End World XY where the strip ends.
	 * @param Side Unit vector across the track.
	 * @param TrackWidth Rut width in cm.
	 * @param StartDepth Rut coverage at Start, 0 to 1.
	 * @param EndDepth Rut coverage at End, 0 to 1.
	 * @param OutTriangles Receives the strip's canvas triangles in render target pixels.
	 */
	void AddRutStrip(const FVector2D& Start, const FVector2D& End, const FVector2D& Side, float TrackWidth,
	                 float StartDepth, float EndDepth, TArray<FCanvasUVTri>& OutTriangles) const;
	FVector2D WorldToPixel(const FVector2D& WorldLocation) const;
	void DrawTriangles(const TArray<FCanvasUVTri>& Triangles) const;
	void DrawFade(float FadeAlpha) const;
};
