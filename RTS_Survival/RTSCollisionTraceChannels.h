#pragma once

#include "CoreMinimal.h"
// Trace channel to hit enemy units, should be blocked by collision primitives on enemy objects where
// we want weapons to be able to hit the enemy's unit.
// should be ignored by everything else.
#define COLLISION_TRACE_ENEMY ECC_GameTraceChannel5

// Collision object channel for hitting enemies 
// should be blocked by components on enemy's units that allow weapon hits.
// should be ignored by everything else.
#define COLLISION_OBJ_ENEMY ECC_GameTraceChannel6

// Trace channel to hit player units, should be blocked by collision primitives on player objects where
// we want weapons to be able to hit the player's unit.
// should be ignored by everything else.
#define COLLISION_TRACE_PLAYER ECC_GameTraceChannel7

// Collision object channel for hitting player 
// should be blocked by components on player units that allow weapon hits.
// should be ignored by everything else.
#define COLLISION_OBJ_PLAYER ECC_GameTraceChannel9

// Collision object channel for hitting pickup items
// should generally be ignored.
#define COLLISION_OBJ_PICKUPITEM ECC_GameTraceChannel4

// The building object collision channel should not be blocked but overlapped to allow for the preview to
// go over it but not be blocked by it.
#define COLLISION_OBJ_BUILDING_PLACEMENT ECC_GameTraceChannel8

// should only be blocked by floors or landscape or other objects on which the navmesh is placed.
#define COLLISION_TRACE_LANDSCAPE ECC_GameTraceChannel2

// should be blocked by components on both player and enemy units that allow weapon hits.
#define COLLISION_OBJ_PROJECTILE ECC_GameTraceChannel3

// Object channel of the hull blockers of enemy vehicles; those of the player use ECC_Vehicle.
// Split by side so a harvester hull can block the hulls of the other side and pass through those of its own.
// Its default response in DefaultEngine.ini MUST stay Block. Chaos pre-filters whole bodies by OR-ing the channel
// numbers of all their shapes: an enemy chassis (EnemyObj, 19) with its welded hull box (23) is seen as channel 23,
// and when the landscape ignores that channel the broadphase never pairs them and enemy vehicles drop through it.
// Only the hull boxes themselves decide who they meet: they ignore every channel but the two hull channels.
#define COLLISION_OBJ_ENEMY_HULL ECC_GameTraceChannel10