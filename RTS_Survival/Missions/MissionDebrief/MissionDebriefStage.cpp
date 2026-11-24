// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionDebriefStage.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

/**
 * The stage is authored in the mock-up's space ("stage units": 1 unit = 10 cm, x right, y up, z towards the camera,
 * right-handed). Unreal gets X = x, Y = z, Z = y (the same swap the glTF import of the stage meshes used), in cm.
 */
namespace MissionDebriefSpace
{
	constexpr float UnitToCm = 10.f;

	FVector ToUnreal(const FVector& StagePoint)
	{
		return FVector(StagePoint.X, StagePoint.Z, StagePoint.Y) * UnitToCm;
	}

	FVector DirectionToUnreal(const FVector& StageDirection)
	{
		return FVector(StageDirection.X, StageDirection.Z, StageDirection.Y).GetSafeNormal();
	}

	/** Column-vector 3x3 rotation in stage space. */
	struct FStageRotation
	{
		double M[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
	};

	FStageRotation Multiply(const FStageRotation& A, const FStageRotation& B)
	{
		FStageRotation Result;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				Result.M[Row][Col] = A.M[Row][0] * B.M[0][Col] + A.M[Row][1] * B.M[1][Col] + A.M[Row][2] * B.M[2][Col];
			}
		}
		return Result;
	}

	FStageRotation RotationX(const double Angle)
	{
		const double C = FMath::Cos(Angle);
		const double S = FMath::Sin(Angle);
		FStageRotation R;
		R.M[1][1] = C;
		R.M[1][2] = -S;
		R.M[2][1] = S;
		R.M[2][2] = C;
		return R;
	}

	FStageRotation RotationY(const double Angle)
	{
		const double C = FMath::Cos(Angle);
		const double S = FMath::Sin(Angle);
		FStageRotation R;
		R.M[0][0] = C;
		R.M[0][2] = S;
		R.M[2][0] = -S;
		R.M[2][2] = C;
		return R;
	}

	FStageRotation RotationZ(const double Angle)
	{
		const double C = FMath::Cos(Angle);
		const double S = FMath::Sin(Angle);
		FStageRotation R;
		R.M[0][0] = C;
		R.M[0][1] = -S;
		R.M[1][0] = S;
		R.M[1][1] = C;
		return R;
	}

	// The mock-up's Euler order 'YXZ': R = Ry * Rx * Rz.
	FStageRotation EulerYXZ(const FVector& Euler)
	{
		return Multiply(Multiply(RotationY(Euler.Y), RotationX(Euler.X)), RotationZ(Euler.Z));
	}

	FQuat RotationToUnreal(const FStageRotation& Stage)
	{
		// Conjugate by the axis swap (x, y, z) -> (x, z, y); a swap of axes keeps a rotation a rotation.
		constexpr int32 Swap[3] = {0, 2, 1};
		double R[3][3];
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				R[Row][Col] = Stage.M[Swap[Row]][Swap[Col]];
			}
		}
		// Unreal matrices hold the image of each basis vector in a row.
		const FMatrix Matrix(FPlane(R[0][0], R[1][0], R[2][0], 0.0), FPlane(R[0][1], R[1][1], R[2][1], 0.0),
		                     FPlane(R[0][2], R[1][2], R[2][2], 0.0), FPlane(0.0, 0.0, 0.0, 1.0));
		return FQuat(Matrix);
	}

	FVector StageScaleToUnreal(const FVector& StageScale)
	{
		return FVector(StageScale.X, StageScale.Z, StageScale.Y);
	}
}

/** Layout and timing of the approved mock-up (Source/Tools/MissionDebrief/steel_seal_mockup.html), in stage units. */
namespace MissionDebriefTimeline
{
	enum class EKeyEase : uint8
	{
		InOut,
		In,
		Out
	};

	struct FStampKey
	{
		float Time;
		FVector Position;
		FVector Euler;
		EKeyEase Ease;
	};

	constexpr float SlamSeconds = 1.85f;
	constexpr float SpinStartSeconds = 3.15f;
	constexpr float SpinEndSeconds = 4.05f;
	constexpr float HoldEndVictory = 4.95f;
	constexpr float HoldEndDefeat = 4.45f;
	constexpr float ExitSeconds = 0.7f;
	constexpr float DurationVictory = 5.95f;
	constexpr float DurationDefeat = 7.9f;
	constexpr float ButtonsVictory = 4.35f;
	constexpr float ButtonsDefeat = 7.35f;
	constexpr float BurnStart = 4.5f;
	constexpr float BurnEnd = 7.2f;
	constexpr float BurnTo = 1.08f;
	constexpr float BurnFrom = -0.14f;
	constexpr float NoBurn = -1.f;
	constexpr float InkDrySeconds = 6.f;
	constexpr float InkRoughWet = 0.34f;
	constexpr float InkRoughDry = 0.55f;

	const FVector TinRest(-2.55f, 0.085f, -0.45f);
	constexpr float TinYaw = 0.08f;
	const FVector AboveTin(-2.55f, 1.1f, -0.45f);
	const FVector AboveOrder(-0.192804f, 1.35f, 0.119784f);
	const FVector OnOrder(0.007196f, 0.006f, 0.119784f);
	constexpr float StampYaw = -0.04f;
	const FVector TipPosition(0.976605f, 1.25f, 1.786662f);
	const FVector TipEuler(-1.933266f, 0.526759f, 0.f);
	constexpr float HoldRise = 0.05f;
	const FVector ExitOffset(3.6f, 2.6f, -0.4f);
	const FVector ExitEulerOffset(0.3f, -0.7f, 0.f);

	const FVector CameraBase(2.7f, 2.55f, 4.75f);
	const FVector CameraLook(-0.45f, 0.2f, -0.45f);
	const FVector CameraDefeatEnd(1.084317f, 1.1724f, 1.971870f);
	const FVector CameraDefeatLook(0.007196f, 0.004f, 0.119784f);
	constexpr float CameraPushStart = 4.6f;
	constexpr float CameraVerticalFov = 38.f;
	constexpr float CameraFovPunch = 1.6f;
	constexpr float CameraFovPunchSeconds = 0.3f;
	constexpr float ShakeAmplitude = 0.06f;
	constexpr float ShakeSeconds = 0.45f;

	constexpr float PaperWidth = 2.1f;
	constexpr float PaperHeight = 2.97f;
	constexpr float PaperLift = 0.004f;
	constexpr float PaperYaw = 0.06f;
	// The stamped word on the sheet in paper UV: centre and half sizes.
	constexpr float WordBoxU = 0.5f;
	constexpr float WordBoxV = 0.459596f;
	constexpr float WordBoxHalfU = 0.528571f;
	constexpr float WordBoxHalfV = 0.161642f;

