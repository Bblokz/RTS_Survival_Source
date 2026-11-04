#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckDebrisSettings.h"

#include "WreckDebrisBurstComponent.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;
class URTSFireSubsystem;

/** A thrown debris piece and how long it has lain still. */
USTRUCT()
struct FWreckDebrisFlyingPiece
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> Mesh;

	float TimeBelowSettleSpeed = 0.f;
	bool bIsSettled = false;
};

/**
 * @brief Added to a wreck when its ammunition or fuel detonates: throws the pieces of one burst profile
 * (UWreckDebrisSettings) out of the hull with physics, sets a few on fire, then freezes each piece with collision
 * off once it lies still, so the landed debris is only decoration around the wreck.
 */
UCLASS(ClassGroup=(RTS))
class RTS_SURVIVAL_API UWreckDebrisBurstComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWreckDebrisBurstComponent();

	/**
	 * @brief Throws the debris; the count grows with the hull's size so a Tiger sheds more than a Panzer II.
	 * @param Origin World location the pieces burst from (the turret ring or the engine deck).
	 * @param HullMesh Wreck hull: its size sets the count and its material paints the armour chunks.
	 * @param CountScale Per-tank multiplier on the piece count.
	 * @param Profile Which pieces fly and how; copied, so a temporary is fine.
	 * @param BurstDirection Horizontal direction the profile's DirectionBias throws towards; zero throws all around.
	 */
	void StartBurst(const FVector& Origin, UStaticMeshComponent* HullMesh, float CountScale,
	                const FWreckDebrisBurstProfile& Profile, const FVector& BurstDirection = FVector::ZeroVector);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	int32 GetPieceCount(const UStaticMeshComponent& HullMesh, float CountScale) const;
	const FWreckDebrisPieceType* PickPieceType() const;
	UStaticMeshComponent* SpawnPiece(const FWreckDebrisPieceType& PieceType, UMaterialInterface* HullMaterial);
	void LaunchPieces() const;
	FVector GetLaunchSideDirection() const;
	void IgniteSomePieces();
	void StartSettleChecks();
	void CheckPiecesSettled();
	bool GetShouldSettle(FWreckDebrisFlyingPiece& Piece, float FlightTime) const;
	void SettlePiece(FWreckDebrisFlyingPiece& Piece) const;
	void StopFires();
	// Project-wide debris settings (Project Settings > Game > Wreck Debris); the default object always exists.
	static const UWreckDebrisSettings& GetSettings();

	UPROPERTY()
	TArray<FWreckDebrisFlyingPiece> M_Pieces;

	UPROPERTY()
	TWeakObjectPtr<URTSFireSubsystem> M_FireSubsystem;

	FWreckDebrisBurstProfile M_Profile;
	FVector M_Origin = FVector::ZeroVector;
	// Horizontal unit direction the biased pieces fly towards; zero when the burst has no direction.
	FVector M_BurstDirection = FVector::ZeroVector;
	FTimerHandle M_SettleCheckTimerHandle;
	float M_LaunchWorldTime = 0.f;
	TArray<int32> M_FireHandles;
};
