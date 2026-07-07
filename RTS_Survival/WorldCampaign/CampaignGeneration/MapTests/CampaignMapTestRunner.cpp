// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CampaignMapTestRunner.h"

#if RTS_WITH_CAMPAIGN_MAP_TESTS

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogCampaignMapTest, Display, All);

namespace CampaignMapTest
{
	namespace
	{
		// Substring identifying the campaign map package (RTS_WorldCampaign at /Game/RTS_Survival/Maps/Campaign).
		// UWorld::GetMapName() returns the short package name (optionally with a PIE prefix), so a substring
		// match tolerates the prefix while staying specific to the campaign map.
		const TCHAR* const CampaignMapNameToken = TEXT("RTS_WorldCampaign");

		// Command-line overrides so an automated run can pick the seed range without a code change, e.g.
		// -RTSCampaignMapTestsFirstSeed=0 -RTSCampaignMapTestsSeedCount=100 -RTSCampaignMapTestsNoQuit
		const TCHAR* const FirstSeedArgument = TEXT("RTSCampaignMapTestsFirstSeed=");
		const TCHAR* const SeedCountArgument = TEXT("RTSCampaignMapTestsSeedCount=");
		const TCHAR* const NoQuitSwitch = TEXT("RTSCampaignMapTestsNoQuit");

		// Seconds to let the campaign map settle (normal BeginPlay generation, difficulty init) before the
		// unattended sweep takes over the generator.
		constexpr float AutoRunSettleSeconds = 3.0f;

		// Poll cadence for the unattended auto-run ticker.
		constexpr float AutoRunPollSeconds = 0.5f;

		FString EnemyItemName(const EMapEnemyItem Value)
		{
			if (const UEnum* EnumType = StaticEnum<EMapEnemyItem>())
			{
				return EnumType->GetNameStringByValue(static_cast<int64>(Value));
			}
			return FString::FromInt(static_cast<int32>(Value));
		}

		FString NeutralTypeName(const EMapNeutralObjectType Value)
		{
			if (const UEnum* EnumType = StaticEnum<EMapNeutralObjectType>())
			{
				return EnumType->GetNameStringByValue(static_cast<int64>(Value));
			}
			return FString::FromInt(static_cast<int32>(Value));
		}

		FString MissionName(const EMapMission Value)
		{
			if (const UEnum* EnumType = StaticEnum<EMapMission>())
			{
				return EnumType->GetNameStringByValue(static_cast<int64>(Value));
			}
			return FString::FromInt(static_cast<int32>(Value));
		}

		// Deterministic, hyphenated GUID string so anchor keys sort and diff identically across runs.
		FString KeyString(const FGuid& Key)
		{
			return Key.ToString(EGuidFormats::DigitsWithHyphens);
		}

