#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/TimeProgressBarWidget.h"
#include "ScavengableObject.generated.h"

enum class ETankSubtype : uint8;
struct FWeightedDecalMaterial;
class UGeometryCollection;
class UScavRewardComponent;
class ACPPController;
class ASquadController;
enum class ERTSResourceType : uint8;

USTRUCT()
struct FScavengeRewardSound
{
	GENERATED_BODY()

	UPROPERTY()
	USoundBase* BasicRewardSound = nullptr;
	UPROPERTY()
	USoundAttenuation* Attenuation = nullptr;
	UPROPERTY()
	USoundConcurrency* Concurrency = nullptr;
};

USTRUCT(BlueprintType)
struct FScavengeAmount
{
	GENERATED_BODY()

	// Whether the player always receives the same set amount or if we use the random amount defined by min and max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsSetAmount = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SetAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Min = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Max = 1;

	// What is the chance of getting this resource. (either as set or as random range amount)
	// in percentage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Chance = 50;
};

/**
 * @brief Placed in the world as a scavenging target and configured through blueprint setup.
 */
UCLASS()
class RTS_SURVIVAL_API AScavengeableObject : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	AScavengeableObject(const FObjectInitializer& ObjectInitializer);

	/** Starts the scavenge timer and progress bar */
	void StartScavengeTimer(TObjectPtr<ASquadController> ScavengingSquadController);

	/** Pauses the scavenging process */
	void PauseScavenging(const TObjectPtr<ASquadController>& RequestingSquadController);

	/** @return The time left for scavenging to complete from the timer. */
	float GetScavengeTimeLeft() const;

	/** @return Whether this scavenges object is enabled. */
	bool GetCanScavenge() const;

	FVector GetScavLocationClosestToPosition(const FVector& Position) const;

	inline float GetScavengeTime() const { return ScavengeTime; };

	void SetIsScavengeEnabled(bool bEnabled);

	/** Provides scavenge positions for units */
	TArray<FVector> GetScavengePositions(int32 NumUnits);