	TArray<FStampKey> MakeStampKeys(const bool bVictory)
	{
		const float HoldEnd = bVictory ? HoldEndVictory : HoldEndDefeat;
		const FVector OnTinEuler(0.f, TinYaw, 0.f);
		const FVector OrderEuler(0.f, StampYaw, 0.f);
		return {
			{0.f, TinRest, OnTinEuler, EKeyEase::InOut},
			{0.5f, TinRest, OnTinEuler, EKeyEase::InOut},
			{0.95f, AboveTin, OnTinEuler, EKeyEase::Out},
			{1.6f, AboveOrder, FVector(0.f, StampYaw, 0.12f), EKeyEase::InOut},
			{SlamSeconds, OnOrder, OrderEuler, EKeyEase::In},
			{2.15f, OnOrder, OrderEuler, EKeyEase::InOut},
			{3.f, TipPosition, TipEuler, EKeyEase::InOut},
			{HoldEnd, TipPosition + FVector(0.f, HoldRise, 0.f), TipEuler, EKeyEase::InOut},
			{HoldEnd + ExitSeconds, TipPosition + ExitOffset, TipEuler + ExitEulerOffset, EKeyEase::In}
		};
	}

	// Weapons lying on their sides: position, yaw, which side faces up and the lift that rests them on the desk.
	const FVector Mp40Position(-4.6f, 0.f, 0.4f);
	constexpr float Mp40Yaw = 2.170796f;
	constexpr float Mp40Lift = 0.3908f;
	const FVector Mp44Position(0.2f, 0.f, -3.3f);
	constexpr float Mp44Yaw = 0.06f;
	constexpr float Mp44Lift = 0.3096f;

	// Lights (stage units); spot angles are half angles in radians.
	const FVector LampPosition(-2.998653f, 2.116323f, -1.280983f);
	const FVector LampTarget(-0.2f, 0.f, -0.2f);
	constexpr float LampAngle = 0.78f;
	constexpr float LampPenumbra = 0.55f;
	const FVector WashPosition(-3.198653f, 2.516323f, -1.580983f);
	const FVector WashTarget(-0.3f, 1.8f, -5.f);
	constexpr float WashAngle = 0.42f;
	constexpr float WashPenumbra = 0.75f;
	const FVector GlowPosition(-3.127021f, 2.325515f, -1.328526f);
	const FVector FillFrom(4.f, 4.5f, 6.f);
	const FVector RimFrom(3.f, 3.f, -5.f);
	const FVector AmbientPosition(2.f, 3.f, 4.f);
	constexpr float FireHeightBurning = 0.9f;
	constexpr float FireHeightRidge = 0.55f;
	constexpr float FireRampSeconds = 0.4f;

	// Closed room around the desk, in stage units: min and max corners.
	const FVector RoomMin(-9.f, -2.2f, -6.2f);
	const FVector RoomMax(9.f, 8.f, 9.f);
	constexpr float RoomWallThickness = 0.1f;

	constexpr int32 FlamePoolSize = 700;
	constexpr int32 EmberPoolSize = 360;
	constexpr int32 SmokePoolSize = 220;
}

namespace MissionDebriefMath
{
	float Seg(const float T, const float A, const float B)
	{
		return FMath::Clamp((T - A) / (B - A), 0.f, 1.f);
	}

	float EaseIn(const float X)
	{
		return X * X * X;
	}

	float EaseOut(const float X)
	{
		return 1.f - FMath::Pow(1.f - X, 3.f);
	}

	float EaseInOut(const float X)
	{
		return X < 0.5f ? 4.f * X * X * X : 1.f - FMath::Pow(-2.f * X + 2.f, 3.f) / 2.f;
	}

	float ApplyEase(const MissionDebriefTimeline::EKeyEase Ease, const float X)
	{
		switch (Ease)
		{
		case MissionDebriefTimeline::EKeyEase::In:
			return EaseIn(X);
		case MissionDebriefTimeline::EKeyEase::Out:
			return EaseOut(X);
		default:
			return EaseInOut(X);
		}
	}

	void PoseAt(const TArray<MissionDebriefTimeline::FStampKey>& Keys, const float T, FVector& OutPosition,
	            FVector& OutEuler)
	{
		OutPosition = Keys[0].Position;
		OutEuler = Keys[0].Euler;
		if (T <= Keys[0].Time)
		{
			return;
		}
		for (int32 Index = 1; Index < Keys.Num(); ++Index)
		{
			const MissionDebriefTimeline::FStampKey& B = Keys[Index];
			if (T > B.Time)
			{
				continue;
			}
			const MissionDebriefTimeline::FStampKey& A = Keys[Index - 1];
			const float K = ApplyEase(B.Ease, Seg(T, A.Time, B.Time));
			OutPosition = FMath::Lerp(A.Position, B.Position, K);
			OutEuler = FMath::Lerp(A.Euler, B.Euler, K);
			return;
		}
		OutPosition = Keys.Last().Position;
		OutEuler = Keys.Last().Euler;
	}

	FVector PaperToStage(const float U, const float V, const float Lift)
	{
		using namespace MissionDebriefTimeline;
		const float LocalX = (U - 0.5f) * PaperWidth;
		const float LocalZ = -(V - 0.5f) * PaperHeight;
		const float C = FMath::Cos(PaperYaw);
		const float S = FMath::Sin(PaperYaw);
		return FVector(LocalX * C + LocalZ * S, PaperLift + Lift, -LocalX * S + LocalZ * C);
	}
}

namespace MissionDebriefCVars
{
	// Look-dev switches for the debrief stage (read when a debrief starts).
	TAutoConsoleVariable<int32> CVarDefaultChannels(
		TEXT("RTS.Debrief.DefaultChannels"), 0,
		TEXT("1: stage meshes and lights use lighting channel 0 instead of the stage-only channel 1."));
	TAutoConsoleVariable<int32> CVarKeepLevelExposure(
		TEXT("RTS.Debrief.KeepLevelExposure"), 0,
		TEXT("1: the stage camera keeps the level's exposure instead of its fixed manual exposure."));
	TAutoConsoleVariable<float> CVarLightScale(
		TEXT("RTS.Debrief.LightScale"), 1.f,
		TEXT("Multiplies every stage light's intensity."));
}

namespace MissionDebriefAssets
{
	const TCHAR* const Root = TEXT("/Game/RTS_Survival/Blueprints/Missions/MissionDebrief");
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const PlaneMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	constexpr int32 StageLightingChannel = 1;
	// The engine plane is 100 x 100 cm.
	constexpr float SpritePlaneSizeCm = 100.f;
	constexpr int32 SpriteCustomDataFloats = 4;
	constexpr float PrestreamSeconds = 4.f;
	constexpr float LightAttenuationCm = 1500.f;

