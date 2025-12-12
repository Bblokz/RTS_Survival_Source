// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerBuildRadiusManager.generated.h"


class UBuildRadiusComp;

/**
 * @brief Manages the player's build radii within the game environment.
 *
 * This component is responsible for tracking all build radius components (`URadiusComp`) associated with the player's buildings or units that support build radii. It ensures that only the necessary build radii are displayed to the player, taking into account overlapping radii to optimize performance and visual clarity. The component handles the registration and unregistration of radius components, validates their states, determines which radii should be shown, and sets up dynamic materials to visually represent overlapping areas.
 *
 * @details
 * **Key Responsibilities:**
 * - **Registration Management:** Keeps track of all `URadiusComp` instances by registering new components and unregistering them when they are no longer needed.
 * - **Validation:** Ensures all registered radius components are valid and removes any that are invalid or have been destroyed.
 * - **Display Logic:** Determines which radii should be displayed based on whether they are completely covered by other radii, thus avoiding unnecessary rendering.
 * - **Material Setup:** Configures dynamic material instances for radius components to visually represent overlaps, handling up to a specified maximum number of overlapping circles.
 * - **Debugging Support:** Provides functions to visualize overlapping radii and material parameters for debugging purposes when enabled.
 *
 * **Usage Workflow:**
 * 1. **Registration:** As buildings or units with build radii are created, their `URadiusComp` components should be registered using `RegisterBuildRadiusComponent()`.
 * 2. **Display Control:** Use `ShowBuildRadius(true)` to display the build radii when needed (e.g., when entering build mode) and `ShowBuildRadius(false)` to hide them.
 * 3. **Construction Support:** Retrieve the list of enabled build radii for construction purposes using `GetBuildRadiiForConstruction()`.
 * 4. **Unregistration:** When a building or unit is destroyed or no longer supports a build radius, its `URadiusComp` should be unregistered using `UnregisterBuildRadiusComponent()`.
 *
 * **Integration with Player Controller:**
 * This component is intended to be part of the player's controller (`CPPPlayerController`), which calls `ShowBuildRadius()` to control the visibility of the build radii based on the game's state.
 *
 * @note The build radii components are now registered and deregistered by the nomadic vehicles that use them in beginplay and
 * begindestroy respecitvely.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerBuildRadiusManager : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * @brief Constructs the `UPlayerBuildRadiusManager` component, setting default values.
     *
     * Initializes the component by disabling ticking and preparing it for use within the game.
     */
    UPlayerBuildRadiusManager();

    /**
     * @brief Shows or hides the player's build radii based on the specified flag.
     *
     * If `bShow` is `true`, the function displays the build radii that are not completely covered by others. If `bShow` is `false`, it hides all build radii managed by this component.
     *
     * @param bShow A boolean flag indicating whether to show (`true`) or hide (`false`) the build radii.
     * @note This function optimizes visual performance by only displaying necessary build radii.
     */
    void ShowBuildRadius(const bool bShow);

    /**
     * @brief Retrieves the list of enabled build radii for construction purposes.
     *
     * @return An array of pointers to `URadiusComp` representing the enabled build radii.
     * @note This function ensures that the radii are valid and currently enabled before returning them.
     */
    TArray<UBuildRadiusComp*> GetBuildRadiiForConstruction();

    /**
     * @brief Registers a build radius component with the manager.
     *
     * Adds the specified `RadiusComp` to the internal list of build radii for tracking and management. This allows the component to manage its visibility and interactions within the game.
     *
     * @param RadiusComp The build radius component to register; used to track and manage build radii associated with the player's assets.
     * @note Only valid components that are not already registered are added.
     */
    void RegisterBuildRadiusComponent(UBuildRadiusComp* RadiusComp);

    /**
     * @brief Unregisters a build radius component from the manager.
     *
     * Removes the specified `RadiusComp` from the internal list, stopping its management by this component. This is typically called when a building or unit is destroyed or no longer supports a build radius.
     *
     * @param RadiusComp The build radius component to unregister; used to remove it from tracking.
     * @note If the component is not found in the manager, an error is reported for debugging purposes.
     */
    void UnregisterBuildRadiusComponent(UBuildRadiusComp* RadiusComp);

protected:
    /**
     * @brief Called when the game starts; performs any necessary initialization.
     */
    virtual void BeginPlay() override;

