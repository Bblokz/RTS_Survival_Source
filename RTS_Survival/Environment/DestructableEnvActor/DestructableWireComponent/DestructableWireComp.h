// DestructableWireComp.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DestructableWireComp.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;

/** Per-connection bookkeeping so we can tear down exactly the right wires on either side. */
USTRUCT()
struct FWireLink
{
	GENERATED_BODY()

	/** The opposite wire component we are connected to. */
	UPROPERTY()
	TWeakObjectPtr<class UDestructibleWire> Other;

	/** Sockets used on this component for this link (pair-unique). */
	UPROPERTY()
	TArray<FName> ThisSockets;

	/** Sockets used on the other component for this link (pair-unique). */
	UPROPERTY()
	TArray<FName> OtherSockets;

	/** Splines spawned by this component for this link (may be empty if the other side spawned). */
	UPROPERTY()
	TArray<USplineComponent*> Splines;

	/** Spline-meshes spawned by this component for this link (may be empty if the other side spawned). */
	UPROPERTY()
	TArray<USplineMeshComponent*> SplineMeshes;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class UDestructibleWire : public UActorComponent
{
	GENERATED_BODY()

public:
	UDestructibleWire(const FObjectInitializer& ObjectInitializer);

	/** Initialize this wire component with its source mesh and droop depths. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitWireComp(UStaticMeshComponent* MyWireMesh, const TArray<float>& DroopPerCurve);

	/** Call when the owning actor dies to clear everything on both ends. */
	void OnOwningActorDies();

	/** Attempt to connect up to MaxAmountWires between this and ChildWireComp. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupWireConnection(
		UDestructibleWire* ChildWireComp,
		int32 MaxAmountWires,
		UStaticMesh* WireMesh,
		const FVector& WireMeshScale
	);

	/** Returns physical socket count on this component (used to clamp per-link request). */
	UFUNCTION(BlueprintCallable, Category="Wires")
	int32 GetAmountWiresSocketsLeft() const;

	/** Check that InitWireComp was called successfully. */
	bool EnsureWireMeshIsValid() const;

protected:
	/** Remove all wires from this component and notify others. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Wires")
	void RemoveAllWires();

	/** (Legacy) Build splines + meshes. Kept for BP compatibility. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Wires")
	void CreateSplineWires(
		UStaticMeshComponent* CompA,
		UStaticMeshComponent* CompB,
		int32 AmountOfWires,
		UStaticMesh* WireMesh,
		const TArray<float>& CurveDepths,
		const FVector& WireMeshScale
	);

	// Lifecycle hooks to guarantee teardown.
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleOwnerDestroyed(AActor* DestroyedActor);

private:
	// === State ===

	/** The static mesh component we draw wires *from*. */
	UPROPERTY()
	UStaticMeshComponent* M_WireMeshConnectFrom = nullptr;

	/** The per-wire curve-depth settings. */
	UPROPERTY()
	TArray<float> M_CurveDepths;

	/** All spline components we spawned (aggregate). */
	UPROPERTY()
	TArray<USplineComponent*> M_SplineComponents;

	/** All spline-mesh components we spawned (aggregate). */
	UPROPERTY()
	TArray<USplineMeshComponent*> M_SplineMeshComponents;

	/** All active links (pair-scoped). */
	UPROPERTY()
	TArray<FWireLink> M_Links;

	/** Guard to avoid double teardown. */
	bool M_bTeardownDone = false;

	// === Connection & teardown ===

	/** Register the backlink on the other side (sockets only if that side didn't spawn meshes). */
	void RegisterBackLinkFromOther(
		UDestructibleWire* Other,
		const TArray<FName>& MySockets,
		const TArray<FName>& OtherSockets
	);

	/** Remove the connection and any meshes we created for it. */
	void RemoveConnectionTo(UDestructibleWire* Other);

	/** Notify all counterparts we are going away, then clear local wires and links. */
	void NotifyAndRemoveAllLinks();

	/** Local clear of meshes/splines. */
	void Wire_ClearAllWires();

	// === Build & selection helpers ===

	/** Collect sockets already used in existing links with a specific counterpart. */
	void Wire_CollectSocketsUsedWithCounterpart(
		const UDestructibleWire* Other,
		TSet<FName>& OutThisUsed,
		TSet<FName>& OutOtherUsed
	) const;

	/** From all sockets on both ends, compute the per-pair available sockets (excludes pair-used only). */
	void Wire_GetAvailableSocketsForPair(
		UStaticMeshComponent* CompA,
		UStaticMeshComponent* CompB,
		const UDestructibleWire* Other,
		TArray<FName>& OutAvailA,
		TArray<FName>& OutAvailB
	) const;

	/** Internal builder that returns exactly what was spawned and which sockets were used (pair-aware). */
	void Wire_CreateSplineWires_Internal(
		UStaticMeshComponent* CompA,
		UStaticMeshComponent* CompB,
		int32 AmountOfWires,
		UStaticMesh* WireMesh,
		const TArray<float>& CurveDepths,
		const FVector& WireMeshScale,
		const UDestructibleWire* OtherForPair,
		TArray<FName>& OutSocketsA,
		TArray<FName>& OutSocketsB,
		TArray<USplineComponent*>& OutSplines,
		TArray<USplineMeshComponent*>& OutSplineMeshes
	);

	// === Logging & statics ===

	void Wire_Debug(const FString& Message) const;
	void Wire_Error(const FString& Message) const;

	static void Wire_GetEnSockets(UStaticMeshComponent* Comp, TArray<FName>& OutSocketNames);
	static int32 Wire_DetermineWireCount(int32 Requested, int32 CountA, int32 CountB);
	static void Wire_CreateCurvePoints(
		const FVector& Start,
		const FVector& End,
		float Depth,
		TArray<FVector>& OutPoints
	);
};