	template <typename T>
	TSoftObjectPtr<T> Asset(const FString& RelativePath)
	{
		const FString Name = FPaths::GetBaseFilename(RelativePath);
		return TSoftObjectPtr<T>(FSoftObjectPath(FString(Root) / RelativePath + TEXT(".") + Name));
	}

	void UseStageLightingOnly(UPrimitiveComponent* const Component)
	{
		Component->LightingChannels.bChannel0 = false;
		Component->LightingChannels.bChannel1 = true;
	}

	void UseStageLightingOnly(ULightComponent* const Light)
	{
		Light->LightingChannels.bChannel0 = false;
		Light->LightingChannels.bChannel1 = true;
	}
}

AMissionDebriefStage::AMissionDebriefStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetActorEnableCollision(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_Root);
	M_Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("DebriefCamera"));
	M_Camera->SetupAttachment(M_Root);

	Constructor_CreateMeshComponents();
	Constructor_CreateLights();
	Constructor_CreateSprites();
	Constructor_SetDefaultAssetPaths();
}

void AMissionDebriefStage::Constructor_CreateMeshComponents()
{
	auto MakeMesh = [this](const TCHAR* const Name, USceneComponent* const Parent) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* const Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		// The stage lights only reach lighting channel 1, and the stage's translucent pieces need the regular path.
		Mesh->bDisallowNanite = true;
		MissionDebriefAssets::UseStageLightingOnly(Mesh);
		return Mesh;
	};
	M_SetComponent = MakeMesh(TEXT("Set"), M_Root);
	M_TinPadComponent = MakeMesh(TEXT("TinPad"), M_Root);
	M_PaperComponent = MakeMesh(TEXT("Paper"), M_Root);
	M_SootComponent = MakeMesh(TEXT("Soot"), M_Root);
	M_SootComponent->SetCastShadow(false);
	M_StampRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StampRoot"));
	M_StampRoot->SetupAttachment(M_Root);
	M_StampBodyComponent = MakeMesh(TEXT("StampBody"), M_StampRoot);
	M_FaceMirroredComponent = MakeMesh(TEXT("FaceMirrored"), M_StampRoot);
	M_FaceReadableComponent = MakeMesh(TEXT("FaceReadable"), M_StampRoot);
	M_Mp40Component = MakeMesh(TEXT("Mp40"), M_Root);
	M_Mp44Component = MakeMesh(TEXT("Mp44"), M_Root);
	static const TCHAR* const RoomNames[] = {
		TEXT("RoomFloor"), TEXT("RoomCeiling"), TEXT("RoomLeft"), TEXT("RoomRight"), TEXT("RoomBack"), TEXT("RoomFront")
	};
	for (const TCHAR* const RoomName : RoomNames)
	{
		M_EnclosureComponents.Add(MakeMesh(RoomName, M_Root));
	}
}

void AMissionDebriefStage::Constructor_CreateLights()
{
	auto Prepare = [](ULightComponent* const Light, const bool bCastShadows)
	{
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(bCastShadows);
		MissionDebriefAssets::UseStageLightingOnly(Light);
	};
	M_LampLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("LampLight"));
	M_WallWashLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("WallWashLight"));
	M_LampGlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LampGlowLight"));
	M_FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	M_AmbientLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AmbientLight"));
	M_FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	M_RimLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("RimLight"));
	for (ULightComponent* const Light : TArray<ULightComponent*>{
		     M_LampLight, M_WallWashLight, M_LampGlowLight, M_FireLight, M_AmbientLight, M_FillLight, M_RimLight
	     })
	{
		Light->SetupAttachment(M_Root);
		Prepare(Light, Light == M_LampLight || Light == M_WallWashLight);
	}
	for (ULocalLightComponent* const Light : TArray<ULocalLightComponent*>{
		     M_LampLight, M_WallWashLight, M_LampGlowLight, M_FireLight, M_AmbientLight
	     })
	{
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetAttenuationRadius(MissionDebriefAssets::LightAttenuationCm);
	}
	M_FillLight->SetAtmosphereSunLight(false);
	M_RimLight->SetAtmosphereSunLight(false);
	M_FillLight->SetIntensity(0.f);
	M_RimLight->SetIntensity(0.f);
}

void AMissionDebriefStage::Constructor_CreateSprites()
{
	auto MakeSprites = [this](const TCHAR* const Name) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* const Sprites = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Sprites->SetupAttachment(M_Root);
		Sprites->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sprites->SetCastShadow(false);
		Sprites->SetCanEverAffectNavigation(false);
		Sprites->NumCustomDataFloats = MissionDebriefAssets::SpriteCustomDataFloats;
		MissionDebriefAssets::UseStageLightingOnly(Sprites);
		return Sprites;
	};
	M_FlameSprites = MakeSprites(TEXT("FlameSprites"));
	M_EmberSprites = MakeSprites(TEXT("EmberSprites"));
	M_SmokeSprites = MakeSprites(TEXT("SmokeSprites"));
}

void AMissionDebriefStage::Constructor_SetDefaultAssetPaths()
{
	using namespace MissionDebriefAssets;
	SetMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_Set/SM_Debrief_Set"));
	StampBodyMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_StampBody/SM_Debrief_StampBody"));
	PaperMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_Paper/SM_Debrief_Paper"));
	SootMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_Soot/SM_Debrief_Soot"));
	TinPadVictoryMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_TinPad_Victory/SM_Debrief_TinPad_Victory"));
	TinPadDefeatMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_TinPad_Defeat/SM_Debrief_TinPad_Defeat"));
	FaceVictoryMirroredMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_Face_Victory_Mirrored/SM_Debrief_Face_Victory_Mirrored"));
	FaceVictoryReadableMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_Face_Victory_Readable/SM_Debrief_Face_Victory_Readable"));
	FaceDefeatMirroredMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_Face_Defeat_Mirrored/SM_Debrief_Face_Defeat_Mirrored"));
	FaceDefeatReadableMesh = Asset<UStaticMesh>(TEXT("Imported/SM_Debrief_Face_Defeat_Readable/SM_Debrief_Face_Defeat_Readable"));
	Mp40Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/MP40/Mesh/SM_Ger_InfWeapon_MP40.SM_Ger_InfWeapon_MP40")));
	Mp44Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/STG44/Meshes/SM_Weapon_MP44.SM_Weapon_MP44")));
	Mp44Material = Asset<UMaterialInterface>(TEXT("Materials/M_Debrief_MP44"));
	PaperMaterial = Asset<UMaterialInterface>(TEXT("Materials/M_Debrief_Paper"));
	SootMaterial = Asset<UMaterialInterface>(TEXT("Materials/M_Debrief_Soot"));
	FlameMaterial = Asset<UMaterialInterface>(TEXT("Materials/MI_Debrief_Flame"));
	EmberMaterial = Asset<UMaterialInterface>(TEXT("Materials/MI_Debrief_Ember"));
	SmokeMaterial = Asset<UMaterialInterface>(TEXT("Materials/M_Debrief_SpriteAlpha"));
	EnclosureMaterial = Asset<UMaterialInterface>(TEXT("Materials/M_Debrief_Black"));
	InkVictoryTexture = Asset<UTexture>(TEXT("Textures/T_Debrief_Ink_Victory"));
	InkDefeatTexture = Asset<UTexture>(TEXT("Textures/T_Debrief_Ink_Defeat"));
	InkHeightVictoryTexture = Asset<UTexture>(TEXT("Textures/T_Debrief_InkHeight_Victory"));
	InkHeightDefeatTexture = Asset<UTexture>(TEXT("Textures/T_Debrief_InkHeight_Defeat"));
	BurnField = Asset<UMissionDebriefBurnField>(TEXT("DA_Debrief_BurnField"));
}

