#include "WreckDebrisSettings.h"

#include "Engine/StaticMesh.h"

namespace WreckDebrisDefaults
{
	const TCHAR* const ArmourChunkPaths[] = {
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/SM_Derbis_1.SM_Derbis_1"),
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/SM_Derbis_2.SM_Derbis_2"),
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/SM_Derbis_3.SM_Derbis_3"),
	};
	const TCHAR* const ShellPaths[] = {
		TEXT("/Game/MillitaryBase/Meshes/SM_TankAmmo_A.SM_TankAmmo_A"),
		TEXT("/Game/MillitaryBase/Meshes/SM_TankAmmo_C.SM_TankAmmo_C"),
		TEXT("/Game/MillitaryBase/Meshes/SM_TankAmmo_E.SM_TankAmmo_E"),
	};
	const TCHAR* const AmmoBoxPaths[] = {
		TEXT("/Game/MillitaryBase/Meshes/SM_ammoBox1.SM_ammoBox1"),
		TEXT("/Game/MillitaryBase/Meshes/SM_ammoBox3.SM_ammoBox3"),
	};
	const TCHAR* const JerryCanPath = TEXT("/Game/Hangar/Models/SM_JerryCan_01.SM_JerryCan_01");
	// Real-size engine deck parts (Source/Tools/EngineFireDeath), burnt-metal material of their own.
	const TCHAR* const DeckPlatePaths[] = {
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_Plate_A.SM_EngineDeck_Plate_A"),
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_Plate_B.SM_EngineDeck_Plate_B"),
	};
	const TCHAR* const DeckGrillePaths[] = {
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_Grille.SM_EngineDeck_Grille"),
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_HatchLid.SM_EngineDeck_HatchLid"),
	};
	const TCHAR* const DeckPartPaths[] = {
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_Muffler.SM_EngineDeck_Muffler"),
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_Fan.SM_EngineDeck_Fan"),
	};

	// The chunk meshes are ~8 cm; scaled up they become 35-80 cm armour fragments painted like the hull.
	const FVector2D ArmourChunkScale(8.0, 14.0);
	constexpr float ArmourChunkWeight = 2.f;
	const FVector2D ShellScale(0.9, 1.1);
	constexpr float ShellWeight = 1.f;
	const FVector2D AmmoBoxScale(0.45, 0.6);
	constexpr float AmmoBoxWeight = 0.6f;
	const FVector2D JerryCanScale(0.9, 1.0);
	constexpr float JerryCanWeight = 0.5f;
	const FVector2D DeckPartScale(0.85, 1.15);
	constexpr float DeckPlateWeight = 2.f;
	constexpr float DeckGrilleWeight = 1.2f;
	constexpr float DeckPartWeight = 0.7f;
	constexpr float DeckArmourChunkWeight = 1.f;
	constexpr float DeckJerryCanWeight = 0.4f;

	// The fuel blast lifts the deck plates about as high as the ammo blast throws its chunks, mostly out over the
	// engine side of the hull.
	const FIntPoint EngineFirePieceCountRange(6, 11);
	const FVector2D EngineFireUpSpeedRange(700.0, 1300.0);
	const FVector2D EngineFireSideSpeedRange(150.0, 550.0);
	constexpr float EngineFireDirectionBias = 0.7f;
	constexpr float EngineFireDirectionSpreadDegrees = 55.f;
	constexpr float EngineFireScatterRadius = 70.f;
	constexpr int32 EngineFireBurningPieceCount = 1;
	constexpr float EngineFireBurningPieceLifeTime = 6.f;

	void AddPieces(TArray<FWreckDebrisPieceType>& OutPieces, const TArrayView<const TCHAR* const> Paths,
	               const FVector2D& ScaleRange, const float Weight, const bool bUseHullMaterial)
	{
		for (const TCHAR* const Path : Paths)
		{
			FWreckDebrisPieceType Piece;
			Piece.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path));
			Piece.ScaleRange = ScaleRange;
			Piece.Weight = Weight;
			Piece.bUseWreckHullMaterial = bUseHullMaterial;
			OutPieces.Add(Piece);
		}
	}
}

UWreckDebrisSettings::UWreckDebrisSettings()
{
	using namespace WreckDebrisDefaults;
	const TCHAR* const JerryCan[] = {JerryCanPath};
	TArray<FWreckDebrisPieceType>& CookOffPieces = AmmoCookOffBurst.PieceTypes;
	AddPieces(CookOffPieces, MakeArrayView(ArmourChunkPaths), ArmourChunkScale, ArmourChunkWeight, true);
	AddPieces(CookOffPieces, MakeArrayView(ShellPaths), ShellScale, ShellWeight, false);
	AddPieces(CookOffPieces, MakeArrayView(AmmoBoxPaths), AmmoBoxScale, AmmoBoxWeight, false);
	AddPieces(CookOffPieces, MakeArrayView(JerryCan), JerryCanScale, JerryCanWeight, false);

	TArray<FWreckDebrisPieceType>& DeckPieces = EngineFireBurst.PieceTypes;
	AddPieces(DeckPieces, MakeArrayView(DeckPlatePaths), DeckPartScale, DeckPlateWeight, false);
	AddPieces(DeckPieces, MakeArrayView(DeckGrillePaths), DeckPartScale, DeckGrilleWeight, false);
	AddPieces(DeckPieces, MakeArrayView(DeckPartPaths), DeckPartScale, DeckPartWeight, false);
	AddPieces(DeckPieces, MakeArrayView(ArmourChunkPaths), ArmourChunkScale, DeckArmourChunkWeight, true);
	AddPieces(DeckPieces, MakeArrayView(JerryCan), JerryCanScale, DeckJerryCanWeight, false);
	EngineFireBurst.PieceCountRange = EngineFirePieceCountRange;
	EngineFireBurst.UpSpeedRange = EngineFireUpSpeedRange;
	EngineFireBurst.SideSpeedRange = EngineFireSideSpeedRange;
	EngineFireBurst.DirectionBias = EngineFireDirectionBias;
	EngineFireBurst.DirectionSpreadDegrees = EngineFireDirectionSpreadDegrees;
	EngineFireBurst.SpawnScatterRadius = EngineFireScatterRadius;
	EngineFireBurst.BurningPieceCount = EngineFireBurningPieceCount;
	EngineFireBurst.BurningPieceFireLifeTime = EngineFireBurningPieceLifeTime;
}
