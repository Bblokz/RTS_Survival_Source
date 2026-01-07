#include "BombBayData.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Weapons/BombComponent/BombActor/BombActor.h"
#include "RTS_Survival/Units/Aircraft/AircaftHelpers/FRTSAircraftHelpers.h"

void FBombBayEntry::InitBombInstancedIndex(const int32 Index)
{
	if (Index == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Attempted to init bomb entry with instance index INDEX_NONE");
		return;
	}
	HidableInstancedMeshIndex = Index;
}

bool FBombBayEntry::EnsureEntryHasSocketSet(const UObject* Owner) const
{
	if (SocketName == NAME_None)
	{
		const FString OwnerName = Owner ? Owner->GetName() : TEXT("NULL OWNER");
		RTSFunctionLibrary::ReportError("BombBayEntry is not valid as the SocketName is NAME_None."
			"\n Owner: " + OwnerName
			+ "\n Please set a valid SocketName for the BombBayEntry.");
		return false;
	}
	return true;
}

bool FBombBayEntry::EnsureEntryIsValid(const UObject* Owner) const
{
	if (SocketName == NAME_None || not IsValid(M_Bomb))
	{
		FString OwnerName = Owner ? Owner->GetName() : "NULL Owner";
		FString IsBombValid = M_Bomb ? M_Bomb->GetName() : "No Valid bomb pointer";
		FString SocketStatus = SocketName == NAME_None ? "Socket not initialized" : SocketName.ToString();
		RTSFunctionLibrary::ReportError(""
			"Bomb entry is not valid for: " + OwnerName +
			"\n With BombValidity: " + IsBombValid +
			"\n SocketStatus: " + SocketStatus);
		return false;
	}
	return true;
}

void FBombBayEntry::SetBombArmed(const bool bArmed)
{
	bIsArmed = bArmed;
}

void FBombBayEntry::SetBombActor(ABombActor* Bomb)
{
	if (not Bomb)
	{
		RTSFunctionLibrary::ReportError("Attempted to set null bomb for bomb entry!");
		return;
	}
	M_Bomb = Bomb;
}

void FBombBayEntry::SetBombInstancerIndex(const int32 Index)
{
	if (Index == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Attempted to set bomb entry with instance index INDEX_NONE");
		return;
	}
	HidableInstancedMeshIndex = Index;
}

int32 FBombBayData::GetBombsRemaining() const
{
	int32 Remaining = 0;
	for (const FBombBayEntry& BombEntry : TBombEntries)
	{
		if (BombEntry.IsBombArmed())
		{
			Remaining++;
		}
	}
	return Remaining;
}

int32 FBombBayData::GetMaxBombs() const
{
	return TBombEntries.Num();
}

bool FBombBayData::GetFirstArmedBomb(FBombBayEntry*& OutBombEntry)
{
	for (FBombBayEntry& BombEntry : TBombEntries)
	{
		if (BombEntry.IsBombArmed())
		{
			OutBombEntry = &BombEntry;
			return true;
		}
	}
	return false;
}

bool FBombBayData::EnsureBombBayIsProperlyInitialized(UObject* Owner)
{
	const FString OwnerName = Owner ? Owner->GetName() : TEXT("NULL OWNER");
	if (not IsValid(BombMesh))
	{
		RTSFunctionLibrary::ReportError("BombBayData is not properly initialized for Owner: " + OwnerName +
			"\n BombMesh is not valid. Please set a valid BombMesh.");
		return false;
	}
	for (auto EachEntry : TBombEntries)
	{
		if (not EachEntry.EnsureEntryHasSocketSet(Owner))
		{
			return false;
		}
	}
	if (BombsThrownPerInterval_Max < 1 || BombsThrownPerInterval_Min < 1)
	{
		RTSFunctionLibrary::ReportError("Max or min setting for bombs thrown per interval"
			"is not valid for: " + OwnerName);
		BombsThrownPerInterval_Max = 1;
		BombsThrownPerInterval_Min = 1;
	}
	if (BombsThrownPerInterval_Max < BombsThrownPerInterval_Min)
	{
		RTSFunctionLibrary::ReportError("Max setting smaller than min setting for throwing bombs per interval"
			"for : " + OwnerName);
		BombsThrownPerInterval_Max = BombsThrownPerInterval_Min;
	}
	return true;
}

void FBombBayData::DebugBombBayState() const
{
	if constexpr (DeveloperSettings::Debugging::GBombBay_Compile_DebugSymbols)
	{
		FString DebugString = "\n Bomb Bay State:";
		for (const FBombBayEntry& BombEntry : TBombEntries)
		{
			DebugString += FString::Printf(TEXT("\n Socket: %s, Armed: %s, InstanceIndex: %d"),
			                               *BombEntry.SocketName.ToString(),
			                               BombEntry.IsBombArmed() ? TEXT("True") : TEXT("False"),
			                               BombEntry.GetBombInstancerIndex());
		}
		FRTSAircraftHelpers::BombDebug(DebugString, FColor::Blue);
	}
}