void AMissionDebriefStage::BeginPlay()
{
	Super::BeginPlay();
	SetTickableWhenPaused(true);
}

bool AMissionDebriefStage::StartDebrief(const EMissionDebriefOutcome Outcome, APlayerController* Viewer)
{
	M_Outcome = Outcome;
	M_Viewer = Viewer;
	if (not StartDebrief_LoadMeshes() || not StartDebrief_SetupPaper())
	{
		RTSFunctionLibrary::ReportError("Mission debrief stage could not load its assets; the end screen shows without it.");
		return false;
	}
	StartDebrief_ApplyLookDevSwitches();
	StartDebrief_PlaceStaticPieces();
	StartDebrief_PlaceLights();
	StartDebrief_SetupCamera();
	StartDebrief_SetupSprites();
	M_BurnField = BurnField.LoadSynchronous();
	M_ElapsedSeconds = 0.f;
	const UWorld* const World = GetWorld();
	M_LastRealSeconds = World != nullptr ? World->GetRealTimeSeconds() : 0.0;
	bM_IsRunning = true;
	TickTimeline(0.f);
	StartDebrief_TakeOverView();
	return true;
}

bool AMissionDebriefStage::StartDebrief_LoadMeshes()
{
	const bool bVictory = GetIsVictory();
	const TArray<TPair<UStaticMeshComponent*, UStaticMesh*>> Assignments = {
		{M_SetComponent, SetMesh.LoadSynchronous()},
		{M_TinPadComponent, (bVictory ? TinPadVictoryMesh : TinPadDefeatMesh).LoadSynchronous()},
		{M_PaperComponent, PaperMesh.LoadSynchronous()},
		{M_SootComponent, SootMesh.LoadSynchronous()},
		{M_StampBodyComponent, StampBodyMesh.LoadSynchronous()},
		{M_FaceMirroredComponent, (bVictory ? FaceVictoryMirroredMesh : FaceDefeatMirroredMesh).LoadSynchronous()},
		{M_FaceReadableComponent, (bVictory ? FaceVictoryReadableMesh : FaceDefeatReadableMesh).LoadSynchronous()},
		{M_Mp40Component, Mp40Mesh.LoadSynchronous()},
		{M_Mp44Component, Mp44Mesh.LoadSynchronous()}
	};
	for (const TPair<UStaticMeshComponent*, UStaticMesh*>& Assignment : Assignments)
	{
		if (Assignment.Value == nullptr)
		{
			return false;
		}
		Assignment.Key->SetStaticMesh(Assignment.Value);
		Assignment.Key->PrestreamTextures(MissionDebriefAssets::PrestreamSeconds, true);
	}
	if (UMaterialInterface* const Mp44Pbr = Mp44Material.LoadSynchronous())
	{
		M_Mp44Component->SetMaterial(0, Mp44Pbr);
	}
	UStaticMesh* const Cube = LoadObject<UStaticMesh>(nullptr, MissionDebriefAssets::CubeMesh);
	UMaterialInterface* const Black = EnclosureMaterial.LoadSynchronous();
	for (UStaticMeshComponent* const Wall : M_EnclosureComponents)
	{
		Wall->SetStaticMesh(Cube);
		Wall->SetMaterial(0, Black);
	}
	return Cube != nullptr;
}

bool AMissionDebriefStage::StartDebrief_SetupPaper()
{
	const bool bVictory = GetIsVictory();
	UMaterialInterface* const PaperBase = PaperMaterial.LoadSynchronous();
	UMaterialInterface* const SootBase = SootMaterial.LoadSynchronous();
	UTexture* const Ink = (bVictory ? InkVictoryTexture : InkDefeatTexture).LoadSynchronous();
	UTexture* const InkHeight = (bVictory ? InkHeightVictoryTexture : InkHeightDefeatTexture).LoadSynchronous();
	if (PaperBase == nullptr || SootBase == nullptr || Ink == nullptr || InkHeight == nullptr)
	{
		return false;
	}
	M_PaperMaterial = UMaterialInstanceDynamic::Create(PaperBase, this);
	M_PaperMaterial->SetTextureParameterValue(TEXT("InkTex"), Ink);
	M_PaperMaterial->SetTextureParameterValue(TEXT("InkHeightTex"), InkHeight);
	M_PaperMaterial->SetScalarParameterValue(TEXT("KeepOn"), bVictory ? 0.f : 1.f);
	const FColor InkSrgb = bVictory ? FColor(0x6c, 0x10, 0x09) : FColor(0x14, 0x11, 0x10);
	M_PaperMaterial->SetVectorParameterValue(TEXT("InkColor"), FLinearColor::FromSRGBColor(InkSrgb));
	M_PaperMaterial->SetScalarParameterValue(TEXT("BumpStrength"), InkBumpStrength);
	M_PaperMaterial->SetScalarParameterValue(TEXT("BumpSignY"), InkBumpSignY);
	M_PaperComponent->SetMaterial(0, M_PaperMaterial);
	M_SootMaterial = UMaterialInstanceDynamic::Create(SootBase, this);
	M_SootComponent->SetMaterial(0, M_SootMaterial);
	return true;
}

void AMissionDebriefStage::StartDebrief_ApplyLookDevSwitches()
{
	M_LightScale = FMath::Max(0.f, MissionDebriefCVars::CVarLightScale.GetValueOnGameThread());
	if (MissionDebriefCVars::CVarDefaultChannels.GetValueOnGameThread() == 0)
	{
		return;
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* const Primitive : Primitives)
	{
		Primitive->SetLightingChannels(true, false, false);
	}
	TInlineComponentArray<ULightComponent*> Lights(this);
	for (ULightComponent* const Light : Lights)
	{
		Light->SetLightingChannels(true, false, false);
	}
}