		// Builds a canonical, sorted, whitespace-stable dump of a single generation so two runs of the same
		// seed produce byte-identical files (barring genuine non-determinism). Also returns a CRC fingerprint
		// over the whole dump for at-a-glance comparison in the summary table.
		FString BuildSeedPlacementReport(
			const int32 Seed,
			const FWorldCampaignPlacementState& State,
			const bool bSuccess,
			const FString& FailureReason,
			uint32& OutFingerprint)
		{
			TMap<FGuid, FVector> LocationByKey;
			LocationByKey.Reserve(State.CachedAnchors.Num());
			for (const TObjectPtr<AAnchorPoint>& Anchor : State.CachedAnchors)
			{
				if (not IsValid(Anchor))
				{
					continue;
				}
				LocationByKey.Add(Anchor->GetAnchorKey(), Anchor->GetActorLocation());
			}

			const auto LocationString = [&LocationByKey](const FGuid& Key) -> FString
			{
				if (const FVector* Location = LocationByKey.Find(Key))
				{
					return FString::Printf(TEXT("%d,%d,%d"),
					                       FMath::RoundToInt32(Location->X),
					                       FMath::RoundToInt32(Location->Y),
					                       FMath::RoundToInt32(Location->Z));
				}
				return TEXT("<no-anchor>");
			};

			// Sort every collection by GUID string so output ordering is independent of container iteration.
			const auto SortKeys = [](TArray<FGuid>& Keys)
			{
				Keys.Sort([](const FGuid& Left, const FGuid& Right)
				{
					return KeyString(Left) < KeyString(Right);
				});
			};

			TArray<FString> Lines;
			Lines.Add(FString::Printf(TEXT("SEED %d"), Seed));
			Lines.Add(FString::Printf(TEXT("STATE_SEED %d"), State.SeedUsed));
			Lines.Add(FString::Printf(TEXT("RESULT %s"), bSuccess ? TEXT("SUCCESS") : TEXT("FAILED")));
			if (not bSuccess)
			{
				Lines.Add(FString::Printf(TEXT("FAILURE_REASON %s"), *FailureReason));
			}
			Lines.Add(FString::Printf(TEXT("ANCHORS %d"), State.CachedAnchors.Num()));
			Lines.Add(FString::Printf(TEXT("CONNECTIONS %d"), State.CachedConnections.Num()));
			Lines.Add(FString::Printf(TEXT("PLAYER_HQ key=%s loc=%s"),
			                          *KeyString(State.PlayerHQAnchorKey),
			                          *LocationString(State.PlayerHQAnchorKey)));
			Lines.Add(FString::Printf(TEXT("ENEMY_HQ key=%s loc=%s"),
			                          *KeyString(State.EnemyHQAnchorKey),
			                          *LocationString(State.EnemyHQAnchorKey)));

			Lines.Add(FString::Printf(TEXT("ENEMY_ITEMS %d"), State.EnemyItemsByAnchorKey.Num()));
			{
				TArray<FGuid> Keys;
				State.EnemyItemsByAnchorKey.GetKeys(Keys);
				SortKeys(Keys);
				for (const FGuid& Key : Keys)
				{
					Lines.Add(FString::Printf(TEXT("ENEMY key=%s loc=%s type=%s"),
					                          *KeyString(Key),
					                          *LocationString(Key),
					                          *EnemyItemName(State.EnemyItemsByAnchorKey.FindChecked(Key))));
				}
			}

			Lines.Add(FString::Printf(TEXT("NEUTRAL_ITEMS %d"), State.NeutralItemsByAnchorKey.Num()));
			{
				TArray<FGuid> Keys;
				State.NeutralItemsByAnchorKey.GetKeys(Keys);
				SortKeys(Keys);
				for (const FGuid& Key : Keys)
				{
					Lines.Add(FString::Printf(TEXT("NEUTRAL key=%s loc=%s type=%s"),
					                          *KeyString(Key),
					                          *LocationString(Key),
					                          *NeutralTypeName(State.NeutralItemsByAnchorKey.FindChecked(Key))));
				}
			}

			Lines.Add(FString::Printf(TEXT("MISSIONS %d"), State.MissionsByAnchorKey.Num()));
			{
				TArray<FGuid> Keys;
				State.MissionsByAnchorKey.GetKeys(Keys);
				SortKeys(Keys);
				for (const FGuid& Key : Keys)
				{
					Lines.Add(FString::Printf(TEXT("MISSION key=%s loc=%s type=%s"),
					                          *KeyString(Key),
					                          *LocationString(Key),
					                          *MissionName(State.MissionsByAnchorKey.FindChecked(Key))));
				}
			}

			// Every anchor (including pruned-through empty routing anchors that survived) for a full graph diff.
			Lines.Add(FString::Printf(TEXT("ALL_ANCHORS %d"), LocationByKey.Num()));
			{
				TArray<FGuid> Keys;
				LocationByKey.GetKeys(Keys);
				SortKeys(Keys);
				for (const FGuid& Key : Keys)
				{
					Lines.Add(FString::Printf(TEXT("ANCHOR key=%s loc=%s"), *KeyString(Key), *LocationString(Key)));
				}
			}

			const FString Canonical = FString::Join(Lines, TEXT("\n"));
			OutFingerprint = FCrc::StrCrc32(*Canonical);
			return FString::Printf(TEXT("FINGERPRINT %08X\n%s\n"), OutFingerprint, *Canonical);
		}

