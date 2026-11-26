// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/CameraTour/MissionCameraTourTypes.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "SovietCounterattackSettings.generated.h"

class ASovietCargoTrain;
class ULevelSequence;
class USoundBase;
class UStaticMesh;
class UW_MissionTimer;

/**
 * Everything of the counterattack that changes with the difficulty. All times are whole seconds.
 * Wave N (counting from 1) has a random unit count in [MinUnitsFirstWave, MaxUnitsFirstWave] + (N - 1) * UnitsAddedPerWave.
 */
USTRUCT(BlueprintType)
struct FSovietCounterattackDifficulty
{
	GENERATED_BODY()

	// "Prepare to defend": seconds from the mission start until the first wave.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0", UIMin = "0"))
	int32 SecondsUntilSovietsArrive = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", UIMin = "0"))
	int32 WaveCount = 4;

	// Seconds between two waves; the "Survive" timer runs WaveCount x this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "1", UIMin = "1"))
	int32 SecondsBetweenWaves = 90;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinUnitsFirstWave = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxUnitsFirstWave = 12;

	// Added to both bounds for every wave after the first (bounds 10-12 with 1: wave 2 is 11-13, wave 3 is 12-14).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	int32 UnitsAddedPerWave = 1;

	// The pool every spawn (centre, landing field, train) draws from; each unit picks one entry of the tanks and
	// squads together at random, so list an entry twice to make it more common.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units")
	TArray<ETankSubtype> TankTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units")
	TArray<ESquadSubtype> SquadTypes;
};

/** Where the three spawns are and how far each spawn's groups stay apart on the shared attack route. */
USTRUCT(BlueprintType)
struct FSovietCounterattackSpawnSettings
{
	GENERATED_BODY()

	FSovietCounterattackSpawnSettings();

	// Centre spawn: units spawn on a grid around this point and drive in to the attack route. On the intro map it is
	// at the north map edge (y 25130) between the rail line (x 20875) and where the aircraft fly in, so the units
	// come in from off-screen. z 0 = the ground under the point is traced at runtime.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Centre")
	FVector CenterSpawnLocation = FVector(15500.f, 23900.f, 0.f);

	// Train spawn: once the train stands at the station its units spawn on a grid around this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FVector StationSpawnLocation = FVector(19953.f, 15593.f, 943.f);

	// Every unit of a wave rides the train and unloads at the station: no centre or landing-field share. Needs a train
	// (Train.TrainClass and two path points); without one the even split applies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	bool bAllUnitsByTrain = false;

	// Landing field: one Pe-8 lands on each pad centre (the pads cannot be destroyed). A batch flies one unit per
	// pad; the next batch comes once every aircraft of the batch has delivered and left.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing Field")
	TArray<FVector> LandingPadCenters;

	// Distance between two units of a ground spawn grid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "300.0"))
	float SpawnSpacing = 800.f;

	// Added (X, Y) to every attack route point for that spawn's groups, so groups do not all drive to one point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Offsets")
	FVector2D CenterRouteOffset = FVector2D(0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Offsets")
	FVector2D TrainRouteOffset = FVector2D(700.f, 900.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Offsets")
	FVector2D AirDropRouteOffset = FVector2D(-700.f, -900.f);
};

/** The landed cargo plane drops of the landing field. */
USTRUCT(BlueprintType)
struct FSovietCounterattackAirDropSettings
{
	GENERATED_BODY()

	// Aircraft drop ability whose Pe-8 flies the units (copied from the global ability data asset).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	EGlobalAbility CargoPlaneAbility = EGlobalAbility::GA_SovietT34Drop;

	// Vehicles hang on ropes under the Pe-8 and are lowered onto the pad; squads come in a Pe-8 that lands.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	bool bRopeSlungVehicles = true;

	// The announcer warns of the first aircraft of every batch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	bool bWarnPlayer = true;

	// Seconds between the take-offs of the aircraft of one batch, so they do not arrive wingtip to wingtip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop", meta = (ClampMin = "0.0"))
	float StaggerSeconds = 1.5f;
};

/** The cargo train that brings the station spawn's units. */
USTRUCT(BlueprintType)
struct FSovietCounterattackTrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	TSubclassOf<ASovietCargoTrain> TrainClass;

	// World points of the train's rails: the north map edge first, the station last (REAR_Rail_Main down to the
	// junction, then REAR_Rail_Siding; Source/Tools/IntroMission/counterattack/train_path.json).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	TArray<FVector> PathPoints;

	// The locomotive stops at the path point closest to this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FVector StopFrontLocation = FVector::ZeroVector;

	// Static tank meshes on the flatcars, one per vehicle the train brings, picked in turn from this list.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	TArray<TObjectPtr<UStaticMesh>> CargoTankMeshes;

	// Seconds between the train stopping and the cargo meshes turning into units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train", meta = (ClampMin = "0.0"))
	float UnloadDelaySeconds = 1.f;

	// Seconds the empty train waits at the station before it drives back out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train", meta = (ClampMin = "0.0"))
	float DepartDelaySeconds = 6.f;
};

/** One resource payout of a mission reward. */
USTRUCT(BlueprintType)
struct FSovietCounterattackResourceReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	ERTSResourceType Resource = ERTSResourceType::Resource_Radixite;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward", meta = (ClampMin = "0"))
	int32 Amount = 0;
};

