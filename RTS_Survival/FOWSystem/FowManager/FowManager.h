// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterfaceExport.h"
#include "GameFramework/Actor.h"
#include "FowManager.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UFowComp;

/**
 * @brief The fow manager keeps track of all the fow components and updates the fow render targets,
 * post process material and niagara systems.
 * It also implements an enemy vision system that uses the vision radius on fow components set to passive
 * with enemy vision.
 *
 * There are three types of components:
 * 1 Fow_Active: These components are used to draw the fow and propagate vision.
 * 2 Fow_Passive: These components are used to hide/show actors based on player vision.
 * 3 Fow_Passive_EnemyVision: These components are used to hide/show actors based on player vision and
 * their sight radius is used to tag player units that are visible to the enemy.
 */
UCLASS()
class RTS_SURVIVAL_API AFowManager : public AActor, public INiagaraParticleCallbackHandler
{
	GENERATED_BODY()

public:
	/**
	 * @brief Constructs the AFowManager and initializes default properties.
	 *
	 * Initializes default values for this actor's properties, sets initial flags, and prepares the Fog of War manager for use.
	 */
	AFowManager(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Adds a Fog of War component to the manager's list of participants.
	 *
	 * @param FowComp The Fog of War component to add; used to manage its visibility and/or contribute to vision.
	 * @note The component will be added as active or passive based on its FowBehaviour.
	 * @pre FowComp must be a valid pointer.
	 */
	void AddFowParticipant(UFowComp* FowComp);

	/**
	 * @brief Removes a Fog of War component from the manager's list of participants.
	 *
	 * @param FowComp The Fog of War component to remove from participation.
	 * @note If the component is not found in the active or passive lists, the function will return without action.
	 * @pre FowComp must be a valid pointer.
	 */
	void RemoveFowParticipant(UFowComp* FowComp);

	/**
	 * @brief Receives particle data from a Niagara system and processes it accordingly.
	 *
	 * @param Data The array of particle data received from the Niagara system.
	 * @param NiagaraSystem The Niagara system that provided the data.
	 * @param SimulationPositionOffset The simulation position offset, if any.
	 * @note This function handles data from different Niagara systems and dispatches it to the appropriate handler.
	 * @pre NiagaraSystem must be a valid pointer.
	 */
	virtual void ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data,
	                                                UNiagaraSystem* NiagaraSystem,
	                                                const FVector& SimulationPositionOffset) override;

	/**
	 * @brief Requests a fog visibility update for a specific component.
	 *
	 * @param RequestingComponent The component requesting a fog visibility update; used to determine its visibility state.
	 * @note The component will be added to the list of components requesting visibility updates.
	 * @pre RequestingComponent must be a valid pointer and implement the IFowVisibility interface.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RequestComponentFogVisibility(UFowComp* RequestingComponent);

	/**
	 * @brief Removes a component from the list of components requesting fog visibility updates.
	 *
	 * @param Component The component to remove from the visibility request list.
	 * @note If the component is not in the list, the function will return without action.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RemoveComponentFromVisibilityRequest(UFowComp* Component);

	UTextureRenderTarget2D* GetIsValidActiveRT() const;
	UTextureRenderTarget2D* GetIsValidPassiveRT() const;
	inline float GetMapExtent () const { return MapExtent; }

protected:
	/**
	 * @brief Called when the game starts or when spawned.
	 *
	 * Initializes the Fog of War manager and prepares it for operation.
	 */
	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;

	/**
	 * @brief Called every frame to update the Fog of War manager.
	 *
	 * @param DeltaTime The time elapsed since the last frame.
	 * @note Updates the draw buffer, checks component validity, and handles readback and enemy vision updates.
	 */
	virtual void Tick(float DeltaTime) override;

	// Render target meant for static units, updated only if needed
	UPROPERTY(BlueprintReadWrite, Category = "FogOfWar|RT")
	UTextureRenderTarget2D* ActiveRT;

	UPROPERTY(BlueprintReadWrite, Category = "FogOfWar|RT")
	UTextureRenderTarget2D* PassiveRT;