void AMissionDebriefStage::StartDebrief_PlaceStaticPieces()
{
	using namespace MissionDebriefSpace;
	using namespace MissionDebriefTimeline;
	auto PlaceWeapon = [](UStaticMeshComponent* const Weapon, const FVector& Position, const float Yaw, const bool bFlip,
	                      const float Lift)
	{
		const FStageRotation Rotation = Multiply(RotationY(Yaw), RotationX(bFlip ? UE_DOUBLE_HALF_PI : -UE_DOUBLE_HALF_PI));
		Weapon->SetRelativeTransform(FTransform(RotationToUnreal(Rotation), ToUnreal(Position + FVector(0.f, Lift, 0.f))));
	};
	PlaceWeapon(M_Mp40Component, Mp40Position, Mp40Yaw, true, Mp40Lift);
	PlaceWeapon(M_Mp44Component, Mp44Position, Mp44Yaw, false, Mp44Lift);

	// Floor, ceiling, left, right, back, front slabs of the closed room.
	const FVector Size = RoomMax - RoomMin;
	const FVector Centre = (RoomMin + RoomMax) * 0.5f;
	const TArray<TPair<FVector, FVector>> Slabs = {
		{FVector(Centre.X, RoomMin.Y, Centre.Z), FVector(Size.X, RoomWallThickness, Size.Z)},
		{FVector(Centre.X, RoomMax.Y, Centre.Z), FVector(Size.X, RoomWallThickness, Size.Z)},
		{FVector(RoomMin.X, Centre.Y, Centre.Z), FVector(RoomWallThickness, Size.Y, Size.Z)},
		{FVector(RoomMax.X, Centre.Y, Centre.Z), FVector(RoomWallThickness, Size.Y, Size.Z)},
		{FVector(Centre.X, Centre.Y, RoomMin.Z), FVector(Size.X, Size.Y, RoomWallThickness)},
		{FVector(Centre.X, Centre.Y, RoomMax.Z), FVector(Size.X, Size.Y, RoomWallThickness)}
	};
	constexpr float CubeSizeCm = 100.f;
	for (int32 Index = 0; Index < Slabs.Num() && Index < M_EnclosureComponents.Num(); ++Index)
	{
		const FVector ScaleCm = StageScaleToUnreal(Slabs[Index].Value) * UnitToCm / CubeSizeCm;
		M_EnclosureComponents[Index]->SetRelativeTransform(FTransform(FQuat::Identity, ToUnreal(Slabs[Index].Key), ScaleCm));
	}
}

void AMissionDebriefStage::StartDebrief_PlaceLights()
{
	using namespace MissionDebriefSpace;
	using namespace MissionDebriefTimeline;
	auto AimSpot = [](USpotLightComponent* const Spot, const FVector& From, const FVector& To, const float HalfAngle,
	                  const float Penumbra, const float Intensity, const FColor& Colour)
	{
		const FVector Direction = DirectionToUnreal(To - From);
		Spot->SetRelativeLocationAndRotation(ToUnreal(From), FRotationMatrix::MakeFromX(Direction).Rotator());
		const float OuterDegrees = FMath::RadiansToDegrees(HalfAngle);
		Spot->SetOuterConeAngle(OuterDegrees);
		Spot->SetInnerConeAngle(OuterDegrees * (1.f - Penumbra));
		Spot->SetIntensity(Intensity);
		Spot->SetLightColor(FLinearColor::FromSRGBColor(Colour));
	};
	// The lamp's bulb and shade are solid meshes here: the light sits just in front of the bulb, inside the shade.
	constexpr float LampOffsetFromBulb = 0.16f;
	const FVector LampFrom = LampPosition + (LampTarget - LampPosition).GetSafeNormal() * LampOffsetFromBulb;
	AimSpot(M_LampLight, LampFrom, LampTarget, LampAngle, LampPenumbra, LampIntensity * M_LightScale, FColor(0xff, 0xcf, 0x96));
	AimSpot(M_WallWashLight, WashPosition, WashTarget, WashAngle, WashPenumbra, WallWashIntensity * M_LightScale, FColor(0xff, 0xd4, 0xa4));
	M_LampGlowLight->SetRelativeLocation(ToUnreal(GlowPosition));
	M_LampGlowLight->SetIntensity(LampGlowIntensity * M_LightScale);
	M_LampGlowLight->SetLightColor(FLinearColor::FromSRGBColor(FColor(0xff, 0xc9, 0x8a)));
	M_AmbientLight->SetRelativeLocation(ToUnreal(AmbientPosition));
	M_AmbientLight->SetIntensity(AmbientIntensity * M_LightScale);
	M_AmbientLight->SetLightColor(FLinearColor::FromSRGBColor(FColor(0xc8, 0xd0, 0xda)));
	M_FireLight->SetIntensity(0.f);
	M_FireLight->SetLightColor(FLinearColor::FromSRGBColor(FColor(0xff, 0x7a, 0x2a)));
	M_FillLight->SetRelativeRotation(FRotationMatrix::MakeFromX(DirectionToUnreal(-FillFrom)).Rotator());
	M_FillLight->SetIntensity(FillIntensity * M_LightScale);
	M_FillLight->SetLightColor(FLinearColor::FromSRGBColor(FColor(0xc4, 0xcf, 0xe2)));
	M_RimLight->SetRelativeRotation(FRotationMatrix::MakeFromX(DirectionToUnreal(-RimFrom)).Rotator());
	M_RimLight->SetIntensity(RimIntensity * M_LightScale);
	M_RimLight->SetLightColor(FLinearColor::FromSRGBColor(FColor(0xb8, 0xc8, 0xff)));
}

void AMissionDebriefStage::StartDebrief_SetupCamera()
{
	constexpr float ReferenceAspect = 16.f / 9.f;
	M_Camera->SetAspectRatio(ReferenceAspect);
	M_Camera->SetConstraintAspectRatio(false);
	M_Camera->bOverrideAspectRatioAxisConstraint = true;
	M_Camera->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
	FPostProcessSettings& Post = M_Camera->PostProcessSettings;
	const bool bFixedExposure = MissionDebriefCVars::CVarKeepLevelExposure.GetValueOnGameThread() == 0;
	Post.bOverride_AutoExposureMethod = bFixedExposure;
	Post.AutoExposureMethod = AEM_Manual;
	Post.bOverride_AutoExposureApplyPhysicalCameraExposure = bFixedExposure;
	Post.AutoExposureApplyPhysicalCameraExposure = false;
	Post.bOverride_AutoExposureBias = bFixedExposure;
	Post.AutoExposureBias = ExposureBias;
	Post.bOverride_MotionBlurAmount = true;
	Post.MotionBlurAmount = 0.f;
	Post.bOverride_LensFlareIntensity = true;
	Post.LensFlareIntensity = 0.f;
	Post.bOverride_VignetteIntensity = true;
	Post.VignetteIntensity = 0.45f;
	M_Camera->PostProcessBlendWeight = 1.f;
}

