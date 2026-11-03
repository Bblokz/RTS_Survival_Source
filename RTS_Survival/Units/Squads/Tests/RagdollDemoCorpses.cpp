// Copyright (C) Bas Blokzijl - All rights reserved.

// The -RagdollCorpseBench mode of URagdollDemoSubsystem: frame cost and pictures of battlefield corpses.

#include "RagdollDemoSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Units/Ragdoll/RTSCorpsePoseLibrary.h"
#include "RTS_Survival/Units/Ragdoll/RTSCorpseSubsystem.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCorpseBench, Log, All);

namespace RagdollCorpseBenchPrivate
{
	// Corpses on the field for each sample; the last is the highest setting. The first sample only warms up: right
	// after the start shaders still compile.
	constexpr int32 CorpseCounts[] = {0, 0, 100, 200, 400};
	constexpr int32 CorpseCountSteps = UE_ARRAY_COUNT(CorpseCounts);
	constexpr int32 FramesToSample = 90;
	constexpr float FieldRadiusCm = 2000.f;
	constexpr float PelvisHeightCm = 20.f;
	constexpr float FullCircleDegrees = 360.f;
	constexpr int32 Seed = 7;
	const FVector OverviewOffset(0.f, -3200.f, 2200.f);
	const FVector CloseUpOffset(0.f, -900.f, 600.f);
	constexpr float BenchFieldOfView = 50.f;
}

bool URagdollDemoSubsystem::TryStartCorpseBench()
{
	using namespace RagdollCorpseBenchPrivate;
	float GroundZ = 0.f;
	if (not TryGetGroundZ(M_DemoLocation, GroundZ) || not CreateCaptureRig())
	{
		Finish(TEXT("no_corpse_field"));
		return false;
	}
	M_DemoLocation.Z = GroundZ;
	if (IConsoleVariable* CorpseCap = IConsoleManager::Get().FindConsoleVariable(TEXT("rts.Corpses")))
	{
		CorpseCap->Set(CorpseCounts[CorpseCountSteps - 1], ECVF_SetByCode);
	}
	M_CorpseBench.Random.Initialize(Seed);
	M_Capture->GetCaptureComponent2D()->FOVAngle = BenchFieldOfView;
	AimCorpseCapture(OverviewOffset);
	M_LastBenchTickSeconds = FPlatformTime::Seconds();
	return true;
}

void URagdollDemoSubsystem::AimCorpseCapture(const FVector& Offset) const
{
	if (ASceneCapture2D* Capture = M_Capture.Get())
	{
		const FVector CameraLocation = M_DemoLocation + Offset;
		Capture->SetActorLocationAndRotation(CameraLocation, (M_DemoLocation - CameraLocation).Rotation());
	}
}

void URagdollDemoSubsystem::TickCorpseBench()
{
	using namespace RagdollCorpseBenchPrivate;
	KeepGameRunning();
	const double NowSeconds = FPlatformTime::Seconds();
	const double FrameMs = (NowSeconds - M_LastBenchTickSeconds) * 1000.0;
	M_LastBenchTickSeconds = NowSeconds;
	if (M_CorpseBench.Step >= CorpseCountSteps)
	{
		TickCorpseCloseUp();
		return;
	}
	const int32 Count = CorpseCounts[M_CorpseBench.Step];
	AddBenchCorpses(Count);
	const FString Label = FString::Printf(TEXT("corpses_%d"), Count);
	if (not SampleFrames(FrameMs, *Label, FramesToSample))
	{
		return;
	}
	const URTSCorpseSubsystem* Corpses = GetWorld()->GetSubsystem<URTSCorpseSubsystem>();
	UE_LOG(LogRTSCorpseBench, Display, TEXT("RTS_RGK step=%d corpses=%d instances=%d batches=%d"), Count,
	       Corpses ? Corpses->GetCorpseCount() : 0, Corpses ? Corpses->GetInstanceCount() : 0,
	       Corpses ? Corpses->GetBatchCount() : 0);
	if (not M_OutputDirectory.IsEmpty() && IsValid(M_RenderTarget))
	{
		UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory, Label + TEXT(".png"));
	}
	++M_CorpseBench.Step;
}

void URagdollDemoSubsystem::TickCorpseCloseUp()
{
	using namespace RagdollCorpseBenchPrivate;
	// A few frames for the capture to move and settle exposure, then one close picture.
	constexpr int32 CloseUpFrames = 10;
	if (M_CorpseBench.CloseUpFrame == 0)
	{
		AimCorpseCapture(CloseUpOffset);
	}
	if (++M_CorpseBench.CloseUpFrame < CloseUpFrames)
	{
		return;
	}
	if (not M_OutputDirectory.IsEmpty() && IsValid(M_RenderTarget))
	{
		UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory,
		                                            TEXT("corpses_closeup.png"));
	}
	Finish(TEXT("corpse_bench_done"));
}

void URagdollDemoSubsystem::AddBenchCorpses(const int32 TargetCount)
{
	using namespace RagdollCorpseBenchPrivate;
	URTSCorpseSubsystem* Corpses = GetWorld()->GetSubsystem<URTSCorpseSubsystem>();
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	const URTSCorpsePoseLibrary* Libraries[] = {
		Settings->GermanCorpsePoses.LoadSynchronous(), Settings->SovietCorpsePoses.LoadSynchronous()
	};
	if (not IsValid(Corpses))
	{
		return;
	}
	FRandomStream& Random = M_CorpseBench.Random;
	while (Corpses->GetCorpseCount() < TargetCount)
	{
		const int32 Side = Random.RandRange(0, UE_ARRAY_COUNT(Libraries) - 1);
		const URTSCorpsePoseLibrary* Library = Libraries[Side];
		if (not IsValid(Library) || Library->Poses.IsEmpty())
		{
			return;
		}
		// A body lying like one of the baked poses, turned at random, as settled ragdolls arrive in play.
		const FRTSCorpsePose& Pose = Library->Poses[Random.RandRange(0, Library->Poses.Num() - 1)];
		const FQuat PoseRotation(FMatrix(Pose.PelvisAxisX, Pose.PelvisAxisY, Pose.PelvisAxisZ, FVector::ZeroVector));
		const float Distance = FMath::Sqrt(Random.FRand()) * FieldRadiusCm;
		const FVector Offset = FRotator(0.f, Random.FRandRange(0.f, FullCircleDegrees), 0.f).Vector() * Distance;
		FRTSCorpseRequest Request;
		Request.Side = static_cast<ERTSCrewSide>(Side);
		Request.Pelvis = FTransform(
			FQuat(FVector::UpVector, FMath::DegreesToRadians(Random.FRandRange(0.f, FullCircleDegrees))) * PoseRotation,
			M_DemoLocation + Offset + FVector(0.f, 0.f, PelvisHeightCm));
		const TArray<TSoftObjectPtr<UMaterialInterface>>& Looks = Side == 0 ? Settings->GermanCrew.Materials
			                                                                : Settings->SovietCrew.Materials;
		Request.Look = Looks.IsEmpty() ? nullptr : Looks[Random.RandRange(0, Looks.Num() - 1)].LoadSynchronous();
		if (not Corpses->TryAddCorpse(Request))
		{
			return;
		}
	}
}

#endif