/**
 * The start of the prepare phase: the Dig In field manual, dig-in markers on the ridges and a short camera tour over
 * them. The "Prepare to defend" countdown starts once the tour is over.
 */
USTRUCT(BlueprintType)
struct FSovietCounterattackDigInIntroSettings
{
	GENERATED_BODY()

	FSovietCounterattackDigInIntroSettings();

	// Off: no takeover, markers or tour; the countdown starts at once.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dig In Intro")
	bool bEnabled = true;

	// Opens the Dig In field manual (clip of a tank digging in) before the tour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dig In Intro")
	bool bShowDigInTakeover = true;

	// World marker on every dig-in position (BP_Hint_DigIn: ghost sandbag wall, arrow and a text above it).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	TSoftClassPtr<AActor> MarkerClass;

	// Where the player should dig in; z = 0 traces the ground. The markers stay for the whole prepare phase.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	TArray<FVector> MarkerLocations;

	// Each marker turns to face this point (the direction the Soviets come from).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	FVector MarkerFacingTarget = FVector(8400.f, -1500.f, 296.f);

	// Yaw added after facing MarkerFacingTarget (fixes the marker mesh's own forward axis).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	float MarkerYawOffset = 0.f;

	// Text on the markers (their SetText event); empty keeps the Blueprint's own text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	FText MarkerText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	EMinimapIconType MarkerMiniMapIcon = EMinimapIconType::Objective;

	// Fog of war is lifted this far around every marker for the tour, the prepare countdown and
	// RevealSecondsAfterTour more (0 = no reveal).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (ClampMin = "0.0"))
	float RevealRadius = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (ClampMin = "0.0"))
	float RevealSecondsAfterTour = 15.f;

	// Camera tour over the ridges, played after the takeover closes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
	TArray<FMissionCameraTourShot> TourShots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
	FMissionCameraTourSettings TourSettings;

	// Optional announcer line over the tour; starts VoiceLineDelaySeconds after the tour starts (a takeover stops a
	// portrait that is already playing). None = no line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
	TObjectPtr<USoundBase> VoiceLine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
	ERTSPortraitTypes VoiceLinePortrait = ERTSPortraitTypes::GermanAnnouncer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour", meta = (ClampMin = "0.0"))
	float VoiceLineDelaySeconds = 0.8f;

	// ---- Positions held (prepare phase feedback) ----
	// A player vehicle that stands dug in within this 2D distance of a marker holds that position. Its marker then
	// drops its ghost wall and arrow, shows MarkerHeldText, and its minimap icon goes; both come back if it leaves.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positions", meta = (ClampMin = "100.0"))
	float PositionRadius = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positions")
	FText MarkerHeldText;

	// Added under the prepare objective's description; {Held} and {Total} are filled in.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positions")
	FText PositionsObjectiveLine;

	// Announcer line the first time a position is held (once per mission). None = no line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positions")
	TObjectPtr<USoundBase> FirstPositionHeldLine = nullptr;

	// Paid out when the waves start with every position held, together with AllPositionsHeldLine.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positions")
	TArray<FSovietCounterattackResourceReward> AllPositionsHeldReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positions")
	TObjectPtr<USoundBase> AllPositionsHeldLine = nullptr;
};

