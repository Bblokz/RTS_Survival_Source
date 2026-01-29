// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "SquadUnitHealthComponent.generated.h"

class USquadHealthComponent;
class ACharacterObjectsMaster;

/**
 * @brief Health component for a squad member that forwards *all* HP mutations to its squad aggregate.
 *
 * The reference to the squad aggregate may be assigned after BeginPlay by the Squad Controller.
 * Until assigned, notifications are deliberately no-op (no error spam), to support BP init ordering.
 *
 * @note SetSquadHealthComponent: called by the Squad Controller once all units are known/loaded.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API USquadUnitHealthComponent : public UHealthComponent
{
	GENERATED_BODY()

public:
	USquadUnitHealthComponent() = default;

	/** Assign the owning squad aggregate. Safe to call after BeginPlay. */
	UFUNCTION(BlueprintCallable, Category="Squad|Health")
	void SetSquadHealthComponent(USquadHealthComponent* InSquadHealth);

	// -------- UHealthComponent overrides (must be virtual in base) --------

	/** Forwards exactly the dealt damage (post-reduction) to the squad; updates snapshot w/o double applying. */
	virtual bool TakeDamage(float& InOutDamage, const ERTSDamageType DamageType) override;

	/** Notifies the squad of the new current value and applies the delta to the aggregate. */
	virtual void SetCurrentHealth(float NewCurrentHealth) override;

	/** Forwards the positive heal delta to the squad and refreshes the snapshot (no double apply). */
	virtual bool Heal(float HealAmount) override;

	/** Notifies the squad of Max/Current delta; aggregate applies delta accordingly. */
	virtual void SetMaxHealth(float NewMaxHealth) override;

	virtual void OnOverwiteHealthbarVisiblityPlayer(ERTSPlayerHealthBarVisibilityStrategy Strategy) override;
	virtual void OnOverwiteHealthbarVisiblityEnemy(ERTSEnemyHealthBarVisibilityStrategy Strategy) override;

protected:
	virtual void BeginPlay() override;
	virtual void BeginPlay_ApplyUserSettingsHealthBarVisibility() override;

private:
	// Owning squad aggregate; intentionally no error spam if unset at BeginPlay.
	UPROPERTY()
	TWeakObjectPtr<USquadHealthComponent> M_SquadHealthComp;

	/** @return true if the squad aggregate is set; does not report errors (by design). */
	bool GetIsValidSquadHealth_NoReport() const;

	/**
	 * @brief Push our current snapshot to the squad.
	 * @param bAffectSquadCurrent If true, the aggregate also applies the Current delta to its totals.
	 */
	void NotifySquadOfState(bool bAffectSquadCurrent) const;

	// Unregister our snapshot when the owning actor dies.
	void BeginPlay_BindOwnerDeath();
	void OnOwnerDies();
};
