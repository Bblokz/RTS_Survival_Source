// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionDebriefTypes.h"

#include "MissionDebriefStage.generated.h"

class APlayerController;
class UCameraComponent;
class UDirectionalLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture;

DECLARE_DELEGATE(FOnMissionDebriefButtonsDue);

/** One CPU sprite of the burn (flame, ember or smoke); positions are in stage units (1 unit = 10 cm, y up). */
struct FMissionDebriefParticle
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Life = 1.f;
	float MaxLife = 0.f;
	float Wobble = 0.f;
	float Seed = 0.f;
	float SizeScale = 1.f;
};

enum class EMissionDebriefParticleKind : uint8
{
	Ember,
	Flame,
	Smoke
};

/** A pool of CPU particles drawn as camera-facing quads on one instanced mesh with RGBA custom data. */
struct FMissionDebriefParticlePool
{
	EMissionDebriefParticleKind Kind = EMissionDebriefParticleKind::Ember;
	TArray<FMissionDebriefParticle> Particles;
	int32 NextIndex = 0;
	float SpawnAccumulator = 0.f;
};

/**
 * @brief The mission end screen's 3D debrief: a lit field desk off the map where a steel seal stamps the operational
 * order with VICTORY, or stamps DEFEAT and the order burns down to the stamped letters. The mission manager spawns it
 * when the mission ends (the world is paused), calls StartDebrief, and shows its buttons when OnButtonsDue fires.
 * Everything runs on real time from Tick (ticks while paused): stamp, camera, ink, burn, lights and the CPU-simulated
 * flames, embers and smoke. Layout and timing come from the approved mock-up (Source/Tools/MissionDebrief).
 */
UCLASS()
class RTS_SURVIVAL_API AMissionDebriefStage : public AActor
{
	GENERATED_BODY()

public:
	AMissionDebriefStage();

	/**
	 * @brief Loads the outcome's assets, takes over the player's view and starts the timeline.
	 * @param Outcome Victory stamps VICTORY; defeat stamps DEFEAT and burns the order.
	 * @param Viewer Player whose view becomes the stage camera.
	 * @return False when the stage could not be set up (missing assets); the caller then shows the buttons at once.
	 */
	bool StartDebrief(const EMissionDebriefOutcome Outcome, APlayerController* Viewer);

	// Fires once, when the timeline reaches the moment the buttons should appear.
	FOnMissionDebriefButtonsDue OnButtonsDue;

