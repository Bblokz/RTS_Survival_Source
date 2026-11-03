// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AirDroppedMine.generated.h"

class AFieldMine;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;

/** @brief Looks and timing of a mine falling from an aircraft; set on the minefield ability template. */
USTRUCT(BlueprintType)
struct FAirDroppedMineSettings
{
	GENERATED_BODY()

	// The mine under its parachute; hangs along -Z from its pivot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> FallingMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> ParachuteMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float MeshScale = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float FallSeconds = 3.f;

	// Played where the mine lands: a puff of dust.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> LandingEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> LandingSound = nullptr;
};

/**
 * @brief A mine on its way down from an aircraft. It is not a mine yet: the real AFieldMine is spawned where this
 * lands, so nothing can trigger in the air. Spawned from code only.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AAirDroppedMine : public AActor
{
	GENERATED_BODY()

public:
	AAirDroppedMine();

	/**
	 * @brief Starts the fall; call once right after spawning.
	 * @param ReleaseLocation Where it leaves the aircraft.
	 * @param GroundLocation Where the mine ends up.
	 * @param MineClass The mine to place on landing.
	 * @param OwningPlayer Player the mine belongs to; other players' mines stay out of the human player's sight.
	 * @param Settings Looks and timing.
	 */
	void StartFall(const FVector& ReleaseLocation, const FVector& GroundLocation, UClass* MineClass,
	               const uint8 OwningPlayer, const FAirDroppedMineSettings& Settings);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_FallingMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ParachuteMesh;

	UPROPERTY()
	TObjectPtr<UClass> M_MineClass;

	UPROPERTY()
	FAirDroppedMineSettings M_Settings;

	FVector M_ReleaseLocation = FVector::ZeroVector;
	FVector M_GroundLocation = FVector::ZeroVector;
	float M_SecondsFalling = 0.f;
	uint8 M_OwningPlayer = 0;

	void Land();
	void PlaceMine() const;
};
