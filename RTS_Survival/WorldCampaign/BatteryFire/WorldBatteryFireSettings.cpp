// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireSettings.h"

#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSWorldBatteryFireSettings, Log, All);

namespace WorldBatteryFireSettingsDefaults
{
	// Same as the battery's support radius in UWorldBuildingSettings.
	constexpr float Range = 3000.f;
	constexpr int32 MissionsPerTurn = 1;
	constexpr int32 EntriesDestroyedPerMission = 2;

	// 0.4 lead-in + 4 x 0.35 stagger + 1.6 flight + 0.6 linger = 4 seconds.
	constexpr int32 ShellCount = 5;
	constexpr float FlightSeconds = 1.6f;
	constexpr float ShellStaggerSeconds = 0.35f;
	constexpr float LeadInSeconds = 0.4f;
	constexpr float LingerSeconds = 0.6f;
	constexpr float ArcHeightFactor = 0.25f;
	constexpr float ImpactScatterRadius = 20.f;
	// The battery piece is about 20 units high and stands 6 units above the node.
	constexpr float MuzzleHeightOffset = 26.f;

	constexpr float CameraMoveSeconds = 0.4f;
	constexpr float CameraZoomPerDistance = 0.9f;
	constexpr float CameraZoomPadding = 500.f;

	// The division Blueprint's own piece is about 34 x 68 units; a build site plate is 58 wide. Six tanks in two rows
	// of three then cover about 58 x 50 units.
	constexpr float TankPieceScale = 1.f;
	constexpr float TankPieceTargetLength = 22.f;
	constexpr float FormationColumnSpacing = 18.f;
	constexpr float FormationRowSpacing = 28.f;
	constexpr int32 FormationMaxPerRow = 3;
	constexpr float FormationPlatePadding = 26.f;
	constexpr float FormationPlateHeight = 4.f;
	const TCHAR* const FormationPlatePath = TEXT(
		"/Game/RTS_Survival/WorldCampaign/Divisions/TankPieces/SM_DivisionPlate.SM_DivisionPlate");

	// The game's effects are made for units about 50 times the size of a map piece.
	constexpr float MuzzleEffectScale = 0.05f;
	constexpr float TrailEffectScale = 0.05f;
	constexpr float TrailShellWidth = 2.f;
	constexpr float ImpactEffectScale = 0.04f;
	constexpr float TankExplosionEffectScale = 0.04f;
	constexpr float WreckFireEffectScale = 0.04f;

	const TCHAR* const TankPieceFolder = TEXT("/Game/RTS_Survival/WorldCampaign/Divisions/TankPieces/");
	const TCHAR* const TankPiecePrefix = TEXT("SM_DivisionTank_");
	const TCHAR* const TankWreckPrefix = TEXT("SM_DivisionTankWreck_");
	// Exists today; shown until the division tank meshes are imported.
	const TCHAR* const FallbackTankPiecePath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/TrainingPreviews/SM_TrainingPreview_T34E.SM_TrainingPreview_T34E");
	const TCHAR* const MuzzleEffectPath = TEXT(
		"/Game/RTS_Survival/Blueprints/ProjectilesAndShells/FireWeaponVfxBase/NS_MuzzleFlash_Arty_1.NS_MuzzleFlash_Arty_1");
	const TCHAR* const TrailEffectPath = TEXT(
		"/Game/RTS_Survival/Blueprints/ProjectilesAndShells/Projectiles/NS_75mmShell.NS_75mmShell");
	const TCHAR* const ImpactEffectPath = TEXT(
		"/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Howitzer/NS_HowitzerImpact_Regular.NS_HowitzerImpact_Regular");
	const TCHAR* const TankExplosionEffectPath = TEXT(
		"/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/NS_Expl_Tank_1.NS_Expl_Tank_1");
	const TCHAR* const WreckFireEffectPath = TEXT(
		"/Game/RTS_Survival/SpecialEffects/M5VFXVOL2/Niagara/Reference/Fireloop/RTS_EngineFire_0.RTS_EngineFire_0");
	const TCHAR* const LaunchSoundPath = TEXT(
		"/Game/RTS_Survival/Sounds/WeaponSounds/HeavyCaliber/Brummbar/S_BrummbarFire.S_BrummbarFire");
	const TCHAR* const DescentSoundPath = TEXT(
		"/Game/RTS_Survival/Sounds/WeaponSounds/Artillery/Descent/Artillery_Descent.Artillery_Descent");
	const TCHAR* const ImpactSoundPath = TEXT(
		"/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Metal/Cue_Artillery_MetalImpact.Cue_Artillery_MetalImpact");
	const TCHAR* const TankExplosionSoundPath = TEXT(
		"/Game/RTS_Survival/Sounds/Explosions/TankExplosion/Cue_TankExplosionLarge.Cue_TankExplosionLarge");