void AMissionDebriefStage::StartDebrief_SetupSprites()
{
	using namespace MissionDebriefTimeline;
	UStaticMesh* const Plane = LoadObject<UStaticMesh>(nullptr, MissionDebriefAssets::PlaneMesh);
	const TArray<TTuple<UInstancedStaticMeshComponent*, FMissionDebriefParticlePool*, UMaterialInterface*, int32,
	                    EMissionDebriefParticleKind>> Pools = {
		{M_FlameSprites, &M_Flames, FlameMaterial.LoadSynchronous(), FlamePoolSize, EMissionDebriefParticleKind::Flame},
		{M_EmberSprites, &M_Embers, EmberMaterial.LoadSynchronous(), EmberPoolSize, EMissionDebriefParticleKind::Ember},
		{M_SmokeSprites, &M_Smoke, SmokeMaterial.LoadSynchronous(), SmokePoolSize, EMissionDebriefParticleKind::Smoke}
	};
	for (const auto& [Sprites, Pool, Material, Count, Kind] : Pools)
	{
		Pool->Kind = Kind;
		Pool->Particles.SetNum(GetIsVictory() ? 0 : Count);
		Sprites->SetStaticMesh(Plane);
		Sprites->SetMaterial(0, Material);
		Sprites->ClearInstances();
		if (GetIsVictory())
		{
			continue;
		}
		TArray<FTransform> Hidden;
		Hidden.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Count);
		Sprites->AddInstances(Hidden, false);
	}
}

void AMissionDebriefStage::StartDebrief_TakeOverView()
{
	if (not GetIsValidViewer())
	{
		return;
	}
	M_Viewer->SetViewTarget(this);
	if (APlayerCameraManager* const CameraManager = M_Viewer->PlayerCameraManager)
	{
		CameraManager->SetGameCameraCutThisFrame();
	}
	// The map's post process materials (fog of war, unexplored ground) black out everything outside the map, the
	// stage included; the debrief needs none of them.
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		bM_HasDisabledPostProcessMaterials = GEngine->GameViewport->EngineShowFlags.PostProcessMaterial;
		GEngine->GameViewport->EngineShowFlags.SetPostProcessMaterial(false);
	}
}

void AMissionDebriefStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bM_HasDisabledPostProcessMaterials && GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->EngineShowFlags.SetPostProcessMaterial(true);
		bM_HasDisabledPostProcessMaterials = false;
	}
	Super::EndPlay(EndPlayReason);
}

void AMissionDebriefStage::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (not bM_IsRunning)
	{
		return;
	}
	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// Real time: the world is paused while the end screen shows. Only a long hitch (the asset load) is cut short.
	constexpr double MaxStepSeconds = 0.25;
	const double Now = World->GetRealTimeSeconds();
	const float Step = static_cast<float>(FMath::Clamp(Now - M_LastRealSeconds, 0.0, MaxStepSeconds));
	M_LastRealSeconds = Now;
	TickTimeline(Step);
}

void AMissionDebriefStage::TickTimeline(const float DeltaSeconds)
{
	using namespace MissionDebriefTimeline;
	const float Duration = GetIsVictory() ? DurationVictory : DurationDefeat;
	M_ElapsedSeconds = FMath::Min(M_ElapsedSeconds + DeltaSeconds, Duration);
	const float Time = M_ElapsedSeconds;
	TickCamera(Time);
	TickStamp(Time);
	TickPaperAndLights(Time, DeltaSeconds);
	const float ButtonsDue = GetIsVictory() ? ButtonsVictory : ButtonsDefeat;
	if (not bM_HasFiredButtonsDue && Time >= ButtonsDue)
	{
		bM_HasFiredButtonsDue = true;
		OnButtonsDue.ExecuteIfBound();
	}
}

void AMissionDebriefStage::TickCamera(const float Time)
{
	using namespace MissionDebriefSpace;
	using namespace MissionDebriefTimeline;
	using namespace MissionDebriefMath;
	const float Duration = GetIsVictory() ? DurationVictory : DurationDefeat;
	const float Push = GetIsVictory() ? 0.f : EaseInOut(Seg(Time, CameraPushStart, Duration));
	FVector Position = FMath::Lerp(CameraBase, GetIsVictory() ? CameraBase : CameraDefeatEnd, Push);
	const FVector Look = FMath::Lerp(CameraLook, GetIsVictory() ? CameraLook : CameraDefeatLook, Push);
	if (Time > SlamSeconds && Time < SlamSeconds + ShakeSeconds)
	{
		const float Decay = FMath::Square(1.f - (Time - SlamSeconds) / ShakeSeconds) * ShakeAmplitude;
		Position += FVector(FMath::Sin(Time * 93.f), FMath::Sin(Time * 71.f + 1.f), FMath::Sin(Time * 83.f + 2.f)) * Decay;
	}
	const FVector Direction = DirectionToUnreal(Look - Position);
	M_Camera->SetRelativeLocationAndRotation(ToUnreal(Position), FRotationMatrix::MakeFromXZ(Direction, FVector::UpVector).Rotator());
	float VerticalFov = CameraVerticalFov;
	if (Time > SlamSeconds && Time < SlamSeconds + CameraFovPunchSeconds)
	{
		VerticalFov -= (1.f - (Time - SlamSeconds) / CameraFovPunchSeconds) * CameraFovPunch;
	}
	constexpr float ReferenceAspect = 16.f / 9.f;
	const float HorizontalFov = 2.f * FMath::RadiansToDegrees(
		FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFov * 0.5f)) * ReferenceAspect));
	M_Camera->SetFieldOfView(HorizontalFov);
}

