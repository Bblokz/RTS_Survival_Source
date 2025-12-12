// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FowComp.generated.h"

enum class EFowBehaviour : uint8;
class AFowManager;

/**
 * @brief This component determines the Fog of War functionality of its owning actor.
 *
 * It can be set to either active or passive Fog of War behavior:
 * - **Active**: The unit creates a vision bubble of a specified radius.
 * - Fow_PassiveEnemyVision : The unit is hidden or shown depending on its position relative to the Fog of War and creates
 * vision for the enemy.
 * - **Passive**: The unit is hidden or shown depending on its position relative to the Fog of War.
 * @note After creation, make sure to call `StartFow` with the desired Fog of War type.
 * @note Make sure to specify the vision radius.
 * @post The component will start participating in the Fog of War system after initialization of the system.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UFowComp : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * @brief Constructs the UFowComp with default values.
     *
     * Initializes the component and sets default properties.
     */
    UFowComp();

    /**
     * @brief Sets the vision radius for active Fog of War components.
     *
     * @param NewVisionRadius The new vision radius to set; determines the size of the vision bubble.
     * @note A value of zero or less will not update the vision radius.
     */
    UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Fow")
    void SetVisionRadius(const float NewVisionRadius);

    /**
     * @brief Starts the Fog of War participation after a short delay.
     *
     * @param FowComponentBehaviour Determines whether the owner will have a vision bubble or be hidden/shown based on the Fog of War.
     * @param bStartImmediately Whether to ignore the delay start and start immediately; this can be used on units that
     * are not asynced spawned.
     * @note Without bStartImmediately There is a delay before participation starts to ensure the unit is fully initialized.
     * @post The component will begin participating in the Fog of War system.
     */
    UFUNCTION(BlueprintCallable, NotBlueprintable)
    void StartFow(const EFowBehaviour FowComponentBehaviour, const bool bStartImmediately = false);

    /**
     * @brief Called when the Fog of War visibility is updated for this component.
     *
     * @param Visibility The visibility value received from the Fog of War system; ranges from 0.0 (hidden) to 1.0 (fully visible).
     * @note Updates the owner's visibility or tags based on the component's Fog of War behavior.
     */
    void OnFowVisibilityUpdated(const float Visibility);

    /**
     * @brief Stops the component's participation in the Fog of War system.
     *
     * Removes the component from the Fog of War manager's lists and stops any pending timers.
     */
    void StopFowParticipation();

    /**
     * @brief Gets the vision radius of the component.
     *
     * @return The vision radius of the component.
     */
    inline float GetVisionRadius() const { return VisionRadius; };

    /**
     * @brief Gets the Fog of War behavior of the component.
     *
     * @return The Fog of War behavior (active or passive).
     */
    inline EFowBehaviour GetFowBehaviour() const { return M_FowBehaviour; };

protected:
    /**
     * @brief Called when the game starts.
     *
     * Initializes references and prepares the component for participation.
     */
    virtual void BeginPlay() override;

    /**
     * @brief Called when the component is destroyed.
     *
     * Cleans up and stops participation in the Fog of War system.
     *
     * @param EndPlayReason The reason why the play has ended.
     */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** The vision radius for active Fog of War components. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fow")
    float VisionRadius;

private:
    /** Reference to the Fog of War manager handling this component. */
    UPROPERTY()
    TWeakObjectPtr<AFowManager> M_FowManager;

    /**
     * @brief Checks if the Fog of War manager reference is valid.
     *
     * @return True if the manager is valid; false otherwise.
     * @note Attempts to retrieve the manager if not already valid.
     */
    bool GetIsValidFowManager();

    /**
     * @brief Starts participation in the Fog of War system after a specified delay.
     *
     * @param Seconds The delay in seconds before starting participation.
     * @note Used to ensure the unit is fully initialized before participating.
     */
    void StartFowParticipationInSeconds(const float Seconds);

    /**
     * @brief Callback function when it's time to start participating in the Fog of War.
     *
     * Adds the component to the Fog of War manager.
     */
    void OnFowStartParticipation();

    /** Timer handle for delaying the start of Fog of War participation. */
    FTimerHandle M_FowParticpationHandle;

    /** The Fog of War behavior of this component (active or passive). */
    EFowBehaviour M_FowBehaviour;
};