protected:
	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void BeginDestroy() override;
	// Gives BP the chance to set the scavenger component if needed.
	UFUNCTION(BlueprintImplementableEvent, Category="ScavengeMesh")
	void BP_PostInitSetScavengerComponent();

	// Sets the mesh component with the given name as the scavenger mesh component.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="ScavengeMesh")
	void SetStaticMeshCompOfNameAsScavenegeMesh(const FName MeshCompName);


	/** @note Overwrites the rewards this scav object provides by using the vehicle parts of the destroyed vehicle.
	 * @param TankSubtype The tank subtype to use for the scavenging rewards.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="DestroyedVehicle")
	void SetupAsDestroyedVehicle(const ETankSubtype TankSubtype);

	/** IF this scavenging object needs to set a mesh manually call this in the constructor.
	 * @note in post init components we try to find a component with the correct tag if the mesh comp was
	 * not already set so ensure this is called before that!
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="CONSTRUCTOR")
	void SetScavengeMesh(UStaticMeshComponent* NewMeshComp);

	// Called at bp to setup the widget from the widget component that is used to track the progress
	// of the scavenging
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="ReferenceCasts")
	void InitScavObject(UTimeProgressBarWidget* NewProgressBarWidget);

	// The resources in the scavengable object.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	TMap<ERTSResourceType, FScavengeAmount> ScavengeResources;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	TSoftClassPtr<UScavRewardComponent> RewardComponentWidgetClass;

	// Will be used instead of the scavenging reward sound set in game settings.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	USoundBase* RewardSoundOverride;

	// Can be set to false so no resource reward widget will be created.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	bool bGivePlayerResourceRewards = true;

	// If true, the scavenging squad is consumed when scavenging completes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scavenge")
	bool bConsumeSquadWhenFinished = false;

	// the total time it takes to scavenge the object.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scavenge")
	float ScavengeTime = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scavenge")
	FVector DestructionForceOffset = FVector(0, 0, 0);

	// How long the regular visible time is on the reward widget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RewardWidget")
	float VisibleTimeRewardWidget = 3;

	// How long the fade duraton on the reward widget lasts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RewardWidget")
	float FadeTimeRewardWidget = 3;

	virtual void OnUnitDies(const ERTSDeathType DeathType) override;

private:
	// Starts with true in c tor. Set to false once one squad unit reaches and ASquadController::StartScavengeObject
	// is called. Is set disabled on MakeScavengingSquadStopScavenging_AndDisableScavObj which happens on complete.
	bool bM_IsScavengeEnabled = true;

	void OnUnitDies_StopScavSquadIfValid();
	void OnUnitDies_StopTimersAndDisableScavObj();
	void MakeScavengingSquadStopScavenging_AndDisableScavObj();
	void HandleScavengeCompletionForSquad();
	bool GetIsValidScavengingSquad() const;

	// The socket names of the mesh comp to find the scavenge positions.
	UPROPERTY()
	TArray<FName> M_ScavengeSocketNames;

	void GenerateAdditionalPositions(TArray<FVector>& Positions, int32 NumRequired) const;

	// Scavenging squad controller
	TObjectPtr<ASquadController> M_ScavengingSquad = nullptr;

	// Scavenge timer handle
	FTimerHandle M_ScavengeTimerHandle;

	// Time when the scavenging started
	float M_ScavengeStartTime = 10;

	// Remaining scavenge time
	float M_ScavengeTimeLeft = 0;

	UPROPERTY()
	TObjectPtr<UTimeProgressBarWidget> M_ProgressBarWidget;
	bool EnsureIsValidProgressBarWidget() const;

	bool StartScavTimer_EnsureAliveScavObj(const TObjectPtr<ASquadController>& ScavengingSquad);
	bool EnsureRequestingSquadIsActiveScavenger(const TObjectPtr<ASquadController>& RequestingSquadController) const;

	// Called when scavenging is complete
	void OnScavengingComplete();
	void DisableProgressBarAndTimers();


	// ----------------------------
	// ---------- Reward Widget -----
	// ----------------------------

	// Reward the player with resources.
	void RandomRewardPlayer();

	// Asynchronously loads the reward widget class and creates the reward widget component.
	// If the widget is already loaded we create instantly.
	void AsyncLoadAndCreateRewardWidget();

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	bool GetIsValidPlayerController();

	void CreateRewardWidgetComponent();

	// Converts the saved rewards into a single string to display on the reward widget rich text.
	FString GetRewardText();

	void OnRewardWidgetClassLoaded();

	// Saves the text of the rewards as provided by the RandomPlayerReward function.
	UPROPERTY()
	TArray<FString> M_RewardTexts;

	// ----------------------------
	// ---------- END Reward Widget -----
	// ----------------------------

	// Contains the types and amounts of resources the player received through scavenging.
	UPROPERTY()
	TMap<ERTSResourceType, int32> M_PlayerResourceAmountRewards;

	// --------------------------
	// ------ Scavenge Mesh ----
	// --------------------------

	// Reference to the mesh of this object.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ScavengeMeshComp = nullptr;

	/**
     * @brief Find the mesh component marked with FRTSGamePlayTags::ScavengeMeshComponent.
     * Tries Gameplay Tags (IGameplayTagAssetInterface) first, then falls back to ComponentTags (FName).
     * @return UStaticMeshComponent* if found, otherwise nullptr.
     */
	UStaticMeshComponent* FindScavengeMeshComponent() const;

	/**
	  * @brief Extract socket names containing "scav" (case-insensitive) from a mesh component.
	  * 
	  * Finds all socket names on the mesh asset that contain the substring "scav"
	  * regardless of casing or position within the name.
	  * 
	  * @param InMeshComponent Mesh component whose sockets to inspect.
	  * @param OutSocketNames Populated with matching socket names.
	  */
	void ExtractScavengeSocketNames(
		const UStaticMeshComponent& InMeshComponent,
		TArray<FName>& OutSocketNames) const;

	const float RewardWidgetHeightOffset = 400.f;

	/**
     * @brief Validates M_MeshComp and logs once if invalid.
     * @param InContext Call-site (for precise diagnostics).
     * @return true if valid; false if null/invalid (and reports).
     */
	bool GetIsValidMeshComp(const FString& InContext) const;
	// --------------------------
	// ----END Scavenge Mesh ----
	// --------------------------
	void BeginPlay_LoadSoundSettings();
	void PlayRewardSound() const;
	FScavengeRewardSound M_ScavRewardSoundSettings;

	bool bM_DestroyedByScavenging = false;
};