void AMissionDebriefStage::TickStamp(const float Time)
{
	using namespace MissionDebriefSpace;
	using namespace MissionDebriefTimeline;
	using namespace MissionDebriefMath;
	const TArray<FStampKey> Keys = MakeStampKeys(GetIsVictory());
	FVector Position;
	FVector Euler;
	PoseAt(Keys, Time, Position, Euler);
	// A full turn around the vertical axis; the face swaps to the readable one while it points away from the camera.
	const float Spin = EaseInOut(Seg(Time, SpinStartSeconds, SpinEndSeconds));
	Euler.Y += Spin * UE_TWO_PI;
	const float HoldEnd = GetIsVictory() ? HoldEndVictory : HoldEndDefeat;
	if (Time > SpinEndSeconds && Time < HoldEnd)
	{
		constexpr float SettleSeconds = 0.5f;
		constexpr float SettleWaves = 2.5f;
		constexpr float SettleAmount = 0.06f;
		const float K = Seg(Time, SpinEndSeconds, SpinEndSeconds + SettleSeconds);
		Euler.Z += FMath::Sin(K * UE_PI * SettleWaves) * (1.f - K) * SettleAmount;
	}
	constexpr float SquashSeconds = 0.25f;
	constexpr float SquashAmount = 0.035f;
	constexpr float SquashWiden = 0.4f;
	const float Squash = Time > SlamSeconds && Time < SlamSeconds + SquashSeconds
		                     ? (1.f - (Time - SlamSeconds) / SquashSeconds) * SquashAmount
		                     : 0.f;
	const FVector StageScale(1.f + Squash * SquashWiden, 1.f - Squash, 1.f + Squash * SquashWiden);
	M_StampRoot->SetRelativeTransform(FTransform(RotationToUnreal(EulerYXZ(Euler)), ToUnreal(Position),
	                                             StageScaleToUnreal(StageScale)));
	const bool bShowReadable = Spin >= 0.5f;
	constexpr float HideBeforeExitEnd = 0.02f;
	const bool bStampVisible = Time < HoldEnd + ExitSeconds - HideBeforeExitEnd;
	M_StampBodyComponent->SetVisibility(bStampVisible);
	M_FaceMirroredComponent->SetVisibility(bStampVisible && not bShowReadable);
	M_FaceReadableComponent->SetVisibility(bStampVisible && bShowReadable);
}

void AMissionDebriefStage::TickPaperAndLights(const float Time, const float DeltaSeconds)
{
	using namespace MissionDebriefSpace;
	using namespace MissionDebriefTimeline;
	using namespace MissionDebriefMath;
	const float Now = static_cast<float>(M_LastRealSeconds);
	const bool bInked = Time >= SlamSeconds;
	M_PaperMaterial->SetScalarParameterValue(TEXT("InkAmount"), bInked ? 1.f : 0.f);
	M_PaperMaterial->SetScalarParameterValue(TEXT("InkRough"),
	                                         FMath::Lerp(InkRoughWet, InkRoughDry, Seg(Time, SlamSeconds, SlamSeconds + InkDrySeconds)));
	M_PaperMaterial->SetScalarParameterValue(TEXT("Time"), Now);
	if (GetIsVictory())
	{
		M_PaperMaterial->SetScalarParameterValue(TEXT("Burn"), NoBurn);
		return;
	}
	const float BurnK = Seg(Time, BurnStart, BurnEnd);
	const float Burn = Time < BurnStart ? NoBurn : FMath::Lerp(BurnFrom, BurnTo, EaseInOut(BurnK));
	M_PaperMaterial->SetScalarParameterValue(TEXT("Burn"), Burn);
	constexpr float SootDelay = 0.3f;
	constexpr float SootOpacity = 0.85f;
	M_SootMaterial->SetScalarParameterValue(TEXT("Opacity"), SootOpacity * Seg(Time, BurnStart + SootDelay, BurnEnd));
	const bool bMainBurn = Time > BurnStart && Time < BurnEnd + 0.1f;
	const bool bRidgeBurn = Time > BurnEnd - 0.6f;
	const float Flicker = 0.7f + 0.3f * FMath::Sin(Now * 13.f) * FMath::Sin(Now * 7.7f);
	const FVector FireAt(OnOrder.X, bMainBurn ? FireHeightBurning : FireHeightRidge, OnOrder.Z);
	M_FireLight->SetRelativeLocation(ToUnreal(FireAt));
	const float BurnFire = bMainBurn ? FireBurnIntensity * Seg(Time, BurnStart, BurnStart + FireRampSeconds) : 0.f;
	M_FireLight->SetIntensity((BurnFire * Flicker + (bRidgeBurn ? FireRidgeIntensity * Flicker : 0.f)) * M_LightScale);
	M_LampLight->SetIntensity(FMath::Lerp(LampIntensity, LampBurnIntensity, BurnK) * M_LightScale);
	M_FillLight->SetIntensity(FMath::Lerp(FillIntensity, FillBurnIntensity, BurnK) * M_LightScale);
	TickBurnParticles(Time, DeltaSeconds, Burn);
}

void AMissionDebriefStage::TickBurnParticles(const float Time, const float DeltaSeconds, const float BurnAmount)
{
	using namespace MissionDebriefTimeline;
	if (not GetIsValidBurnField())
	{
		return;
	}
	const bool bMainBurn = Time > BurnStart && Time < BurnEnd + 0.1f;
	const bool bRidgeBurn = Time > BurnEnd - 0.6f;
	// While the page burns, the whole front spawns; afterwards only the stamped letters' outside edges keep burning.
	const bool bFront = bMainBurn && not bRidgeBurn;
	const float FlameRate = bMainBurn ? 320.f : (bRidgeBurn ? 520.f : 0.f);
	const float EmberRate = bMainBurn ? 120.f : (bRidgeBurn ? 35.f : 0.f);
	const float SmokeRate = bMainBurn ? 30.f : (bRidgeBurn ? 18.f : 0.f);
	TickParticlePool(M_Flames, M_FlameSprites, DeltaSeconds, FlameRate, BurnAmount, bFront ? 0.03f : 0.04f, not bFront);
	TickParticlePool(M_Embers, M_EmberSprites, DeltaSeconds, EmberRate, BurnAmount, bFront ? 0.04f : 0.035f, not bFront);
	TickParticlePool(M_Smoke, M_SmokeSprites, DeltaSeconds, SmokeRate, BurnAmount, bFront ? 0.05f : 0.04f, not bFront);
}

namespace MissionDebriefParticles
{
	void InitParticle(FMissionDebriefParticle& P, const EMissionDebriefParticleKind Kind)
	{
		auto Rand = [](const float Range) { return (FMath::FRand() - 0.5f) * Range; };
		switch (Kind)
		{
		case EMissionDebriefParticleKind::Ember:
			P.MaxLife = 0.8f + FMath::FRand() * 1.4f;
			P.Velocity = FVector(Rand(0.25f), 0.25f + FMath::FRand() * 0.5f, Rand(0.25f));
			P.Wobble = 0.12f;
			break;
		case EMissionDebriefParticleKind::Flame:
			P.MaxLife = 0.25f + FMath::FRand() * 0.4f;
			P.Velocity = FVector(Rand(0.08f), 0.3f + FMath::FRand() * 0.45f, Rand(0.08f));
			P.Wobble = 0.06f;
			break;
		default:
			P.MaxLife = 1.8f + FMath::FRand() * 1.8f;
			P.Velocity = FVector(Rand(0.12f) + 0.06f, 0.2f + FMath::FRand() * 0.2f, Rand(0.12f));
			P.Wobble = 0.08f;
			break;
		}
		P.Seed = FMath::FRand() * 9.f;
		P.SizeScale = 0.75f + FMath::FRand() * 0.6f;
		P.Life = 0.f;
	}