		// Ensures the campaign generator has runtime settings (difficulty/seed baseline) even if the normal
		// BeginPlay flow has not run yet, mirroring RTS.WorldCampaign.ValidatePruning.
		void EnsureGeneratorInitialized(UWorld* World, AGeneratorWorldCampaign* Generator)
		{
			if (not IsValid(World) || not IsValid(Generator))
			{
				return;
			}

			AWorldPlayerController* WorldPlayerController = nullptr;
			for (TActorIterator<AWorldPlayerController> It(World); It; ++It)
			{
				WorldPlayerController = *It;
				break;
			}

			URTSGameInstance* GameInstance = World->GetGameInstance<URTSGameInstance>();
			if (not IsValid(WorldPlayerController) || not IsValid(GameInstance))
			{
				return;
			}

			Generator->InitializeWorldGenerator(
				WorldPlayerController,
				GameInstance->GetCampaignGenerationSettings(),
				GameInstance->GetSelectedGameDifficulty());
		}

		AGeneratorWorldCampaign* FindCampaignGenerator(UWorld* World)
		{
			if (not IsValid(World))
			{
				return nullptr;
			}

			for (TActorIterator<AGeneratorWorldCampaign> It(World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}

		bool WorldIsLoadedCampaignMap(const UWorld* World)
		{
			if (not IsValid(World))
			{
				return false;
			}
			if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
			{
				return false;
			}
			return World->GetMapName().Contains(CampaignMapNameToken);
		}
	}

	void RunCampaignMapTestSweep(UWorld* World, const int32 FirstSeed, const int32 SeedCount, const bool bQuitWhenDone)
	{
		if (SeedCount <= 0)
		{
			UE_LOG(LogCampaignMapTest, Error, TEXT("Campaign map test sweep aborted: seed count must be > 0 (got %d)."),
			       SeedCount);
			return;
		}

		AGeneratorWorldCampaign* Generator = FindCampaignGenerator(World);
		if (not IsValid(Generator))
		{
			UE_LOG(LogCampaignMapTest, Error,
			       TEXT("Campaign map test sweep aborted: no AGeneratorWorldCampaign found in the loaded world. ")
			       TEXT("Ensure the campaign map (%s) is loaded."), CampaignMapNameToken);
			return;
		}

		EnsureGeneratorInitialized(World, Generator);

		const FString RunStamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		const FString RunDirectory = FPaths::ProjectSavedDir() / TEXT("CampaignMapTests") / (TEXT("Run_") + RunStamp);
		IFileManager::Get().MakeDirectory(*RunDirectory, /*Tree*/ true);

		UE_LOG(LogCampaignMapTest, Display,
		       TEXT("Campaign map determinism sweep: seeds [%d..%d], writing to %s"),
		       FirstSeed, FirstSeed + SeedCount - 1, *RunDirectory);

		TArray<FString> SummaryRows;
		TArray<int32> FailedSeeds;
		int32 SuccessCount = 0;

		SummaryRows.Add(FString::Printf(
			TEXT("%-9s %-6s %-6s %-6s %-6s %-6s %-6s %-10s %s"),
			TEXT("Seed"), TEXT("Result"), TEXT("Anch"), TEXT("Conn"),
			TEXT("Enemy"), TEXT("Neut"), TEXT("Miss"), TEXT("FingerPr"), TEXT("Note")));

		for (int32 SeedIndex = 0; SeedIndex < SeedCount; SeedIndex++)
		{
			const int32 Seed = FirstSeed + SeedIndex;

			FString FailureReason;
			const bool bSuccess = Generator->GenerateAndValidatePrunedWorldForSeed(Seed, FailureReason);
			if (bSuccess)
			{
				SuccessCount++;
			}
			else
			{
				FailedSeeds.Add(Seed);
			}

			const FWorldCampaignPlacementState& State = Generator->GetPlacementState();

			uint32 Fingerprint = 0;
			const FString SeedReport = BuildSeedPlacementReport(Seed, State, bSuccess, FailureReason, Fingerprint);

			const FString SeedFilePath = RunDirectory / FString::Printf(TEXT("Seed_%06d.log"), Seed);
			if (not FFileHelper::SaveStringToFile(SeedReport, *SeedFilePath))
			{
				UE_LOG(LogCampaignMapTest, Error, TEXT("Failed to write per-seed report: %s"), *SeedFilePath);
			}

			SummaryRows.Add(FString::Printf(
				TEXT("%-9d %-6s %-6d %-6d %-6d %-6d %-6d %08X   %s"),
				Seed,
				bSuccess ? TEXT("OK") : TEXT("FAIL"),
				State.CachedAnchors.Num(),
				State.CachedConnections.Num(),
				State.EnemyItemsByAnchorKey.Num(),
				State.NeutralItemsByAnchorKey.Num(),
				State.MissionsByAnchorKey.Num(),
				Fingerprint,
				bSuccess ? TEXT("") : *FailureReason));

			UE_LOG(LogCampaignMapTest, Display,
			       TEXT("Seed %d (%d/%d): %s | anchors=%d connections=%d enemies=%d neutrals=%d missions=%d fp=%08X"),
			       Seed, SeedIndex + 1, SeedCount,
			       bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
			       State.CachedAnchors.Num(), State.CachedConnections.Num(),
			       State.EnemyItemsByAnchorKey.Num(), State.NeutralItemsByAnchorKey.Num(),
			       State.MissionsByAnchorKey.Num(), Fingerprint);
		}

		// Leave the generator clean so the map is not left displaying the last test generation.
		Generator->EraseAllGeneration();

		TArray<FString> SummaryLines;
		SummaryLines.Add(TEXT("Campaign Map Determinism Test - Summary"));
		SummaryLines.Add(FString::Printf(TEXT("Run: %s"), *RunStamp));
		SummaryLines.Add(FString::Printf(TEXT("Seed range: %d..%d (%d seeds)"),
		                                 FirstSeed, FirstSeed + SeedCount - 1, SeedCount));
		SummaryLines.Add(FString::Printf(TEXT("Succeeded: %d    Failed: %d"),
		                                 SuccessCount, SeedCount - SuccessCount));
		if (FailedSeeds.Num() > 0)
		{
			TArray<FString> FailedSeedStrings;
			for (const int32 FailedSeed : FailedSeeds)
			{
				FailedSeedStrings.Add(FString::FromInt(FailedSeed));
			}
			SummaryLines.Add(FString::Printf(TEXT("Failed seeds: %s"), *FString::Join(FailedSeedStrings, TEXT(", "))));
		}
		SummaryLines.Add(TEXT(""));
		SummaryLines.Add(TEXT("Per-seed placement (fingerprint = CRC32 of the canonical Seed_XXXXXX.log; "));
		SummaryLines.Add(TEXT("identical seed -> identical fingerprint across runs means determinism held):"));
		SummaryLines.Add(TEXT(""));
		SummaryLines.Append(SummaryRows);

		const FString SummaryPath = RunDirectory / TEXT("Summary.txt");
		if (not FFileHelper::SaveStringToFile(FString::Join(SummaryLines, TEXT("\n")) + TEXT("\n"), *SummaryPath))
		{
			UE_LOG(LogCampaignMapTest, Error, TEXT("Failed to write summary: %s"), *SummaryPath);
		}

		UE_LOG(LogCampaignMapTest, Display,
		       TEXT("Campaign map determinism sweep complete: %d/%d succeeded. Summary: %s"),
		       SuccessCount, SeedCount, *SummaryPath);

		if (bQuitWhenDone)
		{
			UE_LOG(LogCampaignMapTest, Display, TEXT("Requesting exit after campaign map test sweep."));
			FPlatformMisc::RequestExit(/*Force*/ false);
		}
	}

	namespace
	{
		// ---- Manual trigger: console command ----------------------------------------------------------
		// RTS.WorldCampaign.RunMapTests [FirstSeed] [SeedCount] [Quit]
		void RunCampaignMapTestsCommand(const TArray<FString>& Args, UWorld* World)
		{
			int32 FirstSeed = DefaultFirstSeed;
			int32 SeedCount = DefaultSeedCount;
			bool bQuit = false;

			if (Args.Num() >= 1)
			{
				FirstSeed = FCString::Atoi(*Args[0]);
			}
			if (Args.Num() >= 2)
			{
				SeedCount = FCString::Atoi(*Args[1]);
			}
			for (const FString& Arg : Args)
			{
				FString Normalized = Arg;
				Normalized.ReplaceInline(TEXT(";"), TEXT(""));
				if (Normalized.Equals(TEXT("Quit"), ESearchCase::IgnoreCase)
					|| Normalized.Equals(TEXT("Exit"), ESearchCase::IgnoreCase))
				{
					bQuit = true;
				}
			}

			RunCampaignMapTestSweep(World, FirstSeed, SeedCount, bQuit);
		}

		FAutoConsoleCommandWithWorldAndArgs GRunCampaignMapTestsCommand(
			TEXT("RTS.WorldCampaign.RunMapTests"),
			TEXT("Runs the campaign backtracking generator across a seed range, writing a per-seed determinism ")
			TEXT("log and a summary under Saved/CampaignMapTests. Args: [FirstSeed] [SeedCount] [Quit]."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCampaignMapTestsCommand));

		// ---- Unattended trigger: auto-run once the campaign map is loaded -----------------------------
		bool GHasRunAutoSweep = false;
		float GCampaignWorldSettleSeconds = -1.f;

		UWorld* FindLoadedCampaignGameWorld()
		{
			if (GEngine == nullptr)
			{
				return nullptr;
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (WorldIsLoadedCampaignMap(World))
				{
					return World;
				}
			}
			return nullptr;
		}

		int32 ResolveCommandLineFirstSeed()
		{
			int32 Value = DefaultFirstSeed;
			FParse::Value(FCommandLine::Get(), FirstSeedArgument, Value);
			return Value;
		}

		int32 ResolveCommandLineSeedCount()
		{
			int32 Value = DefaultSeedCount;
			FParse::Value(FCommandLine::Get(), SeedCountArgument, Value);
			return Value;
		}

		bool ResolveShouldQuitAfterAutoRun()
		{
			// Unattended runs quit by default so headless/CI invocations terminate; -RTSCampaignMapTestsNoQuit
			// keeps the process alive for interactive inspection.
			return not FParse::Param(FCommandLine::Get(), NoQuitSwitch);
		}

		bool AutoRunTick(float DeltaTime)
		{
			if (GHasRunAutoSweep)
			{
				return false; // Stop ticking.
			}

			UWorld* CampaignWorld = FindLoadedCampaignGameWorld();
			if (CampaignWorld == nullptr)
			{
				GCampaignWorldSettleSeconds = -1.f;
				return true;
			}

			GCampaignWorldSettleSeconds = (GCampaignWorldSettleSeconds < 0.f)
				                              ? 0.f
				                              : GCampaignWorldSettleSeconds + DeltaTime;
			if (GCampaignWorldSettleSeconds < AutoRunSettleSeconds)
			{
				return true;
			}

			if (not IsValid(FindCampaignGenerator(CampaignWorld)))
			{
				return true; // Map loaded but generator not spawned yet; keep waiting.
			}

			GHasRunAutoSweep = true;
			UE_LOG(LogCampaignMapTest, Display,
			       TEXT("Campaign map detected and settled; starting unattended determinism sweep."));
			RunCampaignMapTestSweep(
				CampaignWorld,
				ResolveCommandLineFirstSeed(),
				ResolveCommandLineSeedCount(),
				ResolveShouldQuitAfterAutoRun());
			return false;
		}

		FDelayedAutoRegisterHelper GAutoRunRegister(
			EDelayedRegisterRunPhase::EndOfEngineInit,
			[]()
			{
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(&AutoRunTick), AutoRunPollSeconds);
			});
	}
}

#endif // RTS_WITH_CAMPAIGN_MAP_TESTS
