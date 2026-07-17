// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralDestructibleSplineSpawner.generated.h"

class ADestructibleSplineActor;
class UMeshComponent;
class UProceduralDestructibleSplineManager;

/**
 * @brief Designer defaults used by a procedural destructible spline endpoint.
 * The authored piece dimensions let the component choose a safe number of spline pieces without
 * exposing curve-construction details.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FProceduralDestructibleSplineSpawnerSettings
{
	GENERATED_BODY()

	/** Only mesh sockets whose names contain this text are eligible. Empty text accepts every socket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sockets")
	FString SocketFilter;

	/** Destructible spline Blueprint spawned for connections created by this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline")
	TSubclassOf<ADestructibleSplineActor> DestructibleSplineClass;

	/** Authored +X length and Y width of one spline piece, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline",
		meta=(ClampMin="1.0", Units="cm", DisplayName="Spline Piece X/Y Size"))
	FVector2D SplinePieceSize = FVector2D(686.f, 60.f);

	/** How strongly the path follows the endpoint socket rotations. Zero creates a straight path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Curviness = 0.35f;

	/** Maximum world-space distance at which this component may create a connection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections", meta=(ClampMin="0.0", Units="cm"))
	float Range = 5000.f;

	/** Higher-priority components receive the first opportunity to create connections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections")
	int32 Priority = 0;

	/** Maximum number of splines this component creates; incoming splines do not count against it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections", meta=(ClampMin="0"))
	int32 MaxConnectionsOfOwnSpline = 1;

	/** Refuses a connection when its departure yaw differs from the source socket by more than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections",
		meta=(ClampMin="0.0", ClampMax="360.0", Units="deg"))
	float MaxDegreesRefuseConnection = 180.f;
};

/**
 * @brief Runtime endpoint that finds filtered mesh sockets and lets the level's procedural spline
 * manager connect them to compatible endpoints.
 * @note InitProceduralDestructibleSplineSpawner: call in Blueprint to provide the socket mesh.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UProceduralDestructibleSplineSpawner : public UActorComponent
{
	GENERATED_BODY()

public:
	UProceduralDestructibleSplineSpawner();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural Destructible Spline")
	FProceduralDestructibleSplineSpawnerSettings Settings;

	/** Supplies the mesh whose filtered sockets participate in procedural spline connections. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Procedural Destructible Spline")
	void InitProceduralDestructibleSplineSpawner(UMeshComponent* SocketMesh);

	/** Immediately collapses every connection when a custom owner death flow precedes actor destruction. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Procedural Destructible Spline")
	void OnOwningActorDies();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UProceduralDestructibleSplineManager;

	UFUNCTION()
	void HandleOwningActorDestroyed(AActor* DestroyedActor);

	void CacheEligibleSockets();
	void RegisterWithManager();
	void UnregisterFromManager(bool bCollapseConnections);

	bool GetIsValidSocketMesh() const;
	bool GetIsValidManager() const;
	bool GetIsInitialized() const;
	bool GetIsSocketAvailable(FName SocketName) const;
	bool ReserveSocket(FName SocketName);
	void ReleaseSocket(FName SocketName);
	bool TryGetSocketTransform(FName SocketName, FTransform& OutSocketTransform) const;
	int32 GetMaximumOwnConnections() const;
	float GetConnectionRange() const;
	float GetMaximumDepartureYaw() const;

	/**
	 * @brief Creates the connection owned by this endpoint after the manager atomically reserves both sockets.
	 * @param OwnSocket Reserved socket on this endpoint.
	 * @param OtherSpawner Endpoint containing the reserved destination socket.
	 * @param OtherSocket Reserved destination socket.
	 * @return The configured destructible spline, or null when spawning/configuration failed.
	 */
	ADestructibleSplineActor* SpawnConnectionSpline(
		FName OwnSocket,
		const UProceduralDestructibleSplineSpawner& OtherSpawner,
		FName OtherSocket) const;

	/**
	 * @brief Restores generated points after Blueprint construction and verifies the class can build pieces.
	 * @param SplineActor Deferred actor ready to finish spawning.
	 * @param StartTransform World transform of this component's reserved socket.
	 * @param EndTransform World transform of the destination's reserved socket.
	 * @return True when the finished actor contains at least one destructible piece.
	 */
	bool FinishConnectionSplineSpawn(
		ADestructibleSplineActor* SplineActor,
		const FTransform& StartTransform,
		const FTransform& EndTransform) const;

	/**
	 * @brief Replaces class-authored spline points with a deterministic endpoint-guided connection.
	 * @param SplineActor Spawned actor being configured.
	 * @param StartTransform World transform of this component's reserved socket.
	 * @param EndTransform World transform of the destination's reserved socket.
	 * @return True when at least one spline piece was configured.
	 */
	bool ConfigureConnectionSpline(
		ADestructibleSplineActor& SplineActor,
		const FTransform& StartTransform,
		const FTransform& EndTransform) const;

	/**
	 * @brief Samples one endpoint-guided curve into segments no longer than the authored piece length.
	 * @param StartTransform World transform of the source socket.
	 * @param EndTransform World transform of the destination socket.
	 * @param OutWorldPoints Generated spline points, including both endpoints.
	 * @param OutWorldTangents Tangents scaled for the generated per-piece spline intervals.
	 */
	void BuildConnectionCurve(
		const FTransform& StartTransform,
		const FTransform& EndTransform,
		TArray<FVector>& OutWorldPoints,
		TArray<FVector>& OutWorldTangents) const;

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> M_SocketMesh;

	UPROPERTY()
	TWeakObjectPtr<UProceduralDestructibleSplineManager> M_Manager;

	UPROPERTY()
	TArray<FName> M_EligibleSocketNames;

	UPROPERTY()
	TSet<FName> M_UsedSocketNames;

	int32 M_NumOwnConnections = 0;
	bool bM_IsInitialized = false;
	bool bM_IsRegistered = false;
	bool bM_IsShuttingDown = false;
};
