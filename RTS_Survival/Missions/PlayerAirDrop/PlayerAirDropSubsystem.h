// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "PlayerAirDropSubsystem.generated.h"

class UGlobalAbility;
class UGlobalAbilitiesManager;

/** Units a mission flies in for the player, and the aircraft that bring them. */
USTRUCT(BlueprintType)
struct FPlayerAirDropPayload
{
	GENERATED_BODY()

	// Landed by the cargo plane ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	TArray<ETankSubtype> Vehicles;

	// Parachuted by the paratrooper ability (or landed by the cargo plane when bParachuteSquads is off).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	TArray<ESquadSubtype> Squads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	bool bParachuteSquads = true;

	// Any German aircraft drop ability; only its plane and drop behaviour are used, the payload is replaced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	EGlobalAbility CargoPlaneAbility = EGlobalAbility::GA_GerPzIIIFlak41Drop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop",
		meta = (EditCondition = "bParachuteSquads", EditConditionHides))
	EGlobalAbility ParachuteAbility = EGlobalAbility::GA_GerParatroopers;

	bool GetIsEmpty() const { return Vehicles.IsEmpty() && Squads.IsEmpty(); }
};

/**
 * @brief Missions call DeliverPlayerUnitsByAir to fly reinforcements in for the player with copies of the German
 * global drop abilities (no cost, no cooldown, not on the ability panel). Keeps the copies alive until they end.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerAirDropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief Flies the payload in towards the landing point.
	 * @param LandingPoint Where the units land or the paratroopers come down.
	 * @param Payload The units and the abilities that bring them.
	 * @return False when nothing could be launched (empty payload, missing ability template or player manager).
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Air Drop")
	bool DeliverPlayerUnitsByAir(const FVector& LandingPoint, const FPlayerAirDropPayload& Payload);

	static UPlayerAirDropSubsystem* Get(const UObject* WorldContextObject);

	int32 GetRunningAbilityCount() const { return M_RunningAbilities.Num(); }

private:
	// Copies in flight; the transient package does not keep them alive.
	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> M_RunningAbilities;

	UGlobalAbility* CreateCargoPlaneCopy(const FPlayerAirDropPayload& Payload) const;
	UGlobalAbility* CreateParachuteCopy(const FPlayerAirDropPayload& Payload) const;
	UGlobalAbilitiesManager* GetPlayerGlobalAbilitiesManager() const;
	void HandleAbilityEnded(UGlobalAbility* Ability);
};
