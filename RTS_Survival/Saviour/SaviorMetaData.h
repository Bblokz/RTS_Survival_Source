//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2025 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SaviorTypes.h"
#include "Runtime/Engine/Classes/GameFramework/SaveGame.h"
#include "Runtime/Engine/Public/WorldPartition/DataLayer/DataLayer.h"

#include "SaviorMetaData.generated.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UCLASS(ClassGroup = Synaptech, Category = "Synaptech", Blueprintable, BlueprintType, meta = (DisplayName = "[SAVIOR] Slot Meta"))
class RTS_SURVIVAL_API UMetaSlot : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(Category = "Meta Data", EditDefaultsOnly, BlueprintReadWrite)
	float Progress;
	UPROPERTY(Category = "Meta Data", EditDefaultsOnly, BlueprintReadWrite)
	int32 PlayTime;
	UPROPERTY(Category = "Meta Data", EditDefaultsOnly, BlueprintReadWrite)
	FString Chapter;
	UPROPERTY(Category = "Meta Data", EditDefaultsOnly, BlueprintReadWrite)
	int32 PlayerLevel;
	UPROPERTY(Category = "Meta Data", EditDefaultsOnly, BlueprintReadWrite)
	FString PlayerName;
	UPROPERTY(Category = "Meta Data", EditDefaultsOnly, BlueprintReadWrite)
	FString SaveLocation;
	UPROPERTY(Category = "Meta Data", VisibleDefaultsOnly, BlueprintReadWrite)
	FDateTime SaveDate;

public:
	UMetaSlot()
		: Progress(0.f)
		, PlayTime(0)
		, Chapter(TEXT(""))
		, PlayerLevel(0)
		, PlayerName(TEXT(""))
		, SaveLocation(TEXT(""))
		, SaveDate(FDateTime::Now())
	{
	}

public:
	friend inline FArchive& operator<<(FArchive& AR, UMetaSlot& SD)
	{
		AR << SD.Chapter;
		AR << SD.Progress;
		AR << SD.PlayTime;
		AR << SD.SaveDate;
		AR << SD.PlayerName;
		AR << SD.PlayerLevel;
		AR << SD.SaveLocation;
		return AR;
	}
};

UCLASS(ClassGroup = Synaptech, Category = "Synaptech", Blueprintable, BlueprintType, meta = (DisplayName = "[SAVIOR] Slot Data"))
class RTS_SURVIVAL_API UDataSlot : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FSaviorRecord> Records;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FMaterialRecord> Materials;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FChaosGeometryRecord> ChaosObjects;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, EDataLayerRuntimeState> DataLayers;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FFeatureRecord> GameFeatures;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FMutableRecord> Mutables;

public:
	UDataSlot()
		: Records(TMap<FName, FSaviorRecord>{})
		, Materials(TMap<FName, FMaterialRecord>{})
		, ChaosObjects(TMap<FName, FChaosGeometryRecord>{})
		, DataLayers(TMap<FName, EDataLayerRuntimeState>{})
		, GameFeatures(TMap<FName, FFeatureRecord>{})
		, Mutables(TMap<FName, FMutableRecord>{})
	{}

public:
	friend inline FArchive& operator<<(FArchive& AR, UDataSlot& DS)
	{
		AR << DS.Records;
		AR << DS.Materials;
		AR << DS.ChaosObjects;
		AR << DS.DataLayers;
		AR << DS.Mutables;
		return AR;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType, meta = (DisplayName = "Slot Meta"))
