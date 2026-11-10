// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CinematicTranscriptSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UCinematicTranscriptSubsystem* UCinematicTranscriptSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* const GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UCinematicTranscriptSubsystem>() : nullptr;
}

const FCinematicTranscriptRow* UCinematicTranscriptSubsystem::FindTranscript(
	const TSoftObjectPtr<UDataTable>& Table, const USoundBase* VoiceLine)
{
	if (VoiceLine == nullptr || Table.IsNull())
	{
		return nullptr;
	}
	UDataTable* const LoadedTable = LoadAndIndexTable(Table);
	if (LoadedTable == nullptr)
	{
		return nullptr;
	}
	const TMap<FSoftObjectPath, FName>* const Rows = M_RowsByVoiceLine.Find(Table.ToSoftObjectPath());
	const FName* const RowName = Rows != nullptr ? Rows->Find(FSoftObjectPath(VoiceLine)) : nullptr;
	if (RowName == nullptr)
	{
		return nullptr;
	}
	return LoadedTable->FindRow<FCinematicTranscriptRow>(*RowName, TEXT("UCinematicTranscriptSubsystem"));
}

UDataTable* UCinematicTranscriptSubsystem::LoadAndIndexTable(const TSoftObjectPtr<UDataTable>& Table)
{
	const FSoftObjectPath TablePath = Table.ToSoftObjectPath();
	if (const TObjectPtr<UDataTable>* const Loaded = M_Tables.Find(TablePath))
	{
		return Loaded->Get();
	}

	// Small table (a few hundred rows of words); loaded once on the first voice line that asks for it.
	UDataTable* const LoadedTable = Table.LoadSynchronous();
	M_Tables.Add(TablePath, LoadedTable);
	if (LoadedTable == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"UCinematicTranscriptSubsystem could not load the transcript table " + TablePath.ToString());
		return nullptr;
	}

	TMap<FSoftObjectPath, FName>& Rows = M_RowsByVoiceLine.FindOrAdd(TablePath);
	const FString Context(TEXT("UCinematicTranscriptSubsystem::LoadAndIndexTable"));
	for (const FName& RowName : LoadedTable->GetRowNames())
	{
		const FCinematicTranscriptRow* const Row = LoadedTable->FindRow<FCinematicTranscriptRow>(RowName, Context);
		if (Row != nullptr && not Row->VoiceLine.IsNull())
		{
			Rows.Add(Row->VoiceLine.ToSoftObjectPath(), RowName);
		}
	}
	return LoadedTable;
}
