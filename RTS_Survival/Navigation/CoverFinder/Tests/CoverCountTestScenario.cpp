#include "CoverCountTestScenario.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverCountTest, Log, All);

namespace CoverCountTestScenarioPrivate
{
	const TCHAR* const BaselineDirectory = TEXT("RTS_Survival/Navigation/CoverFinder/Tests/Baselines");
	const TCHAR* const TotalSourceName = TEXT("total");
	const TCHAR* const KeySeparator = TEXT("/");
	const TCHAR* const BaselineValueSeparator = TEXT("=");
	// Some objects on the test map are placed with a random rotation, so counts move between runs: a little for
	// the totals per cover type, a lot for the handful of points around one class of tree.
	constexpr float DefaultTotalTolerance = 0.8f;
	constexpr float ObjectClassTolerance = 0.5f;
	// Lets a class with only a handful of points lose one or two to that randomness without failing.
	constexpr int32 AbsoluteSlack = 2;

	const TCHAR* GetCoverTypeName(const ERTSCoverType CoverType)
	{
		const UEnum* CoverTypeEnum = StaticEnum<ERTSCoverType>();
		static FString NameBuffer;
		NameBuffer = IsValid(CoverTypeEnum)
			? CoverTypeEnum->GetNameStringByValue(static_cast<int64>(CoverType))
			: FString(TEXT("Unknown"));
		return *NameBuffer;
	}

	FString GetSourceName(const URTSCoverFinderWorldSubsystem& CoverSubsystem, const FRTSCoverPoint& CoverPoint)
	{
		const AActor* ProviderActor = CoverSubsystem.ResolveBlockingProvider(CoverPoint);
		FString ClassName = IsValid(ProviderActor) ? ProviderActor->GetClass()->GetName() : FString();
		ClassName.RemoveFromEnd(TEXT("_C"));
		if (CoverPoint.ProviderRegistrationId != 0)
		{
			return ClassName.IsEmpty() ? FString(TEXT("Authored")) : TEXT("Authored:") + ClassName;
		}
		return ClassName.IsEmpty() ? FString(TEXT("Landscape")) : ClassName;
	}
}

void FCoverCountTestScenario::Start(const bool bWriteBaseline)
{
	bM_IsWaitingForCoverScan = true;
	bM_WriteBaseline = bWriteBaseline;
	UE_LOG(LogRTSCoverCountTest, Display, TEXT("RTS_COVER_COUNT_TEST scheduled"));
}

void FCoverCountTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	const UWorld* World = CoverSubsystem.GetWorld();
	if (not bM_IsWaitingForCoverScan || not IsValid(World) || not CoverSubsystem.GetHasCompletedFullScan())
	{
		return;
	}
	bM_IsWaitingForCoverScan = false;
	const TMap<FString, int32> Counts = GatherCounts(CoverSubsystem);
	const FString BaselineFilePath = GetBaselineFilePath(World->GetName());
	if (bM_WriteBaseline)
	{
		WriteBaseline(BaselineFilePath, Counts);
		return;
	}
	TMap<FString, int32> Baseline;
	if (not TryLoadBaseline(BaselineFilePath, Baseline))
	{
		UE_LOG(
			LogRTSCoverCountTest,
			Display,
			TEXT("RTS_COVER_COUNT_TEST RESULT FAIL no baseline at %s; run once with -CoverFinderWriteCountBaseline"),
			*BaselineFilePath);
		return;
	}
	const int32 LostKeyCount = CompareWithBaseline(Counts, Baseline);
	UE_LOG(
		LogRTSCoverCountTest,
		Display,
		TEXT("RTS_COVER_COUNT_TEST RESULT %s points=%d keys_below_baseline=%d map=%s"),
		LostKeyCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
		CoverSubsystem.GetCoverPointsView().Num(),
		LostKeyCount,
		*World->GetName());
}

TMap<FString, int32> FCoverCountTestScenario::GatherCounts(const URTSCoverFinderWorldSubsystem& CoverSubsystem) const
{
	using namespace CoverCountTestScenarioPrivate;
	TMap<FString, int32> Counts;
	for (const FRTSCoverPoint& CoverPoint : CoverSubsystem.GetCoverPointsView())
	{
		const FString TypeName = GetCoverTypeName(CoverPoint.CoverType);
		++Counts.FindOrAdd(GetSourceName(CoverSubsystem, CoverPoint) + KeySeparator + TypeName);
		++Counts.FindOrAdd(FString(TotalSourceName) + KeySeparator + TypeName);
	}
	Counts.KeySort(TLess<FString>());
	return Counts;
}

