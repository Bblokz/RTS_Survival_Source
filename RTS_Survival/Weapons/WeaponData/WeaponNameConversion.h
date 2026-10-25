// Copyright (c) Bas Blokzijl. All rights reserved.
#pragma once

#include "CoreMinimal.h"

enum class EWeaponName : uint32;

/**
 * @brief Resolves a Blueprint-authored weapon name to the enum; Blueprints cannot hold EWeaponName since it is uint32.
 * Accepts "Kar_98k" and "EWeaponName::Kar_98k" and follows the EWeaponName EnumRedirects in DefaultEngine.ini.
 * @param WeaponName The weapon name as typed on a Blueprint pin or in the details panel.
 * @return DEFAULT_WEAPON (and reports an error) when the name matches no entry.
 */
EWeaponName Global_GetWeaponNameFromFName(const FName WeaponName);

// Reflection based so it always matches the enumerator name that Blueprints store.
FName Global_GetWeaponNameAsFName(const EWeaponName WeaponName);

// Every selectable weapon name, for meta=(GetOptions) dropdowns on FName weapon fields.
TArray<FName> Global_GetWeaponNameOptions();