struct RTS_SURVIVAL_API FSlotMeta
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY(Category = "Meta Data", BlueprintReadOnly)
	float Progress;
	UPROPERTY(Category = "Meta Data", BlueprintReadOnly)
	int32 PlayTime;
	UPROPERTY(Category = "Meta Data", BlueprintReadOnly)
	FString Chapter;
	UPROPERTY(Category = "Meta Data", BlueprintReadOnly)
	int32 PlayerLevel;
	UPROPERTY(Category = "Meta Data", BlueprintReadOnly)
	FString PlayerName;
	UPROPERTY(Category = "Meta Data", BlueprintReadOnly)
	FDateTime SaveDate;
	UPROPERTY(Category = "Meta Data", BlueprintReadOnly)
	FString SaveLocation;
	//
	//
	FSlotMeta()
		: Progress(0.f)
		, PlayTime(0)
		, Chapter(TEXT(""))
		, PlayerLevel(0)
		, PlayerName(TEXT(""))
		, SaveDate(FDateTime::Now())
		, SaveLocation(TEXT(""))
	{
	}
	//
	FSlotMeta(UMetaSlot* MS)
		: Progress(0.f)
		, PlayTime(0)
		, Chapter(TEXT(""))
		, PlayerLevel(0)
		, PlayerName(TEXT(""))
		, SaveDate(FDateTime::Now())
		, SaveLocation(TEXT(""))
	{
		if (MS->IsValidLowLevelFast())
		{
			Chapter = MS->Chapter;
			Progress = MS->Progress;
			PlayTime = MS->PlayTime;
			SaveDate = MS->SaveDate;
			PlayerName = MS->PlayerName;
			PlayerLevel = MS->PlayerLevel;
			SaveLocation = MS->SaveLocation;
		}
	}
	//
	FSlotMeta(const UMetaSlot* MS)
		: Progress(0.f)
		, PlayTime(0)
		, Chapter(TEXT(""))
		, PlayerLevel(0)
		, PlayerName(TEXT(""))
		, SaveDate(FDateTime::Now())
		, SaveLocation(TEXT(""))
	{
		if (MS->IsValidLowLevelFast())
		{
			Chapter = MS->Chapter;
			Progress = MS->Progress;
			PlayTime = MS->PlayTime;
			SaveDate = MS->SaveDate;
			PlayerName = MS->PlayerName;
			PlayerLevel = MS->PlayerLevel;
			SaveLocation = MS->SaveLocation;
		}
	}
	//
	//
	inline FSlotMeta& operator=(const FSlotMeta& MS)
	{
		Chapter = MS.Chapter;
		Progress = MS.Progress;
		PlayTime = MS.PlayTime;
		SaveDate = MS.SaveDate;
		PlayerName = MS.PlayerName;
		PlayerLevel = MS.PlayerLevel;
		SaveLocation = MS.SaveLocation;
		return *this;
	}
	//
	inline FSlotMeta& operator=(UMetaSlot* MS)
	{
		Chapter = MS->Chapter;
		Progress = MS->Progress;
		PlayTime = MS->PlayTime;
		SaveDate = MS->SaveDate;
		PlayerName = MS->PlayerName;
		PlayerLevel = MS->PlayerLevel;
		SaveLocation = MS->SaveLocation;
		return *this;
	}
	//
	inline FSlotMeta& operator=(const UMetaSlot* MS)
	{
		Chapter = MS->Chapter;
		Progress = MS->Progress;
		PlayTime = MS->PlayTime;
		SaveDate = MS->SaveDate;
		PlayerName = MS->PlayerName;
		PlayerLevel = MS->PlayerLevel;
		SaveLocation = MS->SaveLocation;
		return *this;
	}
	//
	//
	friend inline uint32 GetTypeHash(const FSlotMeta& MS)
	{
		return FCrc::MemCrc32(&MS, sizeof(FSlotMeta));
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Slot Data"))
struct RTS_SURVIVAL_API FSlotData
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FSaviorRecord> Records;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FMaterialRecord> Materials;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FChaosGeometryRecord> ChaosObjects;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, EDataLayerRuntimeState> DataLayers;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FFeatureRecord> GameFeatures;
	UPROPERTY(Category = "Data", VisibleAnywhere)
	TMap<FName, FMutableRecord> Mutables;
	//
	//
	FSlotData()
		: Records(TMap<FName, FSaviorRecord>{})
		, Materials(TMap<FName, FMaterialRecord>{})
		, ChaosObjects(TMap<FName, FChaosGeometryRecord>{})
		, DataLayers(TMap<FName, EDataLayerRuntimeState>{})
		, GameFeatures(TMap<FName, FFeatureRecord>{})
		, Mutables(TMap<FName, FMutableRecord>{})
	{}
	//
	FSlotData(const UDataSlot* DS)
		: Records(TMap<FName, FSaviorRecord>{})
		, Materials(TMap<FName, FMaterialRecord>{})
		, ChaosObjects(TMap<FName, FChaosGeometryRecord>{})
		, DataLayers(TMap<FName, EDataLayerRuntimeState>{})
		, GameFeatures(TMap<FName, FFeatureRecord>{})
		, Mutables(TMap<FName, FMutableRecord>{})
	{
		if (DS->IsValidLowLevelFast())
		{
			Records.Append(DS->Records);
			Materials.Append(DS->Materials);
			DataLayers.Append(DS->DataLayers);
			ChaosObjects.Append(DS->ChaosObjects);
			GameFeatures.Append(DS->GameFeatures);
			Mutables.Append(DS->Mutables);
		}
	}
	//
	//
	inline FSlotData& operator=(const FSlotData& SD)
	{
		Records.Append(SD.Records);
		Materials.Append(SD.Materials);
		DataLayers.Append(SD.DataLayers);
		ChaosObjects.Append(SD.ChaosObjects);
		GameFeatures.Append(SD.GameFeatures);
		Mutables.Append(SD.Mutables);
		return *this;
	}
	//
	inline FSlotData& operator=(UDataSlot* DS)
	{
		Records.Append(DS->Records);
		Materials.Append(DS->Materials);
		DataLayers.Append(DS->DataLayers);
		ChaosObjects.Append(DS->ChaosObjects);
		GameFeatures.Append(DS->GameFeatures);
		Mutables.Append(DS->Mutables);
		return *this;
	}
	//
	inline FSlotData& operator=(const UDataSlot* DS)
	{
		Records.Append(DS->Records);
		Materials.Append(DS->Materials);
		DataLayers.Append(DS->DataLayers);
		ChaosObjects.Append(DS->ChaosObjects);
		GameFeatures.Append(DS->GameFeatures);
		Mutables.Append(DS->Mutables);
		return *this;
	}
	//
	//
	friend inline uint32 GetTypeHash(const FSlotData& SD)
	{
		return FCrc::MemCrc32(&SD, sizeof(FSlotData));
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
