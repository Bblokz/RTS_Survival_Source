// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InfoTakeoverTypes.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "AsyncShowInfoTakeover.generated.h"

class UInfoTakeoverSubsystem;
enum class EInfoTakeoverRequestResult : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInfoTakeoverNodeClosed, bool, bWasShown);

/**
 * @brief Shared machinery of the mission nodes that show a field manual takeover and wait for it: queues the request
 * on UInfoTakeoverSubsystem and fires On Closed once, after the player closed it and the game is unpaused, or at once
 * when it is not shown. Each node is a subclass with its own factory UFUNCTION that only says which takeover to queue.
 */
UCLASS(Abstract)
class RTS_SURVIVAL_API UAsyncShowInfoTakeoverBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// Fires once: after the player closed the takeover and the game is unpaused, or at once when it is not shown.
	UPROPERTY(BlueprintAssignable)
	FOnInfoTakeoverNodeClosed OnClosed;

	virtual void Activate() override;

protected:
	/**
	 * @brief Creates a node of the subclass for a factory UFUNCTION and keeps it alive until it finished.
	 * @param WorldContextObject The mission (or any object in the game world).
	 * @return The node; its Activate queues the takeover.
	 */
	template <typename TNode>
	static TNode* CreateNode(UObject* WorldContextObject)
	{
		TNode* const Node = NewObject<TNode>();
		static_cast<UAsyncShowInfoTakeoverBase*>(Node)->M_WorldContextObject = WorldContextObject;
		// Without a world it stays unregistered and finishes in Activate.
		if (IsValid(WorldContextObject))
		{
			Node->RegisterWithGameInstance(WorldContextObject);
		}
		return Node;
	}

	/** @return The request id of the queued takeover; INDEX_NONE when it could not be queued. */
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const
		PURE_VIRTUAL(UAsyncShowInfoTakeoverBase::QueueTakeover, return INDEX_NONE;);

	// The node's display name, for error messages.
	virtual FString GetNodeName() const PURE_VIRTUAL(UAsyncShowInfoTakeoverBase::GetNodeName, return FString(););

private:
	UPROPERTY()
	TWeakObjectPtr<UObject> M_WorldContextObject;

	UPROPERTY()
	TWeakObjectPtr<UInfoTakeoverSubsystem> M_InfoTakeoverSubsystem;

	FDelegateHandle M_RequestFinishedHandle;
	int32 M_RequestId = INDEX_NONE;
	bool bM_HasFinished = false;

	void HandleRequestFinished(const int32 RequestId, const EInfoTakeoverRequestResult Result);
	void Finish(const bool bBroadcastClosed, const bool bWasShown);
	void UnbindFromSubsystem();
	UInfoTakeoverSubsystem* FindInfoTakeoverSubsystem() const;
};

