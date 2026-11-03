#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckEngineFireDeathSettings.h"

#include "WreckEngineFireDeathComponent.generated.h"

class UStaticMeshComponent;
class URTSFireSubsystem;

/**
 * @brief Added at runtime to a freshly spawned tank wreck when the tank died during an engine fire.
 * Blows the engine deck apart (fuel explosion + deck debris), keeps the open engine bay burning, spills burning
 * fuel next to the hull and chars the wreck around the engine with glowing embers that cool down.
 * When the ammunition cooked off as well, the turret toss owns the big explosion and this only adds the fuel fire.
 */
UCLASS(ClassGroup=(RTS))
class RTS_SURVIVAL_API UWreckEngineFireDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWreckEngineFireDeathComponent();

	/**
	 * @brief Starts the fuel death; call once right after the wreck spawned.
	 * @param Settings Explosion, fire, spill and char tuning from the tank that died.
	 * @param EngineWorldLocation Where the live tank's engine fire burned as it died.
	 * @param EngineDirection Horizontal direction from the hull centre towards the engine; the deck is thrown that way.
	 * @param bIsAmmoCookOffDeath True when the turret toss already exploded this wreck; skips the blast and deck debris.
	 * @return False when the wreck has no hull mesh; nothing happens then.
	 */
	bool StartEngineFireDeath(const FWreckEngineFireDeathSettings& Settings, const FVector& EngineWorldLocation,
	                          const FVector& EngineDirection, bool bIsAmmoCookOffDeath);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool TryFindHullMesh();
	FVector GetEngineDeckLocation(const FVector& EngineWorldLocation) const;
	void SpawnExplosion(ERTS_ExplosionType ExplosionType, USoundCue* OverrideSound) const;
	void StartSecondaryExplosion();
	void StartDeckDebris() const;
	void StartDeckFire();
	void StartFuelSpill();
	FVector GetFuelSpillGroundLocation() const;
	void ApplyCharOverlay() const;
	void StopFire(int32& InOutFireHandle) const;

	bool GetIsValidHullMesh() const;
	bool GetIsValidFireSubsystem() const;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_HullMesh;

	UPROPERTY()
	TWeakObjectPtr<URTSFireSubsystem> M_FireSubsystem;

	FWreckEngineFireDeathSettings M_Settings;

	// The engine on the wreck: the live engine location, held no higher than the top of the wreck hull.
	FVector M_EngineDeckLocation = FVector::ZeroVector;
	// Horizontal unit direction from the hull centre towards the engine.
	FVector M_EngineDirection = FVector::ZeroVector;

	FTimerHandle M_SecondaryExplosionTimerHandle;
	int32 M_DeckFireHandle = INDEX_NONE;
	int32 M_FuelFireHandle = INDEX_NONE;
};