	template <typename AssetType>
	TSoftObjectPtr<AssetType> MakeSoftAsset(const TCHAR* AssetPath)
	{
		return TSoftObjectPtr<AssetType>(FSoftObjectPath(AssetPath));
	}

	TSoftObjectPtr<UStaticMesh> MakeTankMesh(const TCHAR* Prefix, const TCHAR* TankName)
	{
		const FString AssetName = FString(Prefix) + TankName;
		return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString(TankPieceFolder) + AssetName + TEXT(".") + AssetName));
	}
}

UWorldBatteryFireSettings::UWorldBatteryFireSettings()
{
	namespace Defaults = WorldBatteryFireSettingsDefaults;
	Range = Defaults::Range;
	MissionsPerTurn = Defaults::MissionsPerTurn;
	EntriesDestroyedPerMission = Defaults::EntriesDestroyedPerMission;

	ShellCount = Defaults::ShellCount;
	FlightSeconds = Defaults::FlightSeconds;
	ShellStaggerSeconds = Defaults::ShellStaggerSeconds;
	LeadInSeconds = Defaults::LeadInSeconds;
	LingerSeconds = Defaults::LingerSeconds;
	ArcHeightFactor = Defaults::ArcHeightFactor;
	ImpactScatterRadius = Defaults::ImpactScatterRadius;
	MuzzleSocketName = TEXT("Muzzle");
	MuzzleHeightOffset = Defaults::MuzzleHeightOffset;

	bFrameGunAndTarget = true;
	bReturnCameraAfterShow = true;
	CameraMoveSeconds = Defaults::CameraMoveSeconds;
	CameraZoomPerDistance = Defaults::CameraZoomPerDistance;
	CameraZoomPadding = Defaults::CameraZoomPadding;

	TankPieceScale = Defaults::TankPieceScale;
	TankPieceTargetLength = Defaults::TankPieceTargetLength;
	TankPieceYawOffset = 0.f;
	FormationColumnSpacing = Defaults::FormationColumnSpacing;
	FormationRowSpacing = Defaults::FormationRowSpacing;
	FormationMaxPerRow = Defaults::FormationMaxPerRow;

	const TArray<TPair<ETankSubtype, const TCHAR*>> TankNames = {
		{ETankSubtype::Tank_T26, TEXT("T26")},
		{ETankSubtype::Tank_BT7, TEXT("BT7")},
		{ETankSubtype::Tank_T34_76, TEXT("T34_76")},
		{ETankSubtype::Tank_T34_85, TEXT("T34_85")},
		{ETankSubtype::Tank_IS_1, TEXT("IS1")},
		{ETankSubtype::Tank_IS_2, TEXT("IS2")},
		{ETankSubtype::Tank_IS_3, TEXT("IS3")},
		{ETankSubtype::Tank_KV_1, TEXT("KV1")}
	};
	for (const TPair<ETankSubtype, const TCHAR*>& TankName : TankNames)
	{
		TankPieceMeshes.Add(TankName.Key, Defaults::MakeTankMesh(Defaults::TankPiecePrefix, TankName.Value));
		TankWreckMeshes.Add(TankName.Key, Defaults::MakeTankMesh(Defaults::TankWreckPrefix, TankName.Value));
	}
	FallbackTankPieceMesh = Defaults::MakeSoftAsset<UStaticMesh>(Defaults::FallbackTankPiecePath);
	FormationPlateMesh = Defaults::MakeSoftAsset<UStaticMesh>(Defaults::FormationPlatePath);
	FormationPlatePadding = Defaults::FormationPlatePadding;
	FormationPlateHeight = Defaults::FormationPlateHeight;

	ShellMeshScale = 1.f;
	MuzzleEffect = Defaults::MakeSoftAsset<UNiagaraSystem>(Defaults::MuzzleEffectPath);
	MuzzleEffectScale = Defaults::MuzzleEffectScale;
	TrailEffect = Defaults::MakeSoftAsset<UNiagaraSystem>(Defaults::TrailEffectPath);
	TrailEffectScale = Defaults::TrailEffectScale;
	TrailShellWidth = Defaults::TrailShellWidth;
	TrailShellColor = FLinearColor(1.f, 0.75f, 0.35f);
	ImpactEffect = Defaults::MakeSoftAsset<UNiagaraSystem>(Defaults::ImpactEffectPath);
	ImpactEffectScale = Defaults::ImpactEffectScale;
	TankExplosionEffect = Defaults::MakeSoftAsset<UNiagaraSystem>(Defaults::TankExplosionEffectPath);
	TankExplosionEffectScale = Defaults::TankExplosionEffectScale;
	WreckFireEffect = Defaults::MakeSoftAsset<UNiagaraSystem>(Defaults::WreckFireEffectPath);
	WreckFireEffectScale = Defaults::WreckFireEffectScale;

	LaunchSound = Defaults::MakeSoftAsset<USoundBase>(Defaults::LaunchSoundPath);
	DescentSound = Defaults::MakeSoftAsset<USoundBase>(Defaults::DescentSoundPath);
	ImpactSound = Defaults::MakeSoftAsset<USoundBase>(Defaults::ImpactSoundPath);
	TankExplosionSound = Defaults::MakeSoftAsset<USoundBase>(Defaults::TankExplosionSoundPath);
}

