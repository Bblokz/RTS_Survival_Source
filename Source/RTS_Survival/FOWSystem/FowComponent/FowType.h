#pragma once

#include "CoreMinimal.h"

UENUM()
enum class EFowBehaviour : uint8
{
 // This Fow component defines a vision bubble for the player using the vision range
 Fow_Active,
 // This fow component will hide/show the owning unit depending on the position in the fow
 // from the player's pov.
 Fow_Passive,
 // This fow component will hide/show the owning unit depending on the position in the fow
    // from the enemy's pov and provide its vision range to determine which player units
    // are visible for the enemy 
 Fow_PassiveEnemyVision
};