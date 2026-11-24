#pragma once

#include "CoreMinimal.h"
// ----------------- Up to four players (1 human, the rest AI) -----------------
// The channels below only split the human from the AI side: "PLAYER" is player 1, "ENEMY" is every AI player.
// Which player a weapon can hit is decided on top of them by FRTSPlayerCollision (Utils/CollisionSetup): units carry
// their owner as a mask filter bit and every weapon query ignores the bits of the shooter and its allies.
// Do not pick a weapon channel from an owning player here or in gameplay code; ask FRTSPlayerCollision.

// Trace channel of the HUMAN player's weapons (it "hits the enemy"): blocked by collision primitives on AI-owned
// objects where we want weapons to be able to hit the unit.
// should be ignored by everything else.
#define COLLISION_TRACE_ENEMY ECC_GameTraceChannel5
// Same channel under the name of who fires on it.
#define COLLISION_TRACE_HUMAN_WEAPONS COLLISION_TRACE_ENEMY

// Collision object channel for hitting enemies 
// should be blocked by components on enemy's units that allow weapon hits.
// should be ignored by everything else.
#define COLLISION_OBJ_ENEMY ECC_GameTraceChannel6

// Trace channel of the weapons of ALL AI players (it "hits the player"): blocked by collision primitives on human-
// owned objects, and on AI-owned objects too so AI players can fight each other; the query's ignore mask keeps an
// AI shot from hitting its own player and allies.
// should be ignored by everything else.
#define COLLISION_TRACE_PLAYER ECC_GameTraceChannel7
// Same channel under the name of who fires on it.
#define COLLISION_TRACE_AI_WEAPONS COLLISION_TRACE_PLAYER

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