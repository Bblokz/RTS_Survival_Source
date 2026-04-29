#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "ScriptedTriggerActor.generated.h"

class ATriggerArea;
class USoundBase;

enum class EScriptedTriggerAreaShape : uint8
{
	Sphere,
	Rectangle
};

USTRUCT()
struct FScriptedTriggerAreaRegistration
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ATriggerArea> M_TriggerArea;

	int32 M_TriggerId = INDEX_NONE;
	float M_DelayBetweenCallbacks = 0.0f;
	float M_LastCallbackTime = -1.0f;
	int32 M_MaxCallbacks = -1;
	int32 M_CallbackCount = 0;
	bool bM_PendingRemoval = false;
};

/**
 * @brief Place or subclass this actor when you want self-contained trigger-driven Blueprint logic outside the mission system.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AScriptedTriggerActor : public AActor
{
	GENERATED_BODY()

public:
	AScriptedTriggerActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief Spawns a sphere trigger area owned by this actor so Blueprint logic can react without mission dependencies.
	 * @param Location World location for the spawned trigger actor.
	 * @param Rotation World rotation for the spawned trigger actor.
	 * @param Scale Shape scale applied to the trigger collision component.
	 * @param TriggerOverlapLogic Overlap channel filter for player/enemy trigger behavior.
	 * @param DelayBetweenCallbacks Minimum time between callback executions for this trigger.
	 * @param MaxCallbacks Maximum callbacks before destroy, negative for infinite callbacks.
	 * @param TriggerId Actor-defined callback identifier.
	 * @return Spawned trigger actor or nullptr when creation failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptedTrigger|TriggerArea")
	ATriggerArea* CreateTriggerAreaSphere(const FVector& Location,
	                                      const FRotator& Rotation,
	                                      const FVector& Scale,
	                                      const ETriggerOverlapLogic TriggerOverlapLogic,
	                                      const float DelayBetweenCallbacks,
	                                      const int32 MaxCallbacks,
	                                      const int32 TriggerId);

	/**
	 * @brief Spawns a rectangle trigger area owned by this actor so Blueprint logic can react without mission dependencies.
	 * @param Location World location for the spawned trigger actor.
	 * @param Rotation World rotation for the spawned trigger actor.
	 * @param Scale Shape scale applied to the trigger collision component.
	 * @param TriggerOverlapLogic Overlap channel filter for player/enemy trigger behavior.
	 * @param DelayBetweenCallbacks Minimum time between callback executions for this trigger.
	 * @param MaxCallbacks Maximum callbacks before destroy, negative for infinite callbacks.
	 * @param TriggerId Actor-defined callback identifier.
	 * @return Spawned trigger actor or nullptr when creation failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptedTrigger|TriggerArea")
	ATriggerArea* CreateTriggerAreaRectangle(const FVector& Location,
	                                         const FRotator& Rotation,
	                                         const FVector& Scale,
	                                         const ETriggerOverlapLogic TriggerOverlapLogic,
	                                         const float DelayBetweenCallbacks,
	                                         const int32 MaxCallbacks,
	                                         const int32 TriggerId);

	UFUNCTION(BlueprintCallable, Category = "ScriptedTrigger|TriggerArea")
	void RemoveTriggerAreasById(const int32 TriggerId);

	UFUNCTION(BlueprintCallable, Category = "ScriptedTrigger|TriggerArea")
	void RemoveAllTriggerAreas();

	UFUNCTION(BlueprintCallable, Category = "ScriptedTrigger|Sounds")
	void PlayPortrait(ERTSPortraitTypes PortraitType, USoundBase* VoiceLine) const;

	virtual void OnTriggerAreaCallback(AActor* OverlappingActor, const int32 TriggerId, ATriggerArea* TriggerVolume);

	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptedTrigger|TriggerArea")
	void BP_OnTriggerAreaCallback(AActor* OverlappingActor, const int32 TriggerId, ATriggerArea* TriggerVolume);

private:
	UPROPERTY()
	TArray<FScriptedTriggerAreaRegistration> M_RegisteredTriggerAreas;

	UPROPERTY()
	TArray<FScriptedTriggerAreaRegistration> M_PendingRegisteredTriggerAreas;

	UPROPERTY()
	TSubclassOf<ATriggerArea> M_TriggerAreaRectangleClass;

	UPROPERTY()
	TSubclassOf<ATriggerArea> M_TriggerAreaSphereClass;

	UPROPERTY()
	bool bM_IsProcessingTriggerAreaCallbacks = false;

	ATriggerArea* CreateTriggerArea(const EScriptedTriggerAreaShape TriggerAreaShape,
	                                const FVector& Location,
	                                const FRotator& Rotation,
	                                const FVector& Scale,
	                                const ETriggerOverlapLogic TriggerOverlapLogic,
	                                const float DelayBetweenCallbacks,
	                                const int32 MaxCallbacks,
	                                const int32 TriggerId);

	bool LoadTriggerAreaClasses();
	bool GetHasValidTriggerAreaClass(const EScriptedTriggerAreaShape TriggerAreaShape) const;
	bool GetIsValidWorldContext() const;

	void RegisterTriggerArea(ATriggerArea* TriggerArea,
	                         const int32 TriggerId,
	                         const float DelayBetweenCallbacks,
	                         const int32 MaxCallbacks);

	void MarkTriggerAreasForRemovalById(const int32 TriggerId);
	void MarkTriggerAreasForRemovalById(TArray<FScriptedTriggerAreaRegistration>& TriggerAreaRegistrations, const int32 TriggerId);
	void MarkAllTriggerAreasForRemoval();
	void MarkAllTriggerAreasForRemoval(TArray<FScriptedTriggerAreaRegistration>& TriggerAreaRegistrations);

	void FlushPendingTriggerAreaChanges();
	void FlushPendingTriggerAreaChanges(TArray<FScriptedTriggerAreaRegistration>& TriggerAreaRegistrations);

	void ProcessTriggerAreaRegistrationOverlap(FScriptedTriggerAreaRegistration& TriggerAreaRegistration,
	                                           AActor* OverlappingActor,
	                                           ATriggerArea* TriggerArea,
	                                           const float CurrentTimeSeconds);

	UFUNCTION()
	void OnTriggerAreaOverlap(AActor* OverlappingActor, ATriggerArea* TriggerArea);
};