FString FCoverCountTestScenario::GetBaselineFilePath(const FString& MapName) const
{
	return FPaths::Combine(
		FPaths::GameSourceDir(),
		CoverCountTestScenarioPrivate::BaselineDirectory,
		MapName + TEXT(".txt"));
}

bool FCoverCountTestScenario::TryLoadBaseline(const FString& FilePath, TMap<FString, int32>& OutBaseline) const
{
	TArray<FString> Lines;
	if (not FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		return false;
	}
	for (const FString& Line : Lines)
	{
		FString Key;
		FString Value;
		if (Line.Split(CoverCountTestScenarioPrivate::BaselineValueSeparator, &Key, &Value))
		{
			OutBaseline.Add(Key.TrimStartAndEnd(), FCString::Atoi(*Value));
		}
	}
	return not OutBaseline.IsEmpty();
}

void FCoverCountTestScenario::WriteBaseline(const FString& FilePath, const TMap<FString, int32>& Counts) const
{
	TMap<FString, int32> ExistingBaseline;
	const bool bMergeWithExisting = not FParse::Param(FCommandLine::Get(), TEXT("CoverFinderResetCountBaseline")) &&
		TryLoadBaseline(FilePath, ExistingBaseline);
	TMap<FString, int32> FloorCounts = Counts;
	if (bMergeWithExisting)
	{
		// A key this run did not produce at all has a floor of zero; a key that is new keeps this run's count.
		for (const TPair<FString, int32>& ExistingCount : ExistingBaseline)
		{
			FloorCounts.Add(ExistingCount.Key, FMath::Min(ExistingCount.Value, Counts.FindRef(ExistingCount.Key)));
		}
		FloorCounts.KeySort(TLess<FString>());
	}
	TArray<FString> Lines;
	for (const TPair<FString, int32>& Count : FloorCounts)
	{
		Lines.Add(FString::Printf(
			TEXT("%s%s%d"),
			*Count.Key,
			CoverCountTestScenarioPrivate::BaselineValueSeparator,
			Count.Value));
		UE_LOG(LogRTSCoverCountTest, Display, TEXT("RTS_COVER_COUNT %s=%d"), *Count.Key, Count.Value);
	}
	const bool bSaved = FFileHelper::SaveStringArrayToFile(Lines, *FilePath);
	UE_LOG(
		LogRTSCoverCountTest,
		Display,
		TEXT("RTS_COVER_COUNT_TEST RESULT %s baseline %s with %d keys written to %s"),
		bSaved ? TEXT("PASS") : TEXT("FAIL"),
		bMergeWithExisting ? TEXT("lowered to this run where it was lower,") : TEXT("started afresh"),
		Lines.Num(),
		*FilePath);
}

int32 FCoverCountTestScenario::CompareWithBaseline(
	const TMap<FString, int32>& Counts,
	const TMap<FString, int32>& Baseline) const
{
	using namespace CoverCountTestScenarioPrivate;
	float TotalTolerance = DefaultTotalTolerance;
	FParse::Value(FCommandLine::Get(), TEXT("CoverFinderCountTolerance="), TotalTolerance);
	TSet<FString> AllKeys;
	Counts.GetKeys(AllKeys);
	for (const TPair<FString, int32>& BaselineCount : Baseline)
	{
		AllKeys.Add(BaselineCount.Key);
	}
	AllKeys.Sort(TLess<FString>());
	int32 LostKeyCount = 0;
	for (const FString& Key : AllKeys)
	{
		const int32 Count = Counts.FindRef(Key);
		const int32 BaselineCount = Baseline.FindRef(Key);
		const float Tolerance = Key.StartsWith(TotalSourceName) ? TotalTolerance : ObjectClassTolerance;
		const int32 MinimumCount = FMath::Max(
			0,
			FMath::FloorToInt32(static_cast<float>(BaselineCount) * Tolerance) - AbsoluteSlack);
		const bool bLostCover = Count < MinimumCount;
		LostKeyCount += bLostCover ? 1 : 0;
		UE_LOG(
			LogRTSCoverCountTest,
			Display,
			TEXT("RTS_COVER_COUNT %s=%d baseline=%d minimum=%d %s"),
			*Key,
			Count,
			BaselineCount,
			MinimumCount,
			bLostCover ? TEXT("LOST") : (Count > BaselineCount ? TEXT("more") : TEXT("ok")));
	}
	return LostKeyCount;
}
