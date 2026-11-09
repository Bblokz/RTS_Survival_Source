// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AircraftWreckActor.generated.h"

class UFXSystemComponent;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UNiagaraSystem;

/** How far a carried piece is on its way off the wreck. */
enum class EAircraftWreckPieceState : uint8
{
	// Kinematic, part of the wreck.
	Held,
	// Let go of, waiting until the collection's physics exists.
	ReleaseRequested,
	// Torn out of its cluster by strain; still kinematic, so it keeps moving with the wreck for a frame or two.
	TornOut,
	// Dynamic: falls on with the speed the wreck gave it.
	Loose
};

/** One piece of the crash collection: where it sits on the intact aircraft and whether it was let go. */
struct FAircraftWreckPiece
{
	int32 TransformIndex = INDEX_NONE;
	// Centre and half size of the piece's geometry in the aircraft's (component) space.
	FVector LocalCentre = FVector::ZeroVector;
	FVector LocalExtent = FVector::ZeroVector;
	// Let go of (it may still be on its way off the wreck, see State).
	bool bReleased = false;
	EAircraftWreckPieceState State = EAircraftWreckPieceState::Held;
	uint64 TornOutFrame = 0;
	// Frame of the last release field; the physics state is checked a few frames later and the field retried.
	uint64 ReleaseFieldFrame = 0;
	int32 ReleaseFieldAttempts = 0;
	bool bIsConfirmedDynamic = false;
	// World time it was let go; tells a part torn off in the air from one released on the ground.
	float ReleasedAtSeconds = -1.f;
	FVector PendingVelocity = FVector::ZeroVector;
	FVector PendingSpin = FVector::ZeroVector;
};

/**
 * @brief The pieces a shot-down aircraft breaks into. Spawned by UAircraftDeathComponent in one of two ways:
 * - Carried: the collection replaces the falling aircraft's meshes while it falls; every piece is kinematic and moves
 *   with the wreck until it is let go (ReleasePiece), so parts can tear off in the air and the hull can come apart
 *   from the nose back when it hits the ground.
 * - BreakUp: everything comes apart at once (an aircraft hit while parked).
 * The pieces then rest on the ground and clear themselves away.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AAircraftWreckActor : public AActor
{
	GENERATED_BODY()

public:
	AAircraftWreckActor();

	/**
	 * @brief Turns the collection into loose pieces that keep part of the aircraft's speed and are blown apart.
	 * @param CrashCollection Pieces of this aircraft, built by RTS.Aircraft.BuildCrashCollections.
	 * @param ImpactVelocity Velocity of the aircraft when it hit.
	 * @param VelocityRetained Share of that velocity the pieces keep.
	 * @param ScatterSpeed Outward speed the explosion gives the pieces.
	 * @param LifeSeconds How long the pieces stay.
	 * @return Number of pieces, or 0 when the collection could not be used.
	 */
	int32 BreakUp(UGeometryCollection* CrashCollection, const FVector& ImpactVelocity, float VelocityRetained,
	              float ScatterSpeed, float LifeSeconds);

	/**
	 * @brief Starts the collection whole and kinematic where the actor stands; move the actor to move the wreck.
	 * @param CrashCollection Pieces of this aircraft.
	 * @param LifeSeconds How long the wreck stays; set again with SetLifeSpan when it lands.
	 * @param PieceLinearDamping Air drag on pieces once they are let go.
	 * @return Number of pieces, or 0 when the collection could not be used.
	 */
	int32 StartCarried(UGeometryCollection* CrashCollection, float LifeSeconds, float PieceLinearDamping);

	/**
	 * @brief Lets one piece go: it falls on with the speed it moved with, plus the given kick.
	 * @param PieceIndex Index into GetPieces.
	 * @param AddedVelocity Added to the speed it already has.
	 * @param AngularVelocity Spin added, radians per second.
	 */
	void ReleasePiece(int32 PieceIndex, const FVector& AddedVelocity, const FVector& AngularVelocity);

	/** @return Whether the collection is carried and some piece is still held. */
	bool GetHasHeldPieces() const;

	/**
	 * @brief Moves the carried wreck to the aircraft. Ignored until the collection's physics exists: it is created
	 * some steps after the collection registers, at the transform of that moment, and a wreck moved before then
	 * would carry its pieces that far behind for good.
	 */
	void FollowTransform(const FTransform& AircraftTransform);

	/** @return Whether the carried wreck is drawn (its physics exists and it sits on the aircraft). */
	bool GetIsCarriedWreckShown() const { return bM_IsShown; }

	/** @return Where the centre of a piece is now, held or loose. */
	FVector GetPieceWorldCentre(int32 PieceIndex) const;

	/** @return Where the physics results put a piece, whether or not it is held (they trail a held piece by a step). */
	FVector GetPiecePhysicsCentre(int32 PieceIndex) const;

	const TArray<FAircraftWreckPiece>& GetPieces() const { return M_Pieces; }

	/** @return Pieces the wreck broke into; 0 before BreakUp or without a collection. */
	int32 GetPieceCount() const { return M_PieceCount; }

	/** @return Pieces let go so far in the carried mode. */
	int32 GetReleasedPieceCount() const { return M_ReleasedPieceCount; }

	virtual void Tick(float DeltaSeconds) override;

	/** @brief A fire that belongs to the wreck: it dies down with the others (StopFiresAfter). */
	void AddWreckFire(UFXSystemComponent* Fire);

	/** @brief The wreck's fires stop producing flames after this long; the last ones burn out. */
	void StopFiresAfter(float Seconds);

	UGeometryCollectionComponent* GetGeometryCollectionComponent() const { return M_GeometryCollection; }