FWorldBatteryFireNumbers UWorldBatteryFireSettings::GetNumbers() const
{
	FWorldBatteryFireNumbers Numbers;
	Numbers.Range = Range;
	Numbers.MissionsPerTurn = MissionsPerTurn;
	Numbers.EntriesDestroyedPerMission = EntriesDestroyedPerMission;
	return Numbers;
}

UObject* UWorldBatteryFireSettings::LoadOptionalAsset(const FSoftObjectPath& AssetPath, const TCHAR* SettingName)
{
	// Game thread only: the show and the formations are built there.
	static TSet<FString> LoggedMissingKeys;
	if (AssetPath.IsNull())
	{
		const FString UnsetKey = FString(TEXT("unset:")) + SettingName;
		if (not LoggedMissingKeys.Contains(UnsetKey))
		{
			LoggedMissingKeys.Add(UnsetKey);
			UE_LOG(LogRTSWorldBatteryFireSettings, Log, TEXT("BATTERYFIRE setting %s is not set; skipped."), SettingName);
		}
		return nullptr;
	}

	const FString AssetPathString = AssetPath.ToString();
	if (LoggedMissingKeys.Contains(AssetPathString))
	{
		return nullptr;
	}

	// TryLoad on a missing package reports a load error every time, so ask the package index first.
	UObject* LoadedAsset = FPackageName::DoesPackageExist(AssetPath.GetLongPackageName()) ? AssetPath.TryLoad() : nullptr;
	if (not IsValid(LoadedAsset))
	{
		LoggedMissingKeys.Add(AssetPathString);
		UE_LOG(LogRTSWorldBatteryFireSettings, Warning, TEXT("BATTERYFIRE setting %s: asset %s not found; skipped."),
		       SettingName, *AssetPathString);
		return nullptr;
	}

	return LoadedAsset;
}