	/** Size (stage units) and RGBA of a live particle at age fraction A. */
	void Look(const FMissionDebriefParticle& P, const EMissionDebriefParticleKind Kind, const float A, float& OutSize,
	          FLinearColor& OutColor)
	{
		switch (Kind)
		{
		case EMissionDebriefParticleKind::Ember:
			OutSize = 0.028f;
			OutColor = FLinearColor(1.6f, 0.35f + 0.5f * (1.f - A), 0.08f, 1.f - A);
			return;
		case EMissionDebriefParticleKind::Flame:
		{
			const float Q = FMath::Min(1.f, A * 1.5f);
			OutSize = (0.06f + 0.11f * FMath::Sin(FMath::Min(1.f, A * 1.3f) * UE_PI)) * P.SizeScale;
			OutColor = FLinearColor(1.5f, FMath::Lerp(1.f, 0.3f, Q), FMath::Lerp(0.45f, 0.04f, FMath::Min(1.f, A * 2.2f)),
			                        (1.f - A) * 0.85f);
			return;
		}
		default:
			OutSize = (0.16f + 0.75f * A) * P.SizeScale;
			OutColor = FLinearColor(0.07f, 0.062f, 0.055f, 0.4f * FMath::Min(1.f, A * 5.f) * (1.f - A));
			return;
		}
	}
}

void AMissionDebriefStage::TickParticlePool(FMissionDebriefParticlePool& Pool, UInstancedStaticMeshComponent* Sprites,
                                            const float DeltaSeconds, const float SpawnRate, const float BurnAmount,
                                            const float Tolerance, const bool bUseWordBox)
{
	const int32 Count = Pool.Particles.Num();
	if (Count == 0)
	{
		return;
	}
	Pool.SpawnAccumulator += SpawnRate * DeltaSeconds;
	while (Pool.SpawnAccumulator >= 1.f)
	{
		Pool.SpawnAccumulator -= 1.f;
		FVector SpawnAt;
		if (not SampleBurnEdge(BurnAmount, Tolerance, bUseWordBox, SpawnAt))
		{
			continue;
		}
		FMissionDebriefParticle& Particle = Pool.Particles[Pool.NextIndex];
		Pool.NextIndex = (Pool.NextIndex + 1) % Count;
		MissionDebriefParticles::InitParticle(Particle, Pool.Kind);
		Particle.Position = SpawnAt;
	}
	for (FMissionDebriefParticle& Particle : Pool.Particles)
	{
		if (Particle.Life >= Particle.MaxLife)
		{
			continue;
		}
		Particle.Life += DeltaSeconds;
		constexpr float WobbleFrequency = 5.f;
		Particle.Position.X += (Particle.Velocity.X + FMath::Sin(Particle.Life * WobbleFrequency + Particle.Seed) * Particle.Wobble) * DeltaSeconds;
		Particle.Position.Y += Particle.Velocity.Y * DeltaSeconds;
		Particle.Position.Z += Particle.Velocity.Z * DeltaSeconds;
	}
	UpdateSpriteInstances(Pool, Sprites);
}

bool AMissionDebriefStage::SampleBurnEdge(const float BurnAmount, const float Tolerance, const bool bUseWordBox,
                                          FVector& OutStagePosition) const
{
	using namespace MissionDebriefTimeline;
	constexpr int32 WordBoxTries = 48;
	constexpr int32 PageTries = 16;
	constexpr float SpawnLift = 0.015f;
	const int32 Tries = bUseWordBox ? WordBoxTries : PageTries;
	for (int32 Try = 0; Try < Tries; ++Try)
	{
		const float U = bUseWordBox ? WordBoxU + (FMath::FRand() - 0.5f) * 2.f * WordBoxHalfU : FMath::FRand();
		const float V = bUseWordBox ? WordBoxV + (FMath::FRand() - 0.5f) * 2.f * WordBoxHalfV : FMath::FRand();
		if (FMath::Abs(M_BurnField->SampleField(M_Outcome, U, V) - BurnAmount) < Tolerance)
		{
			OutStagePosition = MissionDebriefMath::PaperToStage(U, V, SpawnLift);
			return true;
		}
	}
	return false;
}

void AMissionDebriefStage::UpdateSpriteInstances(const FMissionDebriefParticlePool& Pool,
                                                 UInstancedStaticMeshComponent* Sprites) const
{
	using namespace MissionDebriefSpace;
	const FQuat CameraQuat = M_Camera->GetComponentQuat();
	// The quad's normal (+Z) points back at the camera; its X runs along the camera's right.
	const FQuat Facing = FRotationMatrix::MakeFromZX(-CameraQuat.GetForwardVector(), CameraQuat.GetRightVector()).ToQuat();
	TArray<FTransform> Transforms;
	Transforms.Reserve(Pool.Particles.Num());
	float CustomData[MissionDebriefAssets::SpriteCustomDataFloats];
	for (int32 Index = 0; Index < Pool.Particles.Num(); ++Index)
	{
		const FMissionDebriefParticle& Particle = Pool.Particles[Index];
		float Size = 0.f;
		FLinearColor Color = FLinearColor::Transparent;
		if (Particle.Life < Particle.MaxLife)
		{
			MissionDebriefParticles::Look(Particle, Pool.Kind, FMath::Min(1.f, Particle.Life / Particle.MaxLife), Size, Color);
		}
		const float ScaleXY = Size * UnitToCm / MissionDebriefAssets::SpritePlaneSizeCm;
		Transforms.Add(FTransform(Facing, ToUnreal(Particle.Position), FVector(ScaleXY, ScaleXY, 1.f)));
		CustomData[0] = Color.R * SpriteGain;
		CustomData[1] = Color.G * SpriteGain;
		CustomData[2] = Color.B * SpriteGain;
		CustomData[3] = Color.A;
		Sprites->SetCustomData(Index, MakeArrayView(CustomData, MissionDebriefAssets::SpriteCustomDataFloats), false);
	}
	Sprites->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
}

bool AMissionDebriefStage::GetIsValidViewer() const
{
	if (M_Viewer.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Viewer", "GetIsValidViewer", this);
	return false;
}

bool AMissionDebriefStage::GetIsValidBurnField() const
{
	if (IsValid(M_BurnField) && M_BurnField->GetHasGrid(M_Outcome))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_BurnField", "GetIsValidBurnField", this);
	return false;
}
