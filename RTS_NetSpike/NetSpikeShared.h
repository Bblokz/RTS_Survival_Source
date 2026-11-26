// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogRTSNetSpike, Log, All);

/**
 * Shared values of the LAN multiplayer spike. Every log line the launch script reads starts with RTS_NETSPIKE and
 * names the side (host or client) and the rung reached, so a run that stops early shows where it stopped.
 */
namespace NetSpike
{
	const TCHAR* const HostSide = TEXT("host");
	const TCHAR* const ClientSide = TEXT("client");

	// Unit-test map of the headless suites; the arena is the flat field the vehicle duel bench fights on.
	const TCHAR* const MapPath = TEXT("/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall");
	const TCHAR* const MapShortName = TEXT("RTS_ToCopySmall");
	const TCHAR* const TankClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/"
			"Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const FVector ArenaLocation(16796.f, -15808.f, 0.f);
	// The tank drops this far onto the ground when it spawns, like the skirmish harness does.
	constexpr float TankSpawnLiftCm = 150.f;
	constexpr float GroundTraceUpCm = 20000.f;
	constexpr float GroundTraceDownCm = 40000.f;
	// The client orders the tank this far from where it stands. Diagonal to the tank's facing, so the drive
	// starts with a turn and rotation is replicated as well as position.
	const FVector MoveOffset(1800.f, 1800.f, 0.f);

	// The HUD and the start-game gate need this long before the match can be started from code.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double TankSettleSeconds = 5.0;
	// The client watches the tank stand still this long before it sends the order.
	constexpr double ClientStillSeconds = 3.0;
	constexpr double ClientViewReportIntervalSeconds = 0.25;
	constexpr double ClientRestCheckIntervalSeconds = 2.0;
	constexpr double ClientMaxObserveSeconds = 50.0;
	constexpr double SampleLogIntervalSeconds = 1.0;
	// Wall-clock limits: a process that passes its limit reports a failure and exits, so it never holds the DLL.
	constexpr double HostDeadlineSeconds = 300.0;
	constexpr double ClientDeadlineSeconds = 240.0;
	constexpr float ExitDelaySeconds = 2.f;
	constexpr float ClientExitDelaySeconds = 3.f;
	constexpr int32 TicksBeforeTravel = 3;

	// A standing tank may settle by less than this.
	constexpr float StillToleranceCm = 50.f;
	constexpr float MovementSeenCm = 200.f;
	constexpr float MinTravelCm = 1000.f;
	// The client's resting tank must end this close to the server's.
	constexpr float FinalToleranceCm = 200.f;
	constexpr float RestToleranceCm = 5.f;
	// The tank must turn at least this far on the way, on the host and in the client's view.
	constexpr float MinTurnDegrees = 20.f;
	// The client's resting tank must face this close to the server's (replicated yaw comes in 1.4 degree steps).
	constexpr float FinalYawToleranceDegrees = 5.f;
	// Above this speed the server's tank counts as driving for the position error figures.
	constexpr float MovingSpeedCmPerSecond = 20.f;

	constexpr int32 ExitCodePass = 0;
	constexpr int32 ExitCodeFail = 1;

	/** @return The world of the running game, or null between maps. */
	UWorld* GetGameWorld();

	/** @brief Logs one milestone line: "RTS_NETSPIKE <side> rung=<n> <text>". */
	void LogRung(const TCHAR* Side, int32 Rung, const FString& Text);

	/** @brief Client only: its test steps are over, so the host closing the connection is no failure any more. */
	void MarkClientWorkDone();
	bool GetIsClientWorkDone();

	/** @brief Ends the process after a delay, so the last log lines and reliable calls still go out. */
	void RequestProcessExit(int32 Status, float DelaySeconds);
}