private:
	UPROPERTY()
	TObjectPtr<UGeometryCollectionComponent> M_GeometryCollection;

	UPROPERTY()
	TArray<TWeakObjectPtr<UFXSystemComponent>> M_WreckFires;

	TArray<FAircraftWreckPiece> M_Pieces;
	int32 M_PieceCount = 0;
	int32 M_ReleasedPieceCount = 0;
	bool bM_IsCarried = false;
	// The collection's physics is created some steps after it registers (half a second in tests): pieces are
	// anchored, moved and torn off only after that.
	float M_CarriedSinceSeconds = 0.f;
	bool bM_IsPhysicsReady = false;
	uint64 M_PhysicsReadyFrame = 0;
	// Hidden until it has moved onto the aircraft after its physics came up.
	bool bM_IsShown = false;
	// After the first tear every piece is a kinematic body of its own and can be made dynamic at once.
	bool bM_AreClustersBroken = false;

	/** @return Whether the collection's physics exists now; it reports dynamic once its proxy runs. */
	bool Carried_CheckPhysicsReady();

	/** @brief Moves released pieces on: torn out of their cluster first, made dynamic a couple of frames later. */
	void Carried_AdvanceReleases();
	/** @brief A dynamic-state field that turns the pieces inside the sphere dynamic and leaves all others alone. */
	void Carried_MakeDynamicInSphere(const FVector& Centre, float Radius) const;
	/** @brief Turns a torn-out piece dynamic with a dynamic-state field around it; wider on each retry. */
	void Carried_ApplyReleaseField(int32 PieceIndex);
	/** @return Whether the physics reports the piece dynamic now (not asleep in mid-air), so the field caught it. */
	bool Carried_GetIsPieceDynamicInPhysics(const FAircraftWreckPiece& Piece) const;

	bool GetIsValidGeometryCollection() const;
	void SetupPieceCollision() const;
	/** @brief Creates and registers the collection component with the collision every wreck uses. */
	bool CreateCollectionComponent(UGeometryCollection* CrashCollection, bool bKinematic, float PieceLinearDamping);
	void GatherPieces(const UGeometryCollection& CrashCollection);
	void StopFires();
};
