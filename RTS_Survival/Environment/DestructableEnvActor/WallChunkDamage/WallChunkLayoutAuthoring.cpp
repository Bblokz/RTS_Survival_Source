// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring of wall chunk layouts (Docs/Environment/WallChunkDamage.md).
// RTS.WallChunks.ApplyLayout Mesh=<chunk mesh> Json=<layout json> Debris=<package folder> Damaged=<mat0>|<mat1>
//   Reads the layout Tools/WallChunks/make_wall_chunks.py wrote next to the FBX, and stores it on the chunk mesh as a
//   UWallChunkLayout with the debris pieces (<folder>/<piece name>) and the masked materials per slot.
// RTS.WallChunks.FinishMaskedMaster Material=<masked copy of an opaque master>
//   Clears the copied constant on the opacity mask pin so the chunk mask compiles.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WallChunkLayout.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallChunkAuthoring, Log, All);

namespace WallChunkLayoutAuthoring
{
	template <typename TAsset>
	TAsset* LoadAssetByPackage(const FString& PackagePath)
	{
		if (PackagePath.IsEmpty())
		{
			return nullptr;
		}
		const FString ObjectPath = PackagePath.Contains(TEXT("."))
			                           ? PackagePath
			                           : PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
		return LoadObject<TAsset>(nullptr, *ObjectPath);
	}