	UPROPERTY(BlueprintReadWrite, Category = "FogOfWar|Init")
	bool bIsInitialized;

	UPROPERTY(BlueprintReadWrite, Category = "FogOfWar|Start")
	bool bIsStarted;

	// -------------------- Settings --------------------

	// The system will retry starting the Fog of War subsystem next frame in case it tried to start it before being initialized,
	// for any reasons. This counter tells how many times we may try, in frames
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	int32 PendingStartLimit;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString NiagaraDrawBufferName;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString NiagaraReadBackBufferName;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString PlayerPositionsBufferName;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString EnemyPositionVisionBufferName;

	// Map extent to capture, in cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	float MapExtent;

	// Render targets size, in pixel
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	int32 RenderTargetSize;

	// Tick rate of the Fog of War subsystem (drawing the FoW)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	float FowTickRate;

	// System to use to propagate units' vision and draw the FoW RT
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Niagara")
	TObjectPtr<UNiagaraSystem> FowSystem;

	// System to use for visibility readback events
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Niagara")
	TObjectPtr<UNiagaraSystem> FoWReadBackSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Niagara")
	TObjectPtr<UNiagaraSystem> FowEnemyVisionSystem;

	// Fog of war post process material to draw
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Material")
	TObjectPtr<UMaterialInterface> PostProcessMaterial;

	UPROPERTY(BlueprintReadWrite, Category = "Settings|Niagara")
	TObjectPtr<UNiagaraComponent> NiagaraDraw;

	UPROPERTY(BlueprintReadWrite, Category = "Settings|Niagara")
	TObjectPtr<UNiagaraComponent> NiagaraReadBack;

	UPROPERTY(BlueprintReadWrite, Category = "Settings|Niagara")
	TObjectPtr<UNiagaraComponent> NiagaraEnemyVision;

private:
	/** List of active Fog of War components contributing to vision. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UFowComp>> M_ActiveFowComponents;

	/** List of passive Fog of War components being hidden/shown based on vision. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UFowComp>> M_PassiveFowComponents;

	/** Buffer containing positions and vision ranges of active components. */
	UPROPERTY()
	TArray<FVector> M_DrawBuffer;

	/** Components requesting fog visibility updates. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UFowComp>> M_ComponentsRequestingFogVisibility;

	/** Passive components awaiting readback results. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UFowComp>> M_CurrentReadBackPassiveComponents;

	/** Player components awaiting enemy vision readback. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UFowComp>> M_CurrentPlayerCompsForEnemyVisionReadBack;

	/**
	 * @brief Checks if the given Fog of War component is unique among active participants.
	 *
	 * @param FowComp The Fog of War component to check for uniqueness.
	 * @return True if the component is not already in the active participants list; false otherwise.
	 * @pre FowComp must be a valid pointer.
	 */
	bool GetIsActiveParticipantUnique(UFowComp* FowComp) const;

	/**
	 * @brief Checks if the given Fog of War component is unique among passive participants.
	 *
	 * @param FowComp The Fog of War component to check for uniqueness.
	 * @return True if the component is not already in the passive participants list; false otherwise.
	 * @pre FowComp must be a valid pointer.
	 */
	bool GetIsPassiveParticipantUnique(UFowComp* FowComp) const;

	/**
	 * @brief Starts the Fog of War subsystem.
	 *
	 * Initializes the system, starts ticking, and prepares for updating Fog of War effects.
	 * @note Will retry initialization if not successful immediately, up to PendingStartLimit times.
	 */
	void StartFow();

	/**
	 * @brief Stops the Fog of War subsystem.
	 *
	 * Disables ticking, clears buffers, and resets state.
	 */
	void StopFow();

	/**
	 * @brief Adds a Fog of War component as an active participant.
	 *
	 * @param FowComp The Fog of War component to add as active; used to contribute to vision.
	 * @note The component's location and vision radius will be used to update the draw buffer.
	 * @pre FowComp must be a valid pointer and not already in the active participants list.
	 */
	void AddActiveFowParticipant(UFowComp* FowComp);

