// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/FieldTrench/FieldTrenchLayout.h"

#include "FieldTrenchAnchorComponent.generated.h"

class ARTSFieldTrench;
class UFieldTrenchAnchorComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** The visuals of one trench anchor: what closes the gap to a joined trench and what fills it while none is. */
USTRUCT(BlueprintType)
struct FFieldTrenchAnchorDefinition
{
	GENERATED_BODY()

	// A TrenchPort_ socket on one of the owner's static meshes.
	UPROPERTY(EditDefaultsOnly, Category = "Trench Anchor")
	FName SocketName;

	// Shown on the socket while a trench is joined: the stub from the owner's wall to the trench end.
	UPROPERTY(EditDefaultsOnly, Category = "Trench Anchor")
	TSoftObjectPtr<UStaticMesh> NeckMesh;

	// Shown on the socket while no trench is joined, for anchors cut into a wall that was closed; may be empty.
	UPROPERTY(EditDefaultsOnly, Category = "Trench Anchor")
	TSoftObjectPtr<UStaticMesh> PlugMesh;
};

/** @brief Names one anchor of one actor; what a trench keeps of the anchor it starts at. */
struct RTS_SURVIVAL_API FFieldTrenchAnchorHandle
{
	TWeakObjectPtr<UFieldTrenchAnchorComponent> Component;
	int32 AnchorIndex = INDEX_NONE;

	UFieldTrenchAnchorComponent* Get() const;
	bool IsSet() const { return AnchorIndex != INDEX_NONE && Component.IsValid(); }
	void Reset() { *this = FFieldTrenchAnchorHandle(); }
	bool operator==(const FFieldTrenchAnchorHandle& Other) const
	{
		return Component == Other.Component && AnchorIndex == Other.AnchorIndex;
	}
};

/** @brief A TrenchPort_ socket read off an actor's meshes, in the trench planner's terms. */
struct FFieldTrenchAnchorSocketPort
{
	UStaticMeshComponent* MeshComponent = nullptr;
	FName SocketName;
	// +X of the socket, pointing away from the owner.
	FFieldTrenchOpenPort Port;
	float Height = 0.0f;
};

USTRUCT()
struct FFieldTrenchAnchorRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	FName SocketName;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SocketMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> NeckComponent;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlugComponent;

	// The trench that starts here, from when it is confirmed; the anchor is not offered to others meanwhile.
	UPROPERTY()
	TWeakObjectPtr<ARTSFieldTrench> Occupant;

	// The socket in world space, in the trench planner's terms: its +X points away from the owner.
	FFieldTrenchOpenPort Port;
	float Height = 0.0f;
	// The trench piece on the anchor is dug: the neck shows instead of the plug.
	bool bIsJoined = false;
};

/**
 * @brief Lets field trenches join an actor at the TrenchPort_ sockets of its static meshes. While the anchors
 * are available they are listed with UFieldTrenchAnchorSubsystem, where the trench placement looks for one
 * near the cursor; a trench that starts at an anchor claims it, and once the trench's first piece is dug the
 * anchor swaps its plug for its neck.
 * Every TrenchPort_ socket is an anchor; Anchors only adds the neck and plug meshes of a socket.
 * On a building expansion the anchors are available while it is built (the construction mesh has no
 * sockets), or from the start when it was placed in the level; on any other actor from BeginPlay on. When the owner dies or packs up, joined trenches cap their
 * end where the anchor was.
 * @note Socket contract: Source/Docs/Units/GerPitTrenchAnchors.md.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UFieldTrenchAnchorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFieldTrenchAnchorComponent();

	/** Sockets whose names start with this are trench anchors. */
	static const TCHAR* const SocketPrefix;

	/**
	 * @brief Reads the anchor sockets of an actor where they are now; works in the editor too, where the anchors
	 * are not made.
	 */
	static void GetAnchorSocketPorts(const AActor& Owner, TArray<FFieldTrenchAnchorSocketPort>& OutSocketPorts);

	/**
	 * @brief Makes the anchors available to trenches, or takes them away and caps the trenches joined to them.
	 * Building expansions are handled by the component itself; other owners call this when their sockets come
	 * and go.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trench Anchor")
	void SetAnchorsAvailable(const bool bAvailable);

	bool GetAreAnchorsAvailable() const { return bM_AreAnchorsAvailable; }
	int32 GetAnchorCount() const { return M_Anchors.Num(); }
	bool GetIsAnchorFree(const int32 AnchorIndex) const;
	const FFieldTrenchOpenPort& GetAnchorPort(const int32 AnchorIndex) const;
	FVector GetAnchorLocation(const int32 AnchorIndex) const;
	FName GetAnchorSocketName(const int32 AnchorIndex) const;
	bool GetIsAnchorJoined(const int32 AnchorIndex) const;
	// The player whose trenches may join; read from the owner's RTS component when the anchors become available.
	int32 GetOwningPlayer() const { return M_OwningPlayer; }

	/** @return False when the anchor is taken by another trench or no longer available. */
	bool TryClaimAnchor(const int32 AnchorIndex, ARTSFieldTrench& Trench);

	/** @brief The trench no longer starts here; the plug closes the gap again. */
	void ReleaseAnchor(const int32 AnchorIndex, const ARTSFieldTrench& Trench);

	/** @brief Shows the neck instead of the plug once the trench piece on the anchor is dug, or the reverse. */
	void SetAnchorJoined(const int32 AnchorIndex, const bool bIsJoined);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Neck and plug meshes per socket; sockets that are not listed are anchors without them.
	UPROPERTY(EditDefaultsOnly, Category = "Trench Anchor")
	TArray<FFieldTrenchAnchorDefinition> Anchors;

private:
	UPROPERTY()
	TArray<FFieldTrenchAnchorRuntime> M_Anchors;

	int32 M_OwningPlayer = INDEX_NONE;
	bool bM_AreAnchorsAvailable = false;

	void BindToBuildingExpansion();
	void UnbindFromBuildingExpansion();
	void CheckIfPlacedInLevel();
	void OnOwnerDies();

	/** @brief Finds the sockets on the owner's meshes, makes their necks and plugs and lists them as anchors. */
	void CreateAnchors();
	/**
	 * @param bCapTrenches The owner is gone for good: trenches close their end with a cap. Off while the world
	 * is torn down.
	 */
	void RemoveAnchors(const bool bCapTrenches, const bool bKeepVisuals);
	UStaticMeshComponent* CreateSocketVisual(UStaticMeshComponent& SocketMesh, const FName SocketName,
	                                         const TSoftObjectPtr<UStaticMesh>& Mesh) const;
	void ShowAnchorState(FFieldTrenchAnchorRuntime& Anchor) const;
	const FFieldTrenchAnchorDefinition* FindDefinition(const FName SocketName) const;
};