/**
 * Skippable cinematic when the first wave arrives: TrainShotSeconds on the cargo train rolling in, then a cut to the
 * landing field where an aircraft lowers its vehicle onto a pad. It runs as a mission cinematic takeover (hold Space to
 * skip, prompt bottom centre) while the game keeps running; the camera is a runtime camera the mission moves.
 * The first wave's train waits until the filmed aircraft is about TrainPreRollSeconds + TrainShotSeconds +
 * DropLeadSeconds from its pad, so the train is still approaching when the shot opens.
 */
USTRUCT(BlueprintType)
struct FSovietCounterattackArrivalCinematicSettings
{
	GENERATED_BODY()

	FSovietCounterattackArrivalCinematicSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arrival Cinematic")
	bool bEnabled = true;

	// Level sequence the takeover plays; only its clock is used (the mission sets its length), so it may be empty.
	// Voice lines on its audio tracks get subtitles from the cinematic overlay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arrival Cinematic")
	TSoftObjectPtr<ULevelSequence> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arrival Cinematic")
	bool bAllowSkipping = true;

	// ---- Train shot ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Shot", meta = (ClampMin = "0.0"))
	float TrainShotSeconds = 2.f;

	// Seconds the train rolls before the cinematic starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Shot", meta = (ClampMin = "0.0"))
	float TrainPreRollSeconds = 4.f;

	// The camera stands this far ahead of the front flatcar along the track, off to its right (seen in the
	// direction of travel) and above the rails, and looks down across the rails at the locomotive, so the map edge
	// the train comes from stays out of the picture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Shot")
	float TrainCameraAhead = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Shot")
	float TrainCameraSide = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Shot")
	float TrainCameraHeight = 1800.f;

	// The camera aims this far ahead of the front flatcar (at the locomotive).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Shot")
	float TrainLookAhead = 500.f;

	// ---- Drop shot ----
	// Seconds before the filmed aircraft reaches its pad at which the drop shot should open.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot", meta = (ClampMin = "0.0"))
	float DropLeadSeconds = 1.5f;

	// The camera looks along this world yaw at the pad from DropCameraDistance away and DropCameraHeight up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot")
	float DropCameraYaw = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot")
	float DropCameraDistance = 4200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot")
	float DropCameraHeight = 3000.f;

	// The camera looks this fraction of the way from the pad to the aircraft (0 = at the pad).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropLookAtAircraftWeight = 0.35f;

	// The look-at point never rises higher than this above the pad, so the camera keeps the ground (not the black
	// sky above the map) in the picture while the aircraft is still high.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot", meta = (ClampMin = "0.0"))
	float DropLookAtMaxHeight = 600.f;

	// The shot ends this long after the filmed unit is on the ground, or after DropShotMaxSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot", meta = (ClampMin = "0.0"))
	float SecondsAfterDelivery = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop Shot", meta = (ClampMin = "1.0"))
	float DropShotMaxSeconds = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "10.0", ClampMax = "120.0"))
	float FieldOfView = 60.f;

	// Game speed while the cinematic plays (1 = normal), so the player loses less game time to it. Restored after.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arrival Cinematic", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TimeDilation = 0.75f;

	// Fog of war is lifted this far around the train and the filmed pad for the cinematic.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float RevealRadius = 4500.f;

	// No aircraft of the first batch came near its pad within this many seconds: no cinematic, the train goes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arrival Cinematic", meta = (ClampMin = "1.0"))
	float MaxWaitSeconds = 120.f;
};

/**
 * Heavy tanks HQ flies into the player's base once the first wave's arrival cinematic is over (also when it was
 * skipped or never played): an announcer line and one cargo plane per vehicle (CargoPlaneAbility's aircraft) landing
 * around LandingPoint. The payload depends on the player's faction.
 */
USTRUCT(BlueprintType)
struct FSovietCounterattackBaseReinforcementSettings
{
	GENERATED_BODY()

	FSovietCounterattackBaseReinforcementSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	bool bEnabled = true;

