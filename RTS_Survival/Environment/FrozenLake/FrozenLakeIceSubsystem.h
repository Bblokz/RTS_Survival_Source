// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "FrozenLakeIceSubsystem.generated.h"

class ATankMaster;
class UCanvas;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture;
class UTextureRenderTarget2D;

/** Where a vehicle stands in the ice-break sequence. */
UENUM()
enum class EFrozenLakeIceVehicleState : uint8
{
	// On ground or ice that holds its class.
	Safe,
	// On ice too weak for its class; breaks if it stays.
	Stressed,
	// The ice glows and cracks under it; it can still drive off.
	Warning,
	// The ice opened: the vehicle goes under.
	Sinking,
	// Under the ice and dead.
	Sunk
};

/** One nav box of the lake (FL_IceNav_*): the vehicle classes it holds out are the classes the ice under it breaks for. */
struct FFrozenLakeIceArea
{
	FBox Bounds = FBox(ForceInit);
	// Bit per ERTSNavAgents value: set when that class is not allowed in the box.
	uint8 BlockedAgentsMask = 0;
};

/** A soft disc drawn into the ice state mask: the warning glow (G) or open water (B). */
struct FFrozenLakeIceDisc
{
	FVector2D Centre = FVector2D::ZeroVector;
	float Radius = 0.f;
};

/** A vehicle the ice watches. */
struct FFrozenLakeIceVehicle
{
	TWeakObjectPtr<ATankMaster> Tank;
	ERTSNavAgents Agent = ERTSNavAgents::NONE;
	EFrozenLakeIceVehicleState State = EFrozenLakeIceVehicleState::Safe;
	float SecondsInState = 0.f;
	FVector2D BreakCentre = FVector2D::ZeroVector;
	float HoleRadius = 0.f;
	FVector SinkStartLocation = FVector::ZeroVector;
	FRotator SinkStartRotation = FRotator::ZeroRotator;
	// How far the hull goes down: to the lake bed under it, so it rests there half buried.
	float SinkDepth = 0.f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFrozenLakeIceVehicleStateChanged, ATankMaster* /*Tank*/,
                                     EFrozenLakeIceVehicleState /*NewState*/);

/**
 * @brief Makes the Frozen Lake ice break under vehicles too heavy for it: stress, a glowing warning, then a hole the
 * vehicle sinks through and dies in. The ice that breaks is exactly the ice the nav keeps that class off (the
 * FL_IceNav_* box components with per-agent meta areas), so nothing new has to be placed in the map.
 * Holes are drawn into a runtime copy of the ice's state mask (B channel), get a water plane and a Null nav box.
 * Active on any map whose ice surface material has a StateMask parameter and whose dynamic-obstacle boxes use
 * NavAreaMeta_SwitchByAgent areas (Docs/Campaign/FrozenLakeMap.md, "Ice breaking").
 */
UCLASS()
class RTS_SURVIVAL_API UFrozenLakeIceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	bool GetIsActive() const { return bM_IsActive; }
	int32 GetHoleCount() const { return M_Holes.Num(); }
	FOnFrozenLakeIceVehicleStateChanged& OnVehicleStateChanged() { return M_OnVehicleStateChanged; }

	/**
	 * @brief Whether a vehicle class standing at this spot is on ice that does not hold it.
	 * @param Agent Nav class of the vehicle.
	 * @param Location World location (Z must be near the ice surface).
	 * @return True when one of the lake's nav boxes keeps that class out here.
	 */
	bool GetIsIceTooWeakFor(ERTSNavAgents Agent, const FVector& Location) const;

	EFrozenLakeIceVehicleState GetVehicleState(const ATankMaster* Tank) const;
	UStaticMeshComponent* GetIceSurface() const { return M_IceSurface.Get(); }
	float GetIceSurfaceZ() const { return M_IceSurfaceZ; }
	const TArray<FFrozenLakeIceDisc>& GetHoles() const { return M_Holes; }

	// Test hook: shows the authored state texture (true) or the runtime copy with the holes (false) on the ice.
	void SetShowAuthoredStateMask(bool bShowAuthored) const;

private:
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_StateTarget = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_IceMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture> M_AuthoredStateMask = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> M_HoleActors;

	TWeakObjectPtr<UStaticMeshComponent> M_IceSurface;
	TArray<FFrozenLakeIceArea> M_Areas;
	TArray<FFrozenLakeIceVehicle> M_Vehicles;
	TArray<FFrozenLakeIceDisc> M_Holes;
	FOnFrozenLakeIceVehicleStateChanged M_OnVehicleStateChanged;
	FDelegateHandle M_ActorSpawnedHandle;

	// State mask mapping (the ice material's LakeOrigin / LakeSize): uv = (XY - origin) / size.
	FVector2D M_LakeOrigin = FVector2D::ZeroVector;
	float M_LakeSize = 0.f;
	float M_IceSurfaceZ = 0.f;
	float M_SecondsSinceCheck = 0.f;
	bool bM_IsActive = false;
	bool bM_IsStateMaskDirty = false;

	bool GetIsValidStateTarget() const;
	bool GetIsValidIceMaterial() const;

	/**
	 * @brief Finds the ice surface: the static mesh whose material reads a world-mapped StateMask.
	 * @param InWorld World that begins play.
	 * @return False when the map has no such ice.
	 */
	bool BeginPlay_FindIceSurface(UWorld& InWorld);
	void BeginPlay_ReadNavAreas(UWorld& InWorld);
	bool BeginPlay_SetupStateTarget();
	void BeginPlay_RegisterVehicles(UWorld& InWorld);

	/**
	 * @brief Which vehicle classes a nav area keeps out, one bit per ERTSNavAgents value.
	 * @param AreaClass The box's area: NavArea_Null (everyone) or a NavAreaMeta_SwitchByAgent child.
	 * @param InWorld World whose navigation system names the agents.
	 * @return The mask; 0 when the area keeps nobody out.
	 */
	static uint8 GetBlockedAgentsMask(const UClass* AreaClass, const UWorld& InWorld);
	static uint8 GetAgentBit(ERTSNavAgents Agent);

	void OnActorSpawned(AActor* SpawnedActor);
	void RegisterVehicle(ATankMaster* Tank);

	void CheckVehicles(float DeltaSeconds);
	void UpdateVehicle(FFrozenLakeIceVehicle& Vehicle, float DeltaSeconds);
	void SetVehicleState(FFrozenLakeIceVehicle& Vehicle, EFrozenLakeIceVehicleState NewState);
	void BreakIce(FFrozenLakeIceVehicle& Vehicle, ATankMaster& Tank);
	static float GetHullRadius(const ATankMaster& Tank);
	float GetSinkDepth(const ATankMaster& Tank) const;
	static void DisableForSinking(ATankMaster& Tank);
	void UpdateSinking(FFrozenLakeIceVehicle& Vehicle, float DeltaSeconds);
	static void FinishSinking(ATankMaster& Tank);

	/**
	 * @brief The hole's water plane and its Null nav box, so units route round it and see water in it.
	 * @param Centre Hole centre in world XY.
	 * @param Radius Hole radius in cm.
	 */
	void SpawnHoleActor(const FVector2D& Centre, float Radius);

	// Redraws the runtime state mask: the authored texture, then the warning glows (G) and holes (B) on top.
	void RedrawStateMask();
	TArray<FFrozenLakeIceDisc> GetWarningDiscs() const;
	void DrawDiscs(UCanvas& Canvas, const TArray<FFrozenLakeIceDisc>& Discs, const FLinearColor& Colour) const;
	FVector2D WorldToStatePixel(const FVector2D& WorldLocation) const;
};