private:
    /** Array of build radius components from buildings that support build radii */
    UPROPERTY()
    TArray<UBuildRadiusComp*> M_BuildRadii;

    /**
     * @brief Validates and cleans up the list of build radius components.
     *
     * Removes any invalid or destroyed radius components from the internal list to maintain integrity and prevent errors.
     *
     * @note This function is called internally to ensure the build radius list remains accurate.
     */
    void EnsureRadiiAreValid();

    /**
     * @brief Determines which build radii should be displayed to the player.
     *
     * @return An array of pointers to `URadiusComp` representing the radii that are not completely covered by others.
     * @note This function helps optimize visual performance by hiding radii that are entirely overlapped by others.
     */
    TArray<UBuildRadiusComp*> GetRadiiToShow();

    /**
     * @brief Checks if a given radius component is completely covered by other radii.
     *
     * Compares the specified `ValidRadiusCompI` against other registered radii to determine if it is entirely within any of them.
     *
     * @param ValidRadiusCompI The radius component to check; used as the reference for coverage comparison.
     * @return `true` if the radius is completely covered by any other radius; `false` otherwise.
     * @pre `ValidRadiusCompI` must be a valid pointer with a valid owner.
     */
    bool IsRadiusCompletelyCovered(UBuildRadiusComp* ValidRadiusCompI);

    /**
     * @brief Sets up the dynamic material parameters for a radius component to visualize overlapping areas.
     *
     * Configures the dynamic material instance of the specified `ValidRadiusComp` to represent overlaps with other radii. It adjusts material parameters to ensure visual accuracy of overlapping build radii.
     *
     * @param ValidRadiusComp The radius component for which to set up the material; used to obtain and modify its dynamic material instance.
     * @param RadiiToShow The list of other radius components that are currently being shown; used to determine which radii overlap with `ValidRadiusComp`.
     * @param MaxOtherCircles The maximum number of overlapping circles the material can handle; used to limit the number of parameters set on the material.
     * @note This function assumes that the material associated with `ValidRadiusComp` supports the necessary parameters for overlap visualization.
     * @pre `ValidRadiusComp` must have a valid owner and a dynamic material instance.
     */
    auto SetupRadiusComponentMaterial(UBuildRadiusComp* ValidRadiusComp, const TArray<UBuildRadiusComp*>& RadiiToShow,
                                      int32 MaxOtherCircles) -> void;

    /**
     * @brief Collects information about circles that overlap with the specified radius component.
     *
     * Identifies overlapping radii up to `MaxOtherCircles` and collects their centers and radii for use in material parameter setup.
     *
     * @param ValidRadiusComp The radius component for which to find overlapping circles; used as the reference circle.
     * @param RadiiToShow The list of other radius components that are currently being shown; used to compare for overlaps with `ValidRadiusComp`.
     * @param MaxOtherCircles The maximum number of overlapping circles to collect; used to limit the output arrays.
     * @param OutOtherCenters Output array of center positions of overlapping circles; used for setting material parameters.
     * @param OutOtherRadii Output array of radii of overlapping circles; corresponds to `OutOtherCenters`.
     * @note Only overlapping circles up to `MaxOtherCircles` are collected to match material limitations.
     * @pre `ValidRadiusComp` must have a valid owner.
     * @post `OutOtherCenters` and `OutOtherRadii` contain the centers and radii of overlapping circles, respectively.
     */
    void CollectOverlappingCircles(UBuildRadiusComp* ValidRadiusComp, const TArray<UBuildRadiusComp*>& RadiiToShow,
                                   int32 MaxOtherCircles, TArray<FVector>& OutOtherCenters,
                                   TArray<float>& OutOtherRadii);

    /**
     * @brief Visualizes overlapping circles for debugging purposes.
     *
     * Draws debug lines from the current component to each overlapping circle, labels them with their indices, and draws spheres representing their radii.
     *
     * @param CurrentComp The radius component being processed; used as the reference point for drawing debug visuals.
     * @param OtherCenters The centers of overlapping circles; used to draw debug lines and spheres.
     * @param OtherRadii The radii of overlapping circles; used to size the debug spheres.
     * @note This function is intended for debugging and visualization during development.
     * @pre `CurrentComp` must have a valid owner and a valid world context.
     */
    void DebugOtherCircles(UBuildRadiusComp* CurrentComp, const TArray<FVector>& OtherCenters, const TArray<float>& OtherRadii);

    /**
     * @brief Outputs the parameters set on the dynamic material for debugging purposes.
     *
     * Retrieves and logs the values of material parameters such as the number of other circles and the details of overlapping circles.
     *
     * @param CurrentComp The radius component whose material is being debugged; used to obtain the owner name for logging.
     * @param DynMaterial The dynamic material instance to inspect; used to retrieve and RTSlog parameter values.
     * @param OtherCenters The centers of overlapping circles; used for verification of material parameters.
     * @param OtherRadii The radii of overlapping circles; used for verification of material parameters.
     * @note This function is useful for ensuring that material parameters are correctly set during development.
     * @pre `DynMaterial` must be a valid dynamic material instance.
     */
    void DebugDynamicMaterial(UBuildRadiusComp* CurrentComp, UMaterialInstanceDynamic* DynMaterial,
                              TArray<FVector> OtherCenters, TArray<float> OtherRadii);

    /**
     * @brief Retrieves the list of enabled build radii from the manager.
     *
     * Filters the internal list of registered radii to include only those that are currently enabled.
     *
     * @return An array of pointers to `URadiusComp` representing the enabled build radii.
     * @note This function is used internally to obtain radii that should be considered for display and construction logic.
     */
    TArray<UBuildRadiusComp*> GetEnabledRadii();
};
