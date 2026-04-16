#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "MissionTriggerVolumesManager.generated.h"

class ATriggerArea;
class UMissionBase;

UENUM()
enum class EMissionTriggerAreaShape : uint8
{
	Sphere,
	Rectangle
};

USTRUCT()
struct FMissionTriggerAreaRegistration
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_Mission;

	UPROPERTY()
	TWeakObjectPtr<ATriggerArea> M_TriggerArea;

	int32 M_TriggerId = INDEX_NONE;
	float M_DelayBetweenCallbacks = 0.0f;
	float M_LastCallbackTime = -1.0f;
	int32 M_MaxCallbacks = -1;
	int32 M_CallbackCount = 0;
};

/**
 * @brief Tracks spawned mission trigger areas and routes overlap callbacks to each mission owner safely.
 */
UCLASS(ClassGroup=(Mission), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UMissionTriggerVolumesManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UMissionTriggerVolumesManager();

	virtual void BeginPlay() override;

	/**
	 * @brief Creates and registers a trigger area so overlap callbacks can be throttled and mission-scoped.
	 * @param Mission Mission that owns the callback registration.
	 * @param TriggerAreaShape Shape type used to resolve the configured trigger actor class.
	 * @param Location World location where the trigger actor is spawned.
	 * @param Rotation World rotation where the trigger actor is spawned.
	 * @param Scale Shape scale payload applied to the trigger collision component.
	 * @param TriggerOverlapLogic Collision overlap filtering for player/enemy overlap channels.
	 * @param DelayBetweenCallbacks Cooldown in seconds between successive mission callbacks for this trigger.
	 * @param MaxCallbacks Max callback count before auto-destroying the trigger, negative for infinite callbacks.
	 * @param TriggerId Mission-defined id forwarded to the callback.
	 * @return Spawned trigger area actor, or nullptr when setup fails.
	 */
	ATriggerArea* CreateTriggerAreaForMission(UMissionBase* Mission,
	                                          const EMissionTriggerAreaShape TriggerAreaShape,
	                                          const FVector& Location,
	                                          const FRotator& Rotation,
	                                          const FVector& Scale,
	                                          const ETriggerOverlapLogic TriggerOverlapLogic,
	                                          const float DelayBetweenCallbacks,
	                                          const int32 MaxCallbacks,
	                                          const int32 TriggerId);

	void RemoveTriggerAreasForMissionById(UMissionBase* Mission, const int32 TriggerId);
	void RemoveAllTriggerAreasForMission(UMissionBase* Mission);

private:
	// Tracks per-trigger callback state per mission to support cooldown, id filtering, and limit-based auto cleanup.
	UPROPERTY()
	TArray<FMissionTriggerAreaRegistration> M_RegisteredTriggerAreas;

	UPROPERTY()
	TSubclassOf<ATriggerArea> M_TriggerAreaRectangleClass;

	UPROPERTY()
	TSubclassOf<ATriggerArea> M_TriggerAreaSphereClass;

	bool BeginPlay_LoadTriggerAreaClasses();
	bool GetHasValidTriggerAreaClass(const EMissionTriggerAreaShape TriggerAreaShape) const;
	bool GetIsValidMission(const UMissionBase* Mission) const;
	bool GetIsValidWorldContext() const;

	void RegisterTriggerArea(UMissionBase* Mission,
	                         ATriggerArea* TriggerArea,
	                         const int32 TriggerId,
	                         const float DelayBetweenCallbacks,
	                         const int32 MaxCallbacks);

	void RemoveDestroyedEntries();
	void DestroyTriggerAreaAndRemoveRegistration(const int32 RegistrationIndex);

	UFUNCTION()
	void OnTriggerAreaOverlap(AActor* OverlappingActor, ATriggerArea* TriggerArea);
};