	float GetElapsedSeconds() const { return M_ElapsedSeconds; }
	bool GetIsRunning() const { return bM_IsRunning; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------------- assets (defaults point at /Game/RTS_Survival/Blueprints/Missions/MissionDebrief) ----------------
	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> SetMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> StampBodyMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> PaperMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> SootMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> TinPadVictoryMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> TinPadDefeatMesh;

	// Mirrored faces are shown for the slam; the readable ones after the spin.
	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> FaceVictoryMirroredMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> FaceVictoryReadableMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> FaceDefeatMirroredMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> FaceDefeatReadableMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> Mp40Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Meshes")
	TSoftObjectPtr<UStaticMesh> Mp44Mesh;

	// The in-game StG 44 material only uses its albedo; the stage uses all of its PBR maps.
	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Materials")
	TSoftObjectPtr<UMaterialInterface> Mp44Material;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Materials")
	TSoftObjectPtr<UMaterialInterface> PaperMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Materials")
	TSoftObjectPtr<UMaterialInterface> SootMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Materials")
	TSoftObjectPtr<UMaterialInterface> FlameMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Materials")
	TSoftObjectPtr<UMaterialInterface> EmberMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Materials")
	TSoftObjectPtr<UMaterialInterface> SmokeMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Materials")
	TSoftObjectPtr<UMaterialInterface> EnclosureMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Textures")
	TSoftObjectPtr<UTexture> InkVictoryTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Textures")
	TSoftObjectPtr<UTexture> InkDefeatTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Textures")
	TSoftObjectPtr<UTexture> InkHeightVictoryTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Textures")
	TSoftObjectPtr<UTexture> InkHeightDefeatTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Burn")
	TSoftObjectPtr<UMissionDebriefBurnField> BurnField;

	// ---------------- look ----------------
	// Camera exposure is fixed (no eye adaptation): the stage is always seen with this bias.
	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float ExposureBias = 0.6f;

	// Lamp, wall spill and fill intensities (candela for spot/point lights, lux for the directional fill and rim).
	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float LampIntensity = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float LampBurnIntensity = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float WallWashIntensity = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float LampGlowIntensity = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float FillIntensity = 1.7f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float FillBurnIntensity = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float RimIntensity = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float AmbientIntensity = 0.12f;

	// Fire light over the burning order while the page burns, and while the letters' edges keep burning.
	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float FireBurnIntensity = 0.07f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float FireRidgeIntensity = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float SpriteGain = 1.f;

	// Bump of the stamped ink relief in the paper material.
	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float InkBumpStrength = 9.f;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief|Look")
	float InkBumpSignY = 1.f;

private:
	// ---------------- components ----------------
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> M_Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> M_Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_SetComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_TinPadComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_PaperComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_SootComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> M_StampRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_StampBodyComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_FaceMirroredComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_FaceReadableComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_Mp40Component;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_Mp44Component;

	// Closed black room around the desk: keeps the level's sky and sun out of the stage.
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> M_EnclosureComponents;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpotLightComponent> M_LampLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpotLightComponent> M_WallWashLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> M_LampGlowLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> M_FireLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> M_AmbientLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> M_FillLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> M_RimLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> M_FlameSprites;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> M_EmberSprites;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> M_SmokeSprites;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_PaperMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_SootMaterial;

	UPROPERTY()
	TObjectPtr<UMissionDebriefBurnField> M_BurnField;

	TWeakObjectPtr<APlayerController> M_Viewer;

	// ---------------- state ----------------
	EMissionDebriefOutcome M_Outcome = EMissionDebriefOutcome::Victory;
	bool bM_IsRunning = false;
	bool bM_HasFiredButtonsDue = false;
	// Post process materials were on when the debrief took the view; EndPlay turns them back on.
	bool bM_HasDisabledPostProcessMaterials = false;
	float M_ElapsedSeconds = 0.f;
	double M_LastRealSeconds = 0.0;
	// RTS.Debrief.LightScale when the debrief started.
	float M_LightScale = 1.f;

	FMissionDebriefParticlePool M_Flames;
	FMissionDebriefParticlePool M_Embers;
	FMissionDebriefParticlePool M_Smoke;

	// ---------------- set-up ----------------
	void Constructor_CreateMeshComponents();
	void Constructor_CreateLights();
	void Constructor_CreateSprites();
	void Constructor_SetDefaultAssetPaths();

	bool StartDebrief_LoadMeshes();
	bool StartDebrief_SetupPaper();
	void StartDebrief_ApplyLookDevSwitches();
	void StartDebrief_PlaceStaticPieces();
	void StartDebrief_PlaceLights();
	void StartDebrief_SetupCamera();
	void StartDebrief_SetupSprites();
	void StartDebrief_TakeOverView();

	// ---------------- timeline ----------------
	void TickTimeline(const float DeltaSeconds);
	void TickCamera(const float Time);
	void TickStamp(const float Time);
	void TickPaperAndLights(const float Time, const float DeltaSeconds);
	void TickBurnParticles(const float Time, const float DeltaSeconds, const float BurnAmount);
	void TickParticlePool(FMissionDebriefParticlePool& Pool, UInstancedStaticMeshComponent* Sprites, const float DeltaSeconds,
	                      const float SpawnRate, const float BurnAmount, const float Tolerance, const bool bUseWordBox);

	bool SampleBurnEdge(const float BurnAmount, const float Tolerance, const bool bUseWordBox, FVector& OutStagePosition) const;
	void UpdateSpriteInstances(const FMissionDebriefParticlePool& Pool, UInstancedStaticMeshComponent* Sprites) const;

	bool GetIsVictory() const { return M_Outcome == EMissionDebriefOutcome::Victory; }
	bool GetIsValidViewer() const;
	bool GetIsValidBurnField() const;
};
