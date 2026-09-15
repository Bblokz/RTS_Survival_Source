// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/TeamWeapons/CrewPositions/CrewPositionType.h"
#include "TeamWeaponCrewMontages.generated.h"

class UAnimMontage;

/**
 * @brief One playable crew configuration; used as the base of a crew role and as the payload of an override.
 * Assign montages to the FullBody slot so they override the weapon aim offset while the crew operates the weapon.
 */
USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontageEntry
{
	GENERATED_BODY()

	bool GetHasMontage() const { return Montage != nullptr; }
	bool GetHasIdleLoopMontage() const { return IdleLoopMontage != nullptr; }

	// Full body montage. Null means: play nothing for this role (no error is reported).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	// Looping roles: straight play-rate multiplier.
	// Reacting roles: multiplied with the reload-synced play rate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	// False: Montage loops while the operator is in position on the deployed weapon.
	// True: Montage plays once per reload start of the first weapon and is stretched to the reload time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	bool bReactToWeaponFire = false;

	// Only used when bReactToWeaponFire. Loops between reactions; null means normal idle between reactions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew",
		meta = (EditCondition = "bReactToWeaponFire"))
	TObjectPtr<UAnimMontage> IdleLoopMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew",
		meta = (EditCondition = "bReactToWeaponFire", ClampMin = "0.01"))
	float IdleLoopPlayRate = 1.0f;
};

/**
 * @brief One override serves every squad subtype listed in SquadSubtypes.
 * Lets a single crouched spotter montage cover all mortar squads without duplicating the entry.
 */
USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontageOverride
{
	GENERATED_BODY()

	bool GetAppliesToSubtype(const ESquadSubtype TeamWeaponSquadSubtype) const
	{
		return SquadSubtypes.Contains(TeamWeaponSquadSubtype);
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	TArray<ESquadSubtype> SquadSubtypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontageEntry Entry;
};

/**
 * @brief Base entry for one crew role plus the subtype overrides that replace it.
 * The first override whose subtype list contains the team weapon squad subtype wins.
 */
USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontage
{
	GENERATED_BODY()

	/**
	 * @brief Resolves the entry for the provided subtype so overrides stay a pure data lookup.
	 * @param TeamWeaponSquadSubtype Subtype of the team weapon squad used as override key.
	 * @return The first matching override entry, otherwise the base entry.
	 */
	const FTeamWeaponCrewMontageEntry& ResolveEntry(const ESquadSubtype TeamWeaponSquadSubtype) const;

	// Used by every team weapon squad that has no matching override.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontageEntry Base;

	// First override whose SquadSubtypes contains the team weapon squad subtype wins.
	// An override with a null montage deliberately disables this role for the listed subtypes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	TArray<FTeamWeaponCrewMontageOverride> Overrides;
};

/**
 * @brief Designer container with one crew montage per crew position type of a team weapon.
 * Set on the squad unit animation instance defaults; resolved per operator by the team weapon controller.
 */
USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontages
{
	GENERATED_BODY()

	FTeamWeaponCrewMontages();

	/**
	 * @brief Picks the entry a crew member must play for its role and team weapon squad subtype.
	 * @param CrewRole Crew position type the operator was assigned to.
	 * @param TeamWeaponSquadSubtype Subtype of the team weapon squad used as override key.
	 * @return Matching override entry, else the base entry; nullptr for ECrewPositionType::None.
	 */
	const FTeamWeaponCrewMontageEntry* ResolveEntry(const ECrewPositionType CrewRole,
	                                                const ESquadSubtype TeamWeaponSquadSubtype) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage Gunner;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage Loader;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage Spotter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage AdditionalLoader;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage Commander;

private:
	const FTeamWeaponCrewMontage* GetMontageForRole(const ECrewPositionType CrewRole) const;
};
