#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "WreckDebrisBurstComponent.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;
class URTSFireSubsystem;
class UWreckDebrisSettings;
struct FWreckDebrisPieceType;

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
 * @brief Added by UWreckTurretTossComponent to the wreck when ammunition detonates: throws metal chunks, shells,
 * ammo boxes and cans (UWreckDebrisSettings) out of the hull with physics, sets a few on fire, then freezes each
 * piece with collision off once it lies still, so the landed debris is only decoration around the wreck.
 */
UCLASS(ClassGroup=(RTS))
class RTS_SURVIVAL_API UWreckDebrisBurstComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWreckDebrisBurstComponent();

	/**
	 * @brief Throws the debris; the count grows with the hull's size so a Tiger sheds more than a Panzer II.
	 * @param Origin World location the pieces burst from (the turret ring).
	 * @param HullMesh Wreck hull: its size sets the count and its material paints the armour chunks.
	 * @param CountScale Per-tank multiplier on the piece count.
	 */
	void StartBurst(const FVector& Origin, UStaticMeshComponent* HullMesh, float CountScale);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	int32 GetPieceCount(const UStaticMeshComponent& HullMesh, float CountScale) const;
	const FWreckDebrisPieceType* PickPieceType() const;
	UStaticMeshComponent* SpawnPiece(const FWreckDebrisPieceType& PieceType, UMaterialInterface* HullMaterial);
	void LaunchPieces() const;
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

	FVector M_Origin = FVector::ZeroVector;
	FTimerHandle M_SettleCheckTimerHandle;
	float M_LaunchWorldTime = 0.f;
	TArray<int32> M_FireHandles;
};