	bool SaveAsset(UObject& Asset)
	{
		UPackage* const Package = Asset.GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(),
		                                                                 FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Asset, *FileName, SaveArgs);
	}

	FVector ReadVector(const TSharedPtr<FJsonObject>& Object, const FString& Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (not Object->TryGetArrayField(Field, Values) || Values->Num() != 3)
		{
			return FVector::ZeroVector;
		}
		return FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
	}

	bool ReadChunks(const TSharedPtr<FJsonObject>& Root, TArray<FWallChunk>& OutChunks)
	{
		const TArray<TSharedPtr<FJsonValue>>* ChunkValues = nullptr;
		if (not Root->TryGetArrayField(TEXT("chunks"), ChunkValues))
		{
			return false;
		}
		OutChunks.SetNum(ChunkValues->Num());
		for (const TSharedPtr<FJsonValue>& Value : *ChunkValues)
		{
			const TSharedPtr<FJsonObject> ChunkObject = Value->AsObject();
			const int32 ChunkIndex = static_cast<int32>(ChunkObject->GetNumberField(TEXT("index")));
			if (not OutChunks.IsValidIndex(ChunkIndex))
			{
				return false;
			}
			FWallChunk& Chunk = OutChunks[ChunkIndex];
			Chunk.MeshSpaceCentre = ReadVector(ChunkObject, TEXT("centre"));
			Chunk.Area = static_cast<float>(ChunkObject->GetNumberField(TEXT("area")));
			Chunk.DebrisPiece = static_cast<int32>(ChunkObject->GetNumberField(TEXT("debris")));
			Chunk.DebrisScale = static_cast<float>(ChunkObject->GetNumberField(TEXT("debris_scale")));
		}
		return true;
	}

	bool ReadDebrisPieces(const TSharedPtr<FJsonObject>& Root, const FString& DebrisFolder,
	                      TArray<UStaticMesh*>& OutPieces)
	{
		const TArray<TSharedPtr<FJsonValue>>* PieceValues = nullptr;
		if (not Root->TryGetArrayField(TEXT("debris"), PieceValues))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *PieceValues)
		{
			const FString PieceName = Value->AsObject()->GetStringField(TEXT("mesh"));
			UStaticMesh* const Piece = LoadAssetByPackage<UStaticMesh>(DebrisFolder / PieceName);
			if (not IsValid(Piece))
			{
				UE_LOG(LogWallChunkAuthoring, Error, TEXT("RTS_WALL_CHUNK_AUTHORING missing debris piece %s/%s"),
				       *DebrisFolder, *PieceName);
				return false;
			}
			OutPieces.Add(Piece);
		}
		return true;
	}

	TArray<UMaterialInterface*> ReadDamagedMaterials(const FString& DamagedList)
	{
		TArray<FString> Paths;
		DamagedList.ParseIntoArray(Paths, TEXT("|"));
		TArray<UMaterialInterface*> Materials;
		for (const FString& Path : Paths)
		{
			Materials.Add(LoadAssetByPackage<UMaterialInterface>(Path));
		}
		return Materials;
	}

	bool ApplyLayoutToMesh(UStaticMesh& Mesh, const TSharedPtr<FJsonObject>& Root, const FString& DebrisFolder,
	                       const FString& DamagedList, int32& OutChunkCount)
	{
		TArray<FWallChunk> Chunks;
		TArray<UStaticMesh*> Pieces;
		if (not ReadChunks(Root, Chunks) || not ReadDebrisPieces(Root, DebrisFolder, Pieces))
		{
			return false;
		}
		OutChunkCount = Chunks.Num();
		if (Chunks.Num() > WallChunkMask::MaxChunksPerMesh)
		{
			UE_LOG(LogWallChunkAuthoring, Error, TEXT("RTS_WALL_CHUNK_AUTHORING %d chunks, at most %d fit the mask"),
			       Chunks.Num(), WallChunkMask::MaxChunksPerMesh);
			return false;
		}
		const FVector WallMin = ReadVector(Root, TEXT("wall_min"));
		const FVector WallMax = ReadVector(Root, TEXT("wall_max"));
		const float HalfThickness = static_cast<float>(FMath::Abs(WallMax.Y - WallMin.Y) * 0.5);
		UWallChunkLayout* Layout = Mesh.GetAssetUserData<UWallChunkLayout>();
		if (not IsValid(Layout))
		{
			Layout = NewObject<UWallChunkLayout>(&Mesh, NAME_None, RF_Public | RF_Transactional);
			Mesh.AddAssetUserData(Layout);
		}
		Mesh.Modify();
		Layout->SetLayout(Chunks, Pieces, ReadDamagedMaterials(DamagedList), HalfThickness);
		return SaveAsset(Mesh);
	}

	void ApplyLayout(const TArray<FString>& Args)
	{
		const FString Joined = FString::Join(Args, TEXT(" "));
		FString MeshPath;
		FString JsonPath;
		FString DebrisFolder;
		FString DamagedList;
		FParse::Value(*Joined, TEXT("Mesh="), MeshPath);
		FParse::Value(*Joined, TEXT("Json="), JsonPath);
		FParse::Value(*Joined, TEXT("Debris="), DebrisFolder);
		FParse::Value(*Joined, TEXT("Damaged="), DamagedList, false);
		UStaticMesh* const Mesh = LoadAssetByPackage<UStaticMesh>(MeshPath);
		FString JsonText;
		TSharedPtr<FJsonObject> Root;
		const bool bReadJson = FFileHelper::LoadFileToString(JsonText, *JsonPath)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root) && Root.IsValid();
		int32 ChunkCount = 0;
		const bool bApplied = IsValid(Mesh) && bReadJson
			&& ApplyLayoutToMesh(*Mesh, Root, DebrisFolder.IsEmpty() ? FPackageName::GetLongPackagePath(MeshPath)
			                                                           : DebrisFolder, DamagedList, ChunkCount);
		UE_LOG(LogWallChunkAuthoring, Display, TEXT("RTS_WALL_CHUNK_AUTHORING apply RESULT %s mesh=%s chunks=%d json=%d"),
		       bApplied ? TEXT("DONE") : TEXT("FAIL"), *MeshPath, ChunkCount, bReadJson ? 1 : 0);
	}

	/**
	 * @brief A masked copy of an opaque master keeps the master's constant on the opacity mask pin, which wins over
	 * the connected chunk mask, and keeps bCanMaskedBeAssumedOpaque until PostEditChange (AddOnArmorPanels.md).
	 */
	void FinishMaskedMaster(const TArray<FString>& Args)
	{
		const FString Joined = FString::Join(Args, TEXT(" "));
		FString MaterialPath;
		FParse::Value(*Joined, TEXT("Material="), MaterialPath);
		UMaterial* const Material = LoadAssetByPackage<UMaterial>(MaterialPath);
		bool bDone = false;
		if (IsValid(Material))
		{
			Material->PreEditChange(nullptr);
			Material->GetEditorOnlyData()->OpacityMask.UseConstant = false;
			Material->BlendMode = BLEND_Masked;
			Material->bUsedWithNiagaraMeshParticles = true;
			Material->PostEditChange();
			bDone = not Material->bCanMaskedBeAssumedOpaque && SaveAsset(*Material);
		}
		UE_LOG(LogWallChunkAuthoring, Display, TEXT("RTS_WALL_CHUNK_AUTHORING masked RESULT %s material=%s assumedOpaque=%d"),
		       bDone ? TEXT("DONE") : TEXT("FAIL"), *MaterialPath,
		       IsValid(Material) && Material->bCanMaskedBeAssumedOpaque ? 1 : 0);
	}

	FAutoConsoleCommand FinishMaskedMasterCommand(
		TEXT("RTS.WallChunks.FinishMaskedMaster"),
		TEXT("Makes a masked copy of an opaque master use its connected opacity mask. Args: Material=<material>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FinishMaskedMaster));

	FAutoConsoleCommand ApplyLayoutCommand(
		TEXT("RTS.WallChunks.ApplyLayout"),
		TEXT("Stores a wall chunk layout on a chunk mesh. Args: Mesh=<chunk mesh> Json=<layout json> "
			"[Debris=<package folder of the debris pieces, default the mesh's folder>] [Damaged=<masked mat slot 0>|<slot 1>]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ApplyLayout));
}

#endif // WITH_EDITOR
