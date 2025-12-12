// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "CrushDestructionTypes/ECrushDestructionType.h"
#include "ImpulseOnCrushed/FImpulseOnCrushed.h"
#include "RTS_Survival/Collapse/VerticalCollapse/RTSVerticalCollapseSettings.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "RTS_Survival/Environment/EnvironmentActor/EnvironmentActor.h"
#include "VerticalCollapseOnCrushed/FVerticalCollapseOnCrushed.h"
#include "DestructableEnvActor.generated.h"

enum class ECrushDestructionType : uint8;
class IRTSNavAgentInterface;
enum class ERTSNavAgents : uint8;
enum class ERTSDeathType : uint8;
struct FDestroySpawnActorsParameters;
struct FSwapToDestroyedMesh;
class UNiagaraSystem;
struct FCollapseFX;
struct FCollapseForce;
struct FCollapseDuration;
class UGeometryCollection;


DECLARE_MULTICAST_DELEGATE(FOnDestructibleCollapse);


/** @brief A destructable environment actor that can take damage and eventually collapse.
* When damaged and no Health is left calls OnUnitDies.
* @note Has SetupComponentCollisions which only sets collision to weapon traces for static meshes
* @note SEtuPComponentCollisions Disables collision and physics for geometry components.
*/

UCLASS()
class RTS_SURVIVAL_API ADestructableEnvActor : public AEnvironmentActor, public IAimOffsetProvider
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADestructableEnvActor(const FObjectInitializer& ObjectInitializer);

	// Destroys the actor with death animation/ other death logic implemented on unit dies.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void TriggerDestroyActor(ERTSDeathType DeathType);

	// Called when collapsed; vertically, impulse, geo swap with physics etc.
	FOnDestructibleCollapse OnDestructibleCollapse;


	/**
	 * @brief Bind this actor's crush-destruction overlap logic to a component's BeginOverlap.
	 * The component should have collision & overlap enabled (we'll ensure overlap is enabled).
	 * @param OverlapComponent Component that will raise overlap events.
	 * @param DestructionType  Which crush rule to apply when overlaps happen.
	 * @param CrushDeathType Use impulse on a geometry component or vertical collapse.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="CrushDestruction")
	void SetupCrushDestructionOverlap(UPrimitiveComponent* OverlapComponent, ECrushDestructionType DestructionType,
	                                  ECrushDeathType CrushDeathType);

	
	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief Configure impulse-on-crush behavior (target primitive, scale, delay).
	 * @param TargetPrimitive Primitive that should receive the impulse (must simulate physics at impulse time).
	 * Usually a geometry collection component {check physics set to simulate}.
	 * 
	 * @param CrushImpulseScale Multiplier for impulse strength.
	 * @param TimeTillImpulse Seconds to wait before applying the impulse.
	 */
	UFUNCTION(BlueprintCallable, Category="CrushDestruction")
	void ConfigureImpulseOnCrushed(UPrimitiveComponent* TargetPrimitive,
	                               float CrushImpulseScale,
	                               float TimeTillImpulse);
	
		UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AimOffset")
    	TArray<FVector> AimOffsetPoints = {
    		FVector(0, 0, 100), FVector(0, 50, 100),
    		FVector(0,-50,100)
    	};

	/**
	 * @brief Configure vertical collapse-on-crush (target primitive, speed, and audio).
	 * @param TargetPrimitive Primitive that should vertically collapse (must be Movable).
	 * Usually the main static mesh.
	 * @param CollapseSpeedScale Higher => quicker collapse (non-linear ramp).
	 * @param CollapseSound (optional) sound to play once when collapse begins.
	 * @param Attenuation (optional) attenuation asset to apply to CollapseSound.
	 * @param Concurrency (optional) concurrency asset to apply to CollapseSound.
	 */
	UFUNCTION(BlueprintCallable, Category="CrushDestruction")
	void ConfigureVerticalCollapse(UPrimitiveComponent* TargetPrimitive,
	                               float CollapseSpeedScale,
	                               USoundBase* CollapseSound,
	                               USoundAttenuation* Attenuation,
	                               USoundConcurrency* Concurrency);


	UFUNCTION(BlueprintCallable)
	void VerticalCollapseInRandomDirection();

	// Use is unit alive check; to prevent multiple death calls.
	// Will have is unit alive set to false when called so any overwritten logic must happen before super call.
	virtual void OnUnitDies(const ERTSDeathType DeathType);


	virtual void BeginDestroy() override;

	// Takes the damage and if no health is left calls OnUnitDies.
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	float Health = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	float DamageReduction = 0.f;

	// Called after damage is taken and now health is left or unit dies through manual trigger destroy.
	// At this point  the unit is set to IsAlive=false and will not trigger death anymore.
	UFUNCTION(BlueprintImplementableEvent, Category="Health")
	void BP_OnUnitDies(ERTSDeathType DeathType);


	// Only called if the mesh collapse does not result in destroying the actor.
	UFUNCTION(BlueprintImplementableEvent, Category="Health")
	void BP_OnMeshCollapsed();


	// Disables the collision on the mesh, calls FRTS_Collapse::CollapseMesh to async load geo collection and
	// collapse it with physics.
	// if set to NOT destroy owning actor after collapse then calls OnMeshCollapsedNoDestroy when done.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	void CollapseMesh(
		UGeometryCollectionComponent* GeoCollapseComp,
		TSoftObjectPtr<UGeometryCollection> GeoCollection,
		UMeshComponent* MeshToCollapse,
		FCollapseDuration CollapseDuration,
		FCollapseForce CollapseForce,
		FCollapseFX CollapseFX
	);


	/**
	 * @brief Collapses the mesh by changing the mesh to a destroyed one on the provided component.
	 * Can play VFX/SFX and eventually destroy the component.
	 * @param CollapseParameters To tune the collapse.
	 * @param bNoLongerBlockWeaponsPostCollapse Whether the mesh should no longer block weapon traces / projectiles after collapsing.
	 * @param AttachSystem
	 * @param AttachSound
	 * @param AttachOffset
	 * @param Attenuation
	 * @param SoundConcurrency
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	void CollapseMeshWithSwapping(
		FSwapToDestroyedMesh CollapseParameters,
		const bool bNoLongerBlockWeaponsPostCollapse = false,
		UNiagaraSystem* AttachSystem = nullptr,
		USoundCue* AttachSound = nullptr,
		const FVector AttachOffset = FVector::ZeroVector);


	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupComponentCollisions(TArray<UMeshComponent*> MeshComponents,
	                              TArray<UGeometryCollectionComponent*> GeometryComponents, bool bOverlapTanks) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void DestroyAndSpawnActors(
		const FDestroySpawnActorsParameters& SpawnParams,
		FCollapseFX CollapseFX);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void VerticalDestruction(
		const FRTSVerticalCollapseSettings& CollapseSettings,
		const FCollapseFX& CollapseFX);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnVerticalDestructionComplete();

private:
	void OnUnitDies_CheckForWireComponent() const;

	void AttemptAttachSpawnSystem(const FSwapToDestroyedMesh& CollapseParameters, UNiagaraSystem* AttachSystem);

	FTimerHandle M_PostCollapseTimerHandle;
	FTimerDelegate M_PostCollapseTimerDelegate;

	// Called when collapsed but actor not destroyed (as specified by the collapse settings).
	UFUNCTION()
	void OnMeshCollapsedNoDestroy(UGeometryCollectionComponent* GeoComponent, const bool bKeepGeometryVisible);

	// ------------------------- CRUSH DESTRUCTION ----------------------------------

	// Keeps track of the impulse timer, impulse target primitive and force settings.
	FImpulseOnCrushed M_ImpulseOnCrushed;

	// Keeps track of the type of crush destruction to apply on (the correct killing type of) overlap.
	ECrushDeathType M_CrushDeathType = ECrushDeathType::Impulse;

	UFUNCTION()
	void HandleBeginOverlap_HeavyTank(UPrimitiveComponent* OverlappedComponent,
	                                  AActor* OtherActor,
	                                  UPrimitiveComponent* OtherComp,
	                                  int32 OtherBodyIndex,
	                                  bool bFromSweep,
	                                  const FHitResult& SweepResult);

	UFUNCTION()
	void HandleBeginOverlap_MediumOrHeavy(UPrimitiveComponent* OverlappedComponent,
	                                      AActor* OtherActor,
	                                      UPrimitiveComponent* OtherComp,
	                                      int32 OtherBodyIndex,
	                                      bool bFromSweep,
	                                      const FHitResult& SweepResult);

	UFUNCTION()
	void HandleBeginOverlap_AnyTank(UPrimitiveComponent* OverlappedComponent,
	                                AActor* OtherActor,
	                                UPrimitiveComponent* OtherComp,
	                                int32 OtherBodyIndex,
	                                bool bFromSweep,
	                                const FHitResult& SweepResult);

	// Will trigger destroy on the actor when overlapping with a Heavy Tank nav agent.
	bool GetIsOverLapDestroyByHeavyTank(AActor* OtherActor);

	// Will trigger destroy on the actor when overlapping with a Heavy Tank Medium tank nav agent.
	bool GetIsOverLapDestroyByMediumOrHeavyTank(AActor* OtherActor);

	// Will trigger destroy on the actor when overlapping with a Heavy Tank Medium tank nav agent.
	bool GetIsOverlapByAnyTank(AActor* OtherActor);

	IRTSNavAgentInterface* GetNavAgentInterfaceFromOverlapActor(AActor* OverlappingActor) const;

	// ------------------------- END CRUSH DESTRUCTION ----------------------------------V

	UFUNCTION()
	void OnVerticalDestructionComplete();

	// Keeps track of the vertical collapse timing/target and ramp.
	FVerticalCollapseOnCrushed M_VerticalCollapseOnCrushed;
};