	/**
	 * @brief Adds a Fog of War component as a passive participant.
	 *
	 * @param FowComp The Fog of War component to add as passive; used to be hidden or shown based on vision.
	 * @note The component will be monitored for visibility updates.
	 * @pre FowComp must be a valid pointer and not already in the passive participants list.
	 */
	void AddPassiveFowParticipant(UFowComp* FowComp);

	/**
	 * @brief Updates the draw buffer with the locations and vision ranges of all active Fog of War components.
	 *
	 * Populates the draw buffer with XY positions and Z vision ranges for each active component.
	 * @pre Assumes all components in M_ActiveFowComponents and their owners are valid.
	 */
	void UpdateDrawBuffer();

	/**
	 * @brief Requests an update to the enemy vision system.
	 *
	 * @note Will only proceed if not already awaiting a previous enemy vision update.
	 */
	void AskUpdateEnemyVision();

	/**
	 * @brief Updates the enemy vision buffer with enemy positions and player positions.
	 *
	 * Prepares data for the enemy vision Niagara system to process visibility of player units.
	 * @pre Assumes all relevant components are valid.
	 * @post Triggers a Niagara system reinitialization to process the new data.
	 */
	void UpdateEnemyVision();

	/**
	 * @brief Handles the data received from the enemy vision Niagara system.
	 *
	 * @param Data The particle data containing visibility information for player units.
	 * @note Updates the visibility status of player units based on enemy vision.
	 */
	void OnReceivedEnemyVisionUpdate(const TArray<FBasicParticleData>& Data);

	/** Flag indicating if an enemy vision update is pending. */
	bool bM_IsPendingEnemyVisionUpdate;

	/**
	 * @brief Handles the case when the Niagara draw component is invalid.
	 *
	 * Logs an error, clears active components, and stops the Fog of War subsystem.
	 */
	void OnInvalidNiagaraDraw();

	/** Counter for the number of attempts to start the Fog of War subsystem. */
	int32 M_StartupAttempts;

	/**
	 * @brief Ensures that all stored Fog of War components and their owners are valid.
	 *
	 * Removes any invalid components from active and passive lists.
	 * @post All components in M_ActiveFowComponents and M_PassiveFowComponents are valid, and M_DrawBuffer is resized accordingly.
	 */
	void EnsureComponentsAreValid();

	/**
	 * @brief Requests a readback update for passive components to determine their visibility.
	 *
	 * @note Will only proceed if not already awaiting a previous readback answer and if there are passive components.
	 */
	void AskReadBack();

	/**
	 * @brief Updates the readback buffer with the locations of passive Fog of War components.
	 *
	 * Prepares data for the readback Niagara system to determine visibility of passive units.
	 * @pre Assumes all components in M_PassiveFowComponents and their owners are valid.
	 * @post Triggers a Niagara system reinitialization to process the new data.
	 */
	void UpdateReadBackBuffer();

	/**
	 * @brief Handles the data received from the readback Niagara system.
	 *
	 * @param Data The particle data containing visibility information for passive units.
	 * @note Updates the visibility status of passive units based on player vision.
	 */
	void OnReceivedReadBackUpdate(const TArray<FBasicParticleData>& Data);

	/**
	 * @brief Extracts a valid particle index from the particle's velocity vector.
	 *
	 * @param ParticleVector The velocity vector of the particle, containing the index in its X component.
	 * @param OutIndex Output parameter; set to the extracted index if valid.
	 * @return True if the extracted index is valid and within the bounds of the components array; false otherwise.
	 */
	bool GetValidParticleIndex(const FVector& ParticleVector, int32& OutIndex) const;

	/** Set to true after we updated the readback buffer with all the passive components,
	 * we do not allow additional readback buffer updates until this current one is answered.
	 * @note This boolean also prevents the system from answering one readback request multiple times.
	 */
	bool bM_IsPendingReadBackAnswer;

	/**
	 * Set to true after we updated the enemy vision buffer with all the player components and enemy vision components,
	 * we do not allow additional enemy vision buffer updates until this current one is answered.
	 * @note This boolean also prevents the system from answering one enemy vision request multiple times.
	 */
	bool bM_IsPendingEnemyVisionAnswer;
};
