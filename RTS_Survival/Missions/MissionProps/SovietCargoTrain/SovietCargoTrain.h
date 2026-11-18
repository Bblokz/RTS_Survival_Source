// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SovietCargoTrain.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class ASovietCargoTrain;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSovietCargoTrainEvent, ASovietCargoTrain* /*Train*/);

/** Where the train is in its run. */
enum class ESovietCargoTrainState : uint8
{
	Idle,
	Arriving,
	Arrived,
	Departing,
	Departed
};

/** Which class default a car takes its mesh and forward-axis fix from. */
enum class ESovietCargoTrainCarType : uint8
{
	Locomotive,
	Flatcar,
	GoodsWagon
};

/** World-space polyline with cumulative distances; samples outside [0, length] extrapolate straight and level. */
struct FSovietCargoTrainPath
{
	TArray<FVector> Points;
	// Distance along the path at each point; Distances[0] = 0.
	TArray<float> Distances;

	bool Build(const TArray<FVector>& InPoints);
	float GetLength() const;
	FVector GetLocationAtDistance(const float Distance) const;
	float GetClosestDistanceToLocation(const FVector& Location) const;
};

/** One car of the consist: its mesh component, size along the track and the mesh-to-car-frame alignment. */
struct FSovietCargoTrainCar
{
	TWeakObjectPtr<UStaticMeshComponent> MeshComponent;
	// Cargo riding on this car (flatcars only); attached to MeshComponent.
	TWeakObjectPtr<UStaticMeshComponent> CargoComponent;
	// Car mesh local -> car frame (X forward along the track, origin = footprint centre at rail height).
	FTransform MeshToCarFrame = FTransform::Identity;
	float Length = 0.f;
	// Path distance from the train front to this car's front end (0 for the locomotive).
	float FrontOffset = 0.f;
};

/**
 * @brief Set-dressing cargo train for missions: a locomotive, flatcars carrying static cargo meshes (tanks) and
 * goods wagons that roll in along a world-space rail path, stop, and roll back out the way they came.
 * Not a unit: no selection, no targeting, no collision, no navigation. Each car is a runtime static mesh component
 * placed every tick by path distance and oriented along the chord between its two bogie points.
 * @note Spawn the Blueprint child (BP_SovietCargoTrain, meshes set in the class defaults) and call StartRun;
 * on OnArrived the mission hides the cargo (HideCargo), spawns the real units and calls Depart.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASovietCargoTrain : public AActor
{
	GENERATED_BODY()

public:
	ASovietCargoTrain(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Builds the consist (locomotive in front, then one flatcar per cargo mesh, then NumGoodsWagons goods wagons),
	 * places it so its front is at the start of the path with the cars trailing off-map behind the path start
	 * (extrapolate straight back along the start tangent), and drives the front along the path until it is at the
	 * path distance closest to StopFrontLocation (ease in/out), then broadcasts OnArrived.
	 * @param WorldPathPoints World-space points, map edge first, station last (z = rail height).
	 * @param StopFrontLocation The front of the locomotive stops at the path distance closest to this point.
	 * @param FlatcarCargoMeshes One flatcar per entry; a null entry is an empty flatcar.
	 * @param NumGoodsWagons Goods wagons coupled behind the last flatcar.
	 */
	UFUNCTION(BlueprintCallable, Category="Cargo Train")
	void StartRun(const TArray<FVector>& WorldPathPoints, const FVector& StopFrontLocation,
	              const TArray<UStaticMesh*>& FlatcarCargoMeshes, const int32 NumGoodsWagons);

	/** Hides every cargo mesh on the flatcars. */
	UFUNCTION(BlueprintCallable, Category="Cargo Train")
	void HideCargo();

	/** Drives back out along the path (reverse) and destroys itself once the whole train is past the path start. */
	UFUNCTION(BlueprintCallable, Category="Cargo Train")
	void Depart();

	UFUNCTION(BlueprintPure, Category="Cargo Train")
	bool GetHasArrived() const;

	/** @return World bounds centres of the cargo meshes (hidden ones included), front flatcar first. */
	UFUNCTION(BlueprintPure, Category="Cargo Train")
	TArray<FVector> GetCargoWorldLocations() const;

	FOnSovietCargoTrainEvent OnArrived;
	// Broadcast right before the train destroys itself.
	FOnSovietCargoTrainEvent OnDeparted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ------------------------------------------------------------------ meshes
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train")
	TObjectPtr<UStaticMesh> LocomotiveMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train")
	TObjectPtr<UStaticMesh> FlatcarMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train")
	TObjectPtr<UStaticMesh> GoodsWagonMesh = nullptr;

	// Each mesh is turned so its long bounds axis runs along the track, with its +X (or +Y) end in front.
	// These add a yaw on top of that fix, e.g. 180 when a mesh's front is its -X end.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Alignment")
	float LocomotiveYawOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Alignment")
	float FlatcarYawOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Alignment")
	float GoodsWagonYawOffset = 0.f;

	// Added to every cargo mesh after its own long-axis fix (180 turns the tanks to face the rear).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Alignment")
	float CargoYawOffset = 0.f;

	// Applied to each cargo mesh around its bounds' bottom centre, in the flatcar frame (X forward, Z up),
	// before it is set on the deck. Use the location for nudges and the scale to shrink tanks onto the deck.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Alignment")
	FTransform CargoOffset = FTransform::Identity;

	// Deck height above the rail where the cargo's bottom sits; <= 0 uses the top of the flatcar mesh bounds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Alignment")
	float FlatcarDeckHeight = 0.f;

	// Lifts every car above the path points (the bounds bottom of each car sits on the path z plus this).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Alignment")
	float RailHeightOffset = 0.f;

	// ------------------------------------------------------------------ motion
	// Speed (cm/s) while rolling in and out.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Motion", meta=(ClampMin="10"))
	float CruiseSpeed = 900.f;

	// Braking distance before the stop and run-up distance when departing.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Motion", meta=(ClampMin="10"))
	float EaseDistance = 2500.f;

	// Lowest speed during the ease so the train never stalls just short of the stop.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Motion", meta=(ClampMin="1"))
	float MinEaseSpeed = 40.f;

	// Gap between the bounds of coupled cars.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Motion", meta=(ClampMin="0"))
	float GapBetweenCars = 120.f;

	// Bogie points sit at car centre +- this fraction of the car length; the car follows the chord between them.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Motion", meta=(ClampMin="0.05", ClampMax="0.5"))
	float BogieFraction = 0.35f;

	// Hides a car while its centre is behind the path start (past the end of the rails, off-map).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Motion")
	bool bHideCarsBeyondPathStart = false;

	// ------------------------------------------------------------------ sound
	// Optional looping engine sound on the locomotive; fades out at the stop and back in on Depart.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Sound")
	TObjectPtr<USoundBase> EngineLoopSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Sound")
	TObjectPtr<USoundAttenuation> EngineSoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo Train|Sound", meta=(ClampMin="0"))
	float EngineSoundFadeSeconds = 1.5f;

