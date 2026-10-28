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

	// The chunk meshes are ~8 cm; scaled up they become 35-80 cm armour fragments painted like the hull.
	const FVector2D ArmourChunkScale(8.0, 14.0);
	constexpr float ArmourChunkWeight = 2.f;
	const FVector2D ShellScale(0.9, 1.1);
	constexpr float ShellWeight = 1.f;
	const FVector2D AmmoBoxScale(0.45, 0.6);
	constexpr float AmmoBoxWeight = 0.6f;
	const FVector2D JerryCanScale(0.9, 1.0);
	constexpr float JerryCanWeight = 0.5f;

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
	AddPieces(PieceTypes, MakeArrayView(ArmourChunkPaths), ArmourChunkScale, ArmourChunkWeight, true);
	AddPieces(PieceTypes, MakeArrayView(ShellPaths), ShellScale, ShellWeight, false);
	AddPieces(PieceTypes, MakeArrayView(AmmoBoxPaths), AmmoBoxScale, AmmoBoxWeight, false);
	const TCHAR* const JerryCan[] = {JerryCanPath};
	AddPieces(PieceTypes, MakeArrayView(JerryCan), JerryCanScale, JerryCanWeight, false);
}