/**
 * @brief Mission node that pauses the game with the camera controls field manual (two pages) and continues from On
 * Closed once the player pressed UNDERSTOOD on the last page, so announcer lines wait for it.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowCameraControlsTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the camera controls takeover (arrow keys, edge panning, zoom, marquee selection) and waits.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Camera Controls Takeover"))
	static UAsyncShowCameraControlsTakeover* ShowCameraControlsTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Camera Controls Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the cover field manual (what the green, yellow and red move preview
 * means) and continues from On Closed once the player pressed UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowCoverTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the cover takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Cover Takeover"))
	static UAsyncShowCoverTakeover* ShowCoverTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Cover Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the spaced armour field manual (side plates add armour and come off
 * under fire, even from shots that bounce off) and continues from On Closed once the player pressed UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowSpacedArmourTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the spaced armour takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Spaced Armour Takeover"))
	static UAsyncShowSpacedArmourTakeover* ShowSpacedArmourTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Spaced Armour Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the nomadic conversion field manual (select a truck, convert it into a
 * building, place it, pack it up again) and continues from On Closed once the player pressed UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowNomadicConversionTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the nomadic conversion takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Nomadic Conversion Takeover"))
	static UAsyncShowNomadicConversionTakeover* ShowNomadicConversionTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Nomadic Conversion Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the building expansion pack-up field manual (expansions pack up with
 * their nomadic building, attached ones come back by themselves after it deploys, free-standing ones are placed again
 * from their slot) and continues from On Closed once the player pressed UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowBxpPackUpTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the building expansion pack-up takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Building Expansion Pack Up Takeover"))
	static UAsyncShowBxpPackUpTakeover* ShowBxpPackUpTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Building Expansion Pack Up Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the Vultures AT stick grenade field manual (a short clip of the squad
 * sticking its grenades onto a T-26, and how the volley works) and continues from On Closed once the player pressed
 * UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowVulturesAtTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Vultures AT stick grenade takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Vultures AT Grenade Takeover"))
	static UAsyncShowVulturesAtTakeover* ShowVulturesAtTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Vultures AT Grenade Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the Rattle-Feed field manual (a short clip of a Panzer II switching
 * its 20 mm gun to Rattle-Feed on a T-26, and how the burst works) and continues from On Closed once the player pressed
 * UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowRattleFeedTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Rattle-Feed takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Rattle-Feed Takeover"))
	static UAsyncShowRattleFeedTakeover* ShowRattleFeedTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Rattle-Feed Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the Wespe Barrage field manual (a Wespe battery
 * shelling a T-26 and a rifle squad, and how the area barrage works) and continues from On Closed once the player pressed
 * UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowWespeBarrageTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Wespe Barrage takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Wespe Barrage Takeover"))
	static UAsyncShowWespeBarrageTakeover* ShowWespeBarrageTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Wespe Barrage Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the Direction Finding field manual (Oder-Warthe: a minimap clip of a
 * listening post's bearing wedge narrowing and crossing a second one) and continues from On Closed once the player
 * pressed UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowDirectionFindingTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Direction Finding takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Direction Finding Takeover"))
	static UAsyncShowDirectionFindingTakeover* ShowDirectionFindingTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Direction Finding Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the Dig In field manual (a Panzer IV digging in
 * behind sandbags while two T-26s fire at it, and how the wall works) and continues from On Closed once the player pressed
 * UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowDigInTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Dig In takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Dig In Takeover"))
	static UAsyncShowDigInTakeover* ShowDigInTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Dig In Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the Wall Breach field manual (a Tiger driving through a Soviet concrete
 * wall, and which tanks can do it) and continues from On Closed once the player pressed UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowWallBreachTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Wall Breach takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Wall Breach Takeover"))
	static UAsyncShowWallBreachTakeover* ShowWallBreachTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Wall Breach Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the Adrenaline Shot field manual (a Panzer 38(t)
 * firing at a T-26 at its normal pace and then with Adrenaline Shot, and how the rush works) and continues from On Closed once the player pressed
 * UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowAdrenalineShotTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Adrenaline Shot takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Adrenaline Shot Takeover"))
	static UAsyncShowAdrenalineShotTakeover* ShowAdrenalineShotTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Adrenaline Shot Takeover"); }
};

/**
 * @brief Mission node that pauses the game with the capturing trucks field manual (scuttle countdown, a squad within
 * 20 m, the 60 % health rule, capture versus destroy) and continues from On Closed once the player pressed UNDERSTOOD.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowCapturingTrucksTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the capturing trucks takeover and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; field manuals ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Capturing Trucks Takeover"))
	static UAsyncShowCapturingTrucksTakeover* ShowCapturingTrucksTakeover(UObject* WorldContextObject);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Capturing Trucks Takeover"); }
};

/**
 * @brief Mission node that pauses the game with a captured paper document whose text is written on the node (split
 * the Document pin), and continues from On Closed once the player pressed UNDERSTOOD. Archived under Lore.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowDocumentTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows a document takeover with the given text and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @param Document Title, typed intro, the sheet's heading, lines (marked ones in red), footer, stamp, archive key.
	 * @return The node; On Closed fires once the player closed it; documents ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Document Takeover"))
	static UAsyncShowDocumentTakeover* ShowDocumentTakeover(UObject* WorldContextObject,
	                                                        const FDocumentTakeoverContent& Document);

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Document Takeover"); }

private:
	UPROPERTY()
	FDocumentTakeoverContent M_Document;
};

/**
 * @brief Mission node for the Ore Mountains radio outpost: pauses the game with the captured convoy manifest (the
 * columns of the week, one armoured truck of sealed cases stamped for Berlin) and continues from On Closed once the
 * player pressed UNDERSTOOD. The text is fixed in C++ (MakeOreManifest) so it obeys the mission's lore rules.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncShowOreManifestTakeover : public UAsyncShowInfoTakeoverBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the Ore Mountains convoy manifest and waits for it to close.
	 * @param WorldContextObject The mission (or any object in the game world); filled in as self.
	 * @return The node; On Closed fires once the player closed it; documents ignore "never show again".
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Info Takeover",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject",
			HidePin = "WorldContextObject", DisplayName = "Show Ore Manifest Takeover"))
	static UAsyncShowOreManifestTakeover* ShowOreManifestTakeover(UObject* WorldContextObject);

	/**
	 * @return The manifest's text. Lore rules (OreMountainsMission.md): never say why Berlin, no cryo, nothing about
	 * where Radixite comes from.
	 */
	static FDocumentTakeoverContent MakeOreManifest();

protected:
	virtual int32 QueueTakeover(UInfoTakeoverSubsystem& InfoTakeoverSubsystem) const override;
	virtual FString GetNodeName() const override { return TEXT("Show Ore Manifest Takeover"); }
};
