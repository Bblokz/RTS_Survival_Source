#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "CoverFinderDeveloperSettings.generated.h"

/**
 * @brief Exposes the small set of accuracy, cadence, and budget controls needed to tune infantry cover discovery.
 * Geometry classification details remain internal so maps share one predictable definition of cover.
 */
class UStaticMesh;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Infantry Cover Finder"))
class RTS_SURVIVAL_API URTSCoverFinderDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSCoverFinderDeveloperSettings();

	UPROPERTY(Config, EditAnywhere, Category="Cover Search")
	bool bM_EnableCoverSearch = true;

	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="1.0", UIMin="1.0", UIMax="60.0", Units="s"))
	float M_RescanIntervalSeconds = 5.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="60.0", UIMin="60.0", UIMax="300.0", Units="cm"))
	float M_SearchGridSpacing = 200.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="0.05", UIMin="0.05", UIMax="2.0", Units="ms"))
	float M_GameThreadBudgetMilliseconds = 0.35f;

	// Budget for the very first scan of a map. Nothing can take cover until that scan is published, so it may
	// cost more per frame than the periodic refreshes that follow it.
	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="0.05", UIMin="0.35", UIMax="10.0", Units="ms"))
	float M_FirstScanGameThreadBudgetMilliseconds = 4.0f;

	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover")
	bool bM_EnableAutomaticCoverUse = true;

	// With only squads selected, shows where each soldier will go under the cursor and sends them exactly there.
	// Off, squads move with the regular formation slots again.
	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview")
	bool bM_EnableSquadMovePreview = true;

	// The meshes the preview stands on each planned position, one per stance. Standing cover uses the high
	// cover mesh for both of its sides.
	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview")
	TSoftObjectPtr<UStaticMesh> M_PreviewStanceNoCover = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Stances/SM_Stance_NoCover.SM_Stance_NoCover")));

	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview")
	TSoftObjectPtr<UStaticMesh> M_PreviewStanceCrouchCover = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Stances/SM_Stance_CrouchCover.SM_Stance_CrouchCover")));

	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview")
	TSoftObjectPtr<UStaticMesh> M_PreviewStanceHighCover = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Stances/SM_Stance_HighCover.SM_Stance_HighCover")));

	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview")
	TSoftObjectPtr<UStaticMesh> M_PreviewStanceProneCover = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Stances/SM_Stance_ProneCover.SM_Stance_ProneCover")));

	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview")
	TSoftObjectPtr<UStaticMesh> M_PreviewStanceTrenchCover = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Stances/SM_Stance_TrenchCover.SM_Stance_TrenchCover")));

	// Instances of each stance mesh that are created up front, so a preview never has to allocate while the
	// cursor moves. A selection that needs more of one stance makes that stance's pool grow once.
	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview", meta=(ClampMin="0", ClampMax="512", UIMin="0", UIMax="128"))
	int32 M_PreviewStancePreloadCount = 32;

	// Added to the yaw of every stance mesh. The stance meshes were made from character poses and look along
	// their Y axis, like the character mesh they came from, so they are turned a quarter back.
	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview", meta=(ClampMin="-180.0", ClampMax="180.0", UIMin="-180.0", UIMax="180.0", Units="deg"))
	float M_PreviewStanceYawOffsetDegrees = -90.0f;

	// Limits how far an otherwise idle or already-in-range infantry unit may reposition itself for cover.
	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover", meta=(ClampMin="100.0", UIMin="100.0", UIMax="2500.0", Units="cm"))
	float M_AutomaticCoverSearchRadius = 1200.0f;

	// The same limit for infantry of the enemy (player 2). Larger than the player's, so enemy squads move into
	// cover that is further away by themselves instead of standing in the open, which makes them feel alive.
	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover", meta=(ClampMin="100.0", UIMin="100.0", UIMax="5000.0", Units="cm"))
	float M_EnemyAutomaticCoverSearchRadius = 2500.0f;

	// How often a soldier running to crouch or prone cover finishes the run with one of the approach moves of
	// its animation Blueprint (CoverAnimations > ApproachMoves), such as a slide or a roll. The move is only
	// played when it ends exactly on the cover position; otherwise the soldier walks in as usual. Zero never.
	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover", meta=(ClampMin="0", ClampMax="100", UIMin="0", UIMax="100", Units="%"))
	int32 M_CoverApproachMoveChancePercent = 30;

	// Staggers decisions across frames; movement completion itself is still handled immediately by the AI callback.
	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover", meta=(ClampMin="1", ClampMax="64", UIMin="1", UIMax="32"))
	int32 M_MaximumTacticalUnitUpdatesPerFrame = 8;

	// In a firefight soldiers pick cover by where the enemy is instead of taking the nearest point, and move to a
	// clearly better point when the enemy has moved around their cover.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover")
	bool bM_EnableCombatCoverRepositioning = true;

	// How far left or right of straight ahead a soldier in cover can aim; the range of the cover aim offsets.
	// A point from which the target is further to the side than this is never used against that target.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="20.0", ClampMax="90.0", UIMin="20.0", UIMax="90.0", Units="deg"))
	float M_CombatCoverMaximumAimYawDegrees = 90.0f;

	// An enemy within this angle of straight ahead counts as blocked by the cover. Smaller values make soldiers
	// insist on cover that squarely faces the enemy and reposition sooner when flanked.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="10.0", ClampMax="90.0", UIMin="10.0", UIMax="90.0", Units="deg"))
	float M_CombatCoverProtectedHalfAngleDegrees = 60.0f;

	// How much it counts that the cover stands between the soldier and the enemies its squad is fighting.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", UIMin="0.0", UIMax="3.0"))
	float M_CombatCoverProtectionWeight = 1.0f;

	// How much it counts that the soldier's own target is straight ahead instead of off to one side.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", UIMin="0.0", UIMax="3.0"))
	float M_CombatCoverFacingWeight = 0.5f;

	// How much a long walk under fire counts against a point; the full weight applies at the reposition radius.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", UIMin="0.0", UIMax="3.0"))
	float M_CombatCoverTravelWeight = 0.4f;

	// A soldier already in cover only looks this far for a better point.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="100.0", UIMin="100.0", UIMax="2500.0", Units="cm"))
	float M_CombatCoverRepositionRadius = 900.0f;

	// A better point must beat the occupied one by this much score before the soldier leaves. Higher values
	// keep soldiers in place; lower values make them hop between similar points.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float M_CombatCoverMinimumScoreGain = 0.3f;

	// A soldier stays at least this long on a point before it may move to a better one.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0", Units="s"))
	float M_CombatCoverMinimumHoldSeconds = 6.0f;

	// How often a soldier in cover compares its point with the others around it.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.5", UIMin="0.5", UIMax="15.0", Units="s"))
	float M_CombatCoverReevaluationSeconds = 3.0f;

	// How long a soldier stays hidden in cover that it cannot engage its current target from, before it gives
	// the cover up. Weapons change target often and targets move; without this a soldier leaves cover for the
	// open on every change. Zero leaves at once. A squad that was ordered to attack a specific enemy never
	// waits: a soldier that cannot engage that enemy from its cover leaves immediately.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", ClampMax="15.0", UIMin="0.0", UIMax="10.0", Units="s"))
	float M_CoverTargetChangeToleranceSeconds = 2.5f;

	// How long a soldier keeps a cover point that the latest cover scan did not find again. A scan misses a
	// point for a moment when another soldier walks through it; without this the occupant gets up and walks back
	// in a few seconds later. Cover whose object was destroyed is always given up at once.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", ClampMax="60.0", UIMin="0.0", UIMax="30.0", Units="s"))
	float M_CoverPointLossToleranceSeconds = 12.0f;

	// How long a soldier that stepped out of standing cover holds its firing position, without firing, after its
	// weapon lost its target or swapped to an enemy this point cannot reach. Weapons change target all the time;
	// without this the soldier steps back and out again on every change. Zero steps back at once.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", ClampMax="10.0", UIMin="0.0", UIMax="6.0", Units="s"))
	float M_StandingCoverTargetLossHoldSeconds = 2.0f;

	// Shortest time a soldier stays behind standing cover after stepping back before it steps out again.
	// An attack order from the player is never delayed by this.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="0.0", ClampMax="10.0", UIMin="0.0", UIMax="6.0", Units="s"))
	float M_StandingCoverMinimumHiddenSeconds = 1.0f;

	// Enemies taken into account per soldier: its own target first, then the targets of its squad mates.
	UPROPERTY(Config, EditAnywhere, Category="Combat Cover", meta=(ClampMin="1", ClampMax="8", UIMin="1", UIMax="8"))
	int32 M_CombatCoverMaximumThreats = 6;

	// Finds cover a soldier lies behind: bumps in the landscape and low objects that reach knee height (30 cm)
	// but are too low to crouch behind. Costs one extra probe only where something low was hit.
	UPROPERTY(Config, EditAnywhere, Category="Prone Cover")
	bool bM_FindProneCover = true;

	// Prone cover must top out below this height, or the lying soldier cannot fire over it.
	UPROPERTY(Config, EditAnywhere, Category="Prone Cover", meta=(ClampMin="35.0", ClampMax="85.0", UIMin="35.0", UIMax="85.0", Units="cm", EditCondition="bM_FindProneCover"))
	float M_ProneCoverMaximumHeight = 60.0f;

	// How steep the face of a bump must be to count. Lower values also accept gentle rises in the ground.
	UPROPERTY(Config, EditAnywhere, Category="Prone Cover", meta=(ClampMin="10.0", ClampMax="80.0", UIMin="10.0", UIMax="80.0", Units="deg", EditCondition="bM_FindProneCover"))
	float M_ProneCoverMinimumSlopeDegrees = 25.0f;

	// Distance from the face of the cover to the middle of the lying soldier. Match it to the prone animation,
	// so the soldier's head ends up just behind the cover.
	UPROPERTY(Config, EditAnywhere, Category="Prone Cover", meta=(ClampMin="40.0", ClampMax="160.0", UIMin="40.0", UIMax="160.0", Units="cm", EditCondition="bM_FindProneCover"))
	float M_ProneCoverStandOff = 90.0f;

	// Prone points keep at least this distance from each other. Rolling ground would otherwise fill up with
	// them; raise it for fewer prone points, lower it for more.
	UPROPERTY(Config, EditAnywhere, Category="Prone Cover", meta=(ClampMin="90.0", ClampMax="3000.0", UIMin="90.0", UIMax="1500.0", Units="cm", EditCondition="bM_FindProneCover"))
	float M_ProneCoverPointSpacing = 500.0f;

	// Chance that a crouch or standing cover point gets a prone point beside it, facing roughly the same way,
	// so a squad in cover does not all take the same pose. Zero adds none.
	UPROPERTY(Config, EditAnywhere, Category="Prone Cover", meta=(ClampMin="0", ClampMax="100", UIMin="0", UIMax="100", Units="%", EditCondition="bM_FindProneCover"))
	int32 M_ProneCompanionChancePercent = 15;

	// How far to the side of that crouch or standing point the prone point is placed.
	UPROPERTY(Config, EditAnywhere, Category="Prone Cover", meta=(ClampMin="80.0", ClampMax="400.0", UIMin="80.0", UIMax="400.0", Units="cm", EditCondition="bM_FindProneCover"))
	float M_ProneCompanionOffset = 170.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="40.0", ClampMax="143.0", UIMin="40.0", UIMax="143.0", Units="cm"))
	float M_MinimumCrouchCoverHeight = 90.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="60.0", UIMin="60.0", UIMax="300.0", Units="cm"))
	float M_MaximumCoverSearchDistance = 175.0f;

	// Minimum edge-to-edge opening that must fit the full standing capsule before a side peek is published.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="80.0", UIMin="80.0", UIMax="250.0", Units="cm"))
	float M_StandingPeekGapWidth = 110.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="50.0", UIMin="50.0", UIMax="250.0", Units="cm"))
	float M_CoverPointSpacing = 90.0f;

	// A position only becomes cover when a soldier's body fits there. The body is tested as a column of this
	// radius, slimmer than the navigation capsule, so a branch or ledge that merely brushes past does not count.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="10.0", ClampMax="60.0", UIMin="10.0", UIMax="60.0", Units="cm"))
	float M_StandingSpaceRadius = 25.0f;

	// Height of the top of that column above the ground. Kept below head height so branches overhead do not reject the ground under a tree;
	// raise it if soldiers end up standing with their heads inside low geometry.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="60.0", ClampMax="200.0", UIMin="60.0", UIMax="200.0", Units="cm"))
	float M_StandingSpaceHeight = 150.0f;

	// The column starts this far above the ground, so roots, rubble and kerbs that a soldier simply stands in
	// or steps over do not reject a position. Dead trees, for example, flare out up to 60 cm high at their foot.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="0.0", ClampMax="90.0", UIMin="0.0", UIMax="90.0", Units="cm"))
	float M_StandingSpaceFloorClearance = 60.0f;

	// Probes thin objects such as tree trunks and posts from a ring of positions around them, aimed at their
	// centre. The regular grid only finds such an object when a grid position happens to line up with it.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification")
	bool bM_ProbeThinObstacles = true;

	// Objects no wider than this where a soldier stands against them get the ring of probes. For a tree that
	// is the width of the trunk at crouch height, low branches and roots included, not the width of its crown.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="20.0", ClampMax="400.0", UIMin="20.0", UIMax="400.0", Units="cm", EditCondition="bM_ProbeThinObstacles"))
	float M_ThinObstacleMaximumWidth = 300.0f;

	// Probe positions around one thin object; more finds cover on more sides of it and costs more per scan.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="4", ClampMax="16", UIMin="4", UIMax="16", EditCondition="bM_ProbeThinObstacles"))
	int32 M_ThinObstacleRingSamples = 8;

	// A thin object has cover points on every side, but it only shelters one soldier per this much of its
	// width, and always at least one. A 20 cm pole takes one soldier, a 150 cm trunk two; the rest of the squad
	// looks for cover elsewhere. Lower values let more soldiers share one tree.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="20.0", ClampMax="400.0", UIMin="20.0", UIMax="400.0", Units="cm", EditCondition="bM_ProbeThinObstacles"))
	float M_ThinObstacleWidthPerSoldier = 70.0f;

	// Finds crouch cover behind thin objects that are a frame of beams instead of a solid surface, such as tank
	// hedgehogs. The regular crouch cover rule needs one continuous surface from knee to crouch height and is not
	// changed by this; the frame rule is only tried around a thin object where the regular rule found nothing.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(EditCondition="bM_ProbeThinObstacles"))
	bool bM_FindOpenFrameCover = true;

	// The most crouch points one such frame may get, spread evenly around it. A hedgehog is probed from every
	// side; without a limit each side that works becomes a point.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="1", ClampMax="16", UIMin="1", UIMax="8", EditCondition="bM_FindOpenFrameCover"))
	int32 M_OpenFramePointsPerObstacle = 3;

	// How far from the nearest beam of such a frame the soldier crouches.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="30.0", ClampMax="250.0", UIMin="30.0", UIMax="250.0", Units="cm", EditCondition="bM_FindOpenFrameCover"))
	float M_OpenFrameCoverStandOff = 60.0f;

	// The cover found around a thin object is reused for this many scans before its ring is probed again, and
	// the objects take turns, so a forest costs a fraction of its probes per scan. An object that moved is always
	// probed again at once. Higher values are cheaper; a change next to a tree is noticed later.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="1", ClampMax="60", UIMin="1", UIMax="60", EditCondition="bM_ProbeThinObstacles"))
	int32 M_ThinObstacleRefreshScans = 6;

	// When a grid probe only grazes an object, probes it once more from straight in front of the spot it hit,
	// so round and diagonal surfaces are judged the same as walls that happen to line up with the grid.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification")
	bool bM_ReaimSlantedHits = true;

	// How far inside the end of a high wall a standing point is placed. Must be smaller than the sideways step of
	// the shortest expose animation, or the peeking soldier's muzzle stays behind the wall.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="0.0", ClampMax="80.0", UIMin="0.0", UIMax="80.0", Units="cm"))
	float M_StandingPeekEdgeInset = 25.0f;

	// Actors of these classes, and of classes derived from them, carry their cover on the sockets of their first
	// mesh that has sockets, instead of having it found by the scan: trenches, whose collision is a flat plane
	// so tanks can drive over them, and hand-placed cover such as sandbag walls. Each socket whose name contains
	// one of the name parts below becomes a cover point of that kind, with the socket's forward axis as the
	// direction the soldier faces. Read once when the map starts; the points of a destroyed actor go with it.
	UPROPERTY(Config, EditAnywhere, Category="Cover From Sockets")
	TArray<TSoftClassPtr<AActor>> M_SocketCoverActorClasses = {
		TSoftClassPtr<AActor>(FSoftObjectPath(
			TEXT("/Game/RTS_Survival/Blueprints/Environment/Trenches/BP_TrenchMaster.BP_TrenchMaster_C"))),
		TSoftClassPtr<AActor>(FSoftObjectPath(
			TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/BP_NaturalCover.BP_NaturalCover_C")))
	};

	// Parts of a socket name that mark a cover point of each kind; upper and lower case do not matter. When a
	// name contains more than one of them, the longest wins: "standing_right_2" is a right-peek point although
	// it also contains "standing". A part left empty finds nothing.
	UPROPERTY(Config, EditAnywhere, Category="Cover From Sockets")
	FString M_TrenchCoverSocketNamePart = TEXT("cargo");

	UPROPERTY(Config, EditAnywhere, Category="Cover From Sockets")
	FString M_CrouchCoverSocketNamePart = TEXT("crouch");

	UPROPERTY(Config, EditAnywhere, Category="Cover From Sockets")
	FString M_StandingLeftCoverSocketNamePart = TEXT("standing");

	UPROPERTY(Config, EditAnywhere, Category="Cover From Sockets")
	FString M_StandingRightCoverSocketNamePart = TEXT("standing_right");

	UPROPERTY(Config, EditAnywhere, Category="Cover From Sockets")
	FString M_ProneCoverSocketNamePart = TEXT("prone");

	UPROPERTY(Config, EditAnywhere, Category="Debug")
	bool bM_DrawDetectedCover = true;

	UPROPERTY(Config, EditAnywhere, Category="Debug", meta=(ClampMin="1.0", UIMin="1.0", UIMax="60.0", Units="s", EditCondition="bM_DrawDetectedCover"))
	float M_DebugDrawDurationSeconds = 10.0f;

	static const URTSCoverFinderDeveloperSettings* Get();
};