	// Seconds after the arrival cinematic ended before the line plays and the aircraft take off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements", meta = (ClampMin = "0.0"))
	float DelayAfterCinematicSeconds = 1.5f;

	// Centre of the drop = the player HQ plus this XY offset (default: the open concrete west of the HQ, away from the
	// attack). The first aircraft lands there, the others 1000 apart around it; every point is projected onto the
	// navmesh, so buildings the player put there push the aircraft aside.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	FVector LandingOffsetFromHQ = FVector(-1200.f, 0.f, 0.f);

	// Used instead of the HQ when the player has none (z 0 = traced to the ground).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	FVector FallbackLandingPoint = FVector(250.f, 300.f, 0.f);

	// Any German aircraft drop ability: only its aircraft and landing behaviour are used, one aircraft per vehicle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	EGlobalAbility CargoPlaneAbility = EGlobalAbility::GA_GerPzIIIFlak41Drop;

	// Breakthrough Doctrine (and any faction that is not Strike Division).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	TArray<ETankSubtype> BreakthroughVehicles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	TArray<ETankSubtype> StrikeDivisionVehicles;

	// "Kommandant, HQ provides us with more heavy tanks..." Set on the data asset so the cook sees it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	TObjectPtr<USoundBase> VoiceLine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements")
	ERTSPortraitTypes VoiceLinePortrait = ERTSPortraitTypes::GermanAnnouncer;

	// The reinforcements ping on the minimap at the landing point for this long.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Reinforcements", meta = (ClampMin = "0.0"))
	float MiniMapPingSeconds = 8.f;
};

/**
 * @brief Data of USovietCounterattackMission: per difficulty the time until the Soviets arrive, the waves and their
 * unit pool; plus the three spawns, the attack route, the timer and the objective texts.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USovietCounterattackSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	USovietCounterattackSettings();

	const FSovietCounterattackDifficulty& GetDifficultySettings(const ERTSGameDifficulty Difficulty) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", DisplayName = "New To RTS")
	FSovietCounterattackDifficulty NewToRTS;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Hard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Brutal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Ironman;

	// Attack-move route of every group, each spawn with its own offset; the player HQ (at the time the group sets
	// off) is added as the last point.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	TArray<FVector> AttackRoute;

	// The station's groups take TrainAttackRoute instead of AttackRoute, so they do not detour over a route laid out
	// for the centre spawn. The HQ is still added as the last point; an empty TrainAttackRoute goes straight there.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	bool bTrainUsesOwnRoute = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (EditCondition = "bTrainUsesOwnRoute"))
	TArray<FVector> TrainAttackRoute;

	// Last route point when the player has no HQ.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	FVector FallbackHQLocation = FVector(1450.f, 300.f, 600.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "1"))
	int32 MaxFormationWidth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "0.1"))
	float FormationOffsetMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	FAttackMoveWaveSettings AttackMoveSettings;

	// Seconds between a group's last unit appearing and its attack-move order (fresh squads drop early orders).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "0.0"))
	float OrderDelaySeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawns")
	FSovietCounterattackSpawnSettings Spawns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawns")
	FSovietCounterattackAirDropSettings AirDrop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawns")
	FSovietCounterattackTrainSettings Train;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UW_MissionTimer> TimerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText PrepareTimerText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText SurviveTimerText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText PrepareObjectiveTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MultiLine = "true"))
	FText PrepareObjectiveDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText SurviveObjectiveTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MultiLine = "true"))
	FText SurviveObjectiveDescription;

	// Shown with the number of Soviet units still alive once the Survive timer ran out.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText RemainingObjectiveTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MultiLine = "true"))
	FText RemainingObjectiveDescription;

	// Incoming groups show as survival-style division markers on the minimap while the waves come.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bMarkAttackGroupsOnMiniMap = true;

	// Once the Survive timer ran out every remaining Soviet unit gets this icon on the minimap.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	EMinimapIconType RemainingUnitIcon = EMinimapIconType::AttackTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FSovietCounterattackDigInIntroSettings DigInIntro;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FSovietCounterattackArrivalCinematicSettings ArrivalCinematic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FSovietCounterattackBaseReinforcementSettings BaseReinforcements;
};