private:
	UPROPERTY(VisibleAnywhere, Category="Cargo Train|Components")
	TObjectPtr<USceneComponent> M_Root = nullptr;

	// Car and cargo components created by StartRun; kept here so they are not garbage collected.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_CarComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_CargoComponents;

	UPROPERTY()
	TObjectPtr<UAudioComponent> M_EngineAudio = nullptr;

	FSovietCargoTrainPath M_Path;
	TArray<FSovietCargoTrainCar> M_Cars;

	ESovietCargoTrainState M_State = ESovietCargoTrainState::Idle;
	// Path distance of the locomotive's front end.
	float M_FrontDistance = 0.f;
	float M_StopDistance = 0.f;
	bool bM_CargoHidden = false;

	bool GetIsValidConsistMeshes(const int32 NumGoodsWagons) const;

	void StartRun_BuildConsist(const TArray<UStaticMesh*>& FlatcarCargoMeshes, const int32 NumGoodsWagons);
	void AddCar(const ESovietCargoTrainCarType CarType, UStaticMesh* CargoMesh, float& InOutFrontOffset);
	UStaticMeshComponent* CreateMeshComponent(UStaticMesh* Mesh, USceneComponent* AttachParent);
	void AttachCargoToCar(FSovietCargoTrainCar& Car, UStaticMesh* CargoMesh);

	UStaticMesh* GetCarMesh(const ESovietCargoTrainCarType CarType) const;
	float GetCarYawOffset(const ESovietCargoTrainCarType CarType) const;

	/**
	 * @brief Transform that turns a mesh's long bounds axis onto +X and puts its bounds' bottom centre at the origin.
	 * @param Mesh The mesh to align.
	 * @param ExtraYaw Yaw added after the long-axis fix.
	 * @param OutLength Mesh size along +X after the turn.
	 * @return Mesh local -> aligned frame.
	 */
	static FTransform GetBottomCentreAlignment(const UStaticMesh* Mesh, const float ExtraYaw, float& OutLength);

	/** @return Car frame (X towards the train front, Z up) for a car whose centre is at this path distance. */
	FTransform GetCarFrameAtDistance(const float CentreDistance, const float CarLength) const;

	void ApplyConsistAtFrontDistance() const;

	/** @return Speed for this tick; eases near the stop (arriving) and away from it (departing). */
	float GetEasedSpeed() const;

	void Tick_Arriving(const float DeltaSeconds);
	void Tick_Departing(const float DeltaSeconds);

	void StartEngineSound();
	void FadeEngineSound(const bool bFadeIn) const;
};
