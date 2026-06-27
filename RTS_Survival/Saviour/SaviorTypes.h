//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2025 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"
#include "Savior_Shared.h"

#include "Runtime/Core/Public/Serialization/ArchiveSaveCompressedProxy.h"
#include "Runtime/Core/Public/Serialization/ArchiveLoadCompressedProxy.h"

#include "Runtime/CoreUObject/Public/UObject/SoftObjectPtr.h"

#include "Runtime/Engine/Public/Materials/Material.h"
#include "Runtime/Engine/Public/Materials/MaterialExpression.h"
#include "Runtime/Engine/Public/Materials/MaterialInstanceDynamic.h"

#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"

#include "SaviorTypes.generated.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class USavior;
class UMetaSlot;
class SaviorReflector;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Internal Types:

enum class ESeverity : uint8
{
	Fatal = 0,
	Error = 1,
	Warning = 2,
	Info = 3
};

UENUM(BlueprintType, meta = (DisplayName = "[SAVIOR] Runtime Mode", BlueprintInternalUseOnly = true))
enum class ERuntimeMode : uint8
{
	/// Executes Serialization within Game Thread.
	SynchronousTask,
	/// Executes Serialization from its own Asynchronous Threaded.
	BackgroundTask
};

UENUM(BlueprintType, meta = (DisplayName = "[SAVIOR] Thread Safety", BlueprintInternalUseOnly = true))
enum class EThreadSafety : uint8
{
	/// Is Safe to Bind any Interface Calls.
	IsCurrentlyThreadSafe,
	/// Is about to perform runtime expensive operations.
	IsPreparingToSaveOrLoad,
	/// Is Unsafe Loading. Any interaction with Main Game's while loading is dangerous!
	AsynchronousLoading,
	/// Is Unsafe Saving. Any interaction with Main Game's while saving is dangerous!
	AsynchronousSaving,
	/// Is Safely Loading. Any interaction with Main Game's Thread is safe!
	SynchronousLoading,
	/// Is Safely Saving. Any interaction with Main Game's Thread is safe!
	SynchronousSaving
};

UENUM(BlueprintType, meta = (DisplayName = "[SAVIOR] Serializer Type"))
enum class ERecordType : uint8
{
	Complex,
	Minimal
};

UENUM(BlueprintType, meta = (DisplayName = "[SAVIOR] Serializer Result"))
enum class ESaviorResult : uint8
{
	Success,
	Failed
};

UENUM(meta = (BlueprintInternalUseOnly = true))
enum class EStructType : uint8
{
	Struct,
	Transform,
	Vector2D,
	Vector3D,
	Rotator,
	LColor,
	FColor,
	Curve,
	Generic
};

UENUM(BlueprintType, meta = (DisplayName = "[SAVIOR] Load-Screen Mode", BlueprintInternalUseOnly = true))
enum class ELoadScreenMode : uint8
{
	BackgroundBlur,
	SplashScreen,
	MoviePlayer,
	NoLoadScreen
};

UENUM(BlueprintType, meta = (DisplayName = "[SAVIOR] Load-Screen Trigger", BlueprintInternalUseOnly = true))
enum class ELoadScreenTrigger : uint8
{
	AllScreens,
	WhileSaving,
	WhileLoading
};

UENUM(BlueprintType, meta = (DisplayName = "[SAVIOR] SGUID Generation Mode", BlueprintInternalUseOnly = true))
enum class EGuidGeneratorMode : uint8
{
	StaticNameToGUID,
	ResolvedNameToGUID,
	MemoryAddressToGUID
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

USTRUCT()
struct FMatScalarRecord
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY()
	FName ParamName;
	UPROPERTY()
	float ParamValue;
	//
	//
	FMatScalarRecord()
		: ParamName(NAME_None)
		, ParamValue(0)
	{}
};

USTRUCT()
struct FMatVectorRecord
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY()
	FName ParamName;
	UPROPERTY()
	FLinearColor ParamValue;
	//
	//
	FMatVectorRecord()
		: ParamName(NAME_None)
		, ParamValue(FLinearColor::Black)
	{}
};

USTRUCT()
struct FMatTextureRecord
{
	GENERATED_USTRUCT_BODY()
	//
	//
	UPROPERTY()
	FName ParamName;
	UPROPERTY()
	FSoftObjectPath ParamValue;
	//
	//
	FMatTextureRecord()
		: ParamName(NAME_None)
		, ParamValue(FSoftObjectPath{})
	{}
};

USTRUCT()
struct FMaterialRecord
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY()
	TMap<FGuid, FMatScalarRecord> ScalarParams;
	UPROPERTY()
	TMap<FGuid, FMatVectorRecord> VectorParams;
	UPROPERTY()
	TMap<FGuid, FMatTextureRecord> TextureParams;
	//
	//
	FMaterialRecord()
		: ScalarParams(TMap<FGuid, FMatScalarRecord>{})
		, VectorParams(TMap<FGuid, FMatVectorRecord>{})
		, TextureParams(TMap<FGuid, FMatTextureRecord>{})
	{}
};

USTRUCT()
struct FFeatureRecord
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY()
	FString PluginName;
	UPROPERTY()
	FString PluginURL;
	UPROPERTY()
	bool IsActive;
	//
	//
	FFeatureRecord()
		: PluginName(TEXT(""))
		, PluginURL(TEXT(""))
		, IsActive(false)
	{}
};

USTRUCT()
struct FChaosGeometryRecord
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY()
	FGuid CollectionID;
	UPROPERTY()
	TArray<bool> ActiveStates;
	UPROPERTY()
	TArray<bool> SimulatableParticles;
	UPROPERTY()
	TArray<uint8> DynamicStates;
	UPROPERTY()
	TArray<FTransform3f> Transforms;
	//
	//
	FChaosGeometryRecord()
		: CollectionID(FGuid())
		, ActiveStates(TArray<bool>{})
		, SimulatableParticles(TArray<bool>{})
		, DynamicStates(TArray<uint8>{})
		, Transforms(TArray<FTransform3f>{})
	{}
};

USTRUCT()
struct FMutableRecord
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY()
	FGuid GUID;
	//
	UPROPERTY()
	FString State;
	//
	UPROPERTY()
	TArray<FCustomizableObjectBoolParameterValue>BoolParams;
	UPROPERTY()
	TArray<FCustomizableObjectIntParameterValue>IntParams;
	UPROPERTY()
	TArray<FCustomizableObjectFloatParameterValue>FloatParams;
	UPROPERTY()
	TArray<FCustomizableObjectTextureParameterValue>TextureParams;
	UPROPERTY()
	TArray<FCustomizableObjectVectorParameterValue>VectorParams;
	UPROPERTY()
	TArray<FCustomizableObjectProjectorParameterValue>ProjectorParams;
	UPROPERTY()
	TArray<FCustomizableObjectTransformParameterValue>TransformParams;
	//
	//
	FMutableRecord()
		: GUID(), State()
		, BoolParams(TArray<FCustomizableObjectBoolParameterValue>{})
		, IntParams(TArray<FCustomizableObjectIntParameterValue>{})
		, FloatParams(TArray<FCustomizableObjectFloatParameterValue>{})
		, TextureParams(TArray<FCustomizableObjectTextureParameterValue>{})
		, VectorParams(TArray<FCustomizableObjectVectorParameterValue>{})
		, ProjectorParams(TArray<FCustomizableObjectProjectorParameterValue>{})
		, TransformParams(TArray<FCustomizableObjectTransformParameterValue>{})
	{}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType, meta = (BlueprintInternalUseOnly = true))
struct RTS_SURVIVAL_API FSaviorRecord
{
	GENERATED_USTRUCT_BODY()
	//
	//
	UPROPERTY()
	FGuid GUID;
	//
	UPROPERTY()
	FString Buffer;
	//
	UPROPERTY()
	FString FullName;
	UPROPERTY()
	FString OuterName;
	UPROPERTY()
	FString ClassPath;
	//
	UPROPERTY()
	FName Socket;
	UPROPERTY()
	FString RootMesh;
	UPROPERTY()
	FVector Scale;
	UPROPERTY()
	FVector Location;
	UPROPERTY()
	FRotator Rotation;
	//
	UPROPERTY()
	FVector LinearVelocity;
	UPROPERTY()
	FVector AngularVelocity;
	//
	UPROPERTY()
	bool Active;
	UPROPERTY()
	bool Destroyed;
	UPROPERTY()
	bool HiddenInGame;
	UPROPERTY(Transient)
	bool IgnoreTransform;
	//
	//
	FSaviorRecord()
		: GUID(FGuid{})
		//
		, Buffer(TEXT(""))
		//
		, FullName(TEXT(""))
		, OuterName(TEXT(""))
		, ClassPath(TEXT(""))
		//
		, Socket(NAME_None)
		, RootMesh(TEXT(""))
		, Scale(FVector::ZeroVector)
		, Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		//
		, LinearVelocity(FVector::ZeroVector)
		, AngularVelocity(FVector::ZeroVector)
		//
		, Active(true)
		, Destroyed(false)
		, HiddenInGame(false)
		, IgnoreTransform(false)
	{
	}
	//
	//
	inline FSaviorRecord& operator=(const FSaviorRecord& REC)
	{
		GUID = REC.GUID;
		Scale = REC.Scale;
		Buffer = REC.Buffer;
		FullName = REC.FullName;
		Location = REC.Location;
		Rotation = REC.Rotation;
		RootMesh = REC.RootMesh;
		ClassPath = REC.ClassPath;
		Destroyed = REC.Destroyed;
		HiddenInGame = REC.HiddenInGame;
		LinearVelocity = REC.LinearVelocity;
		AngularVelocity = REC.AngularVelocity;
		Socket = REC.Socket;
		return *this;
	}
	//
	inline bool operator==(const FSaviorRecord& REC) const
	{
		return (
			(GUID == REC.GUID) && (Scale == REC.Scale) && (Buffer == REC.Buffer) && (FullName == REC.FullName) && (Location == REC.Location) && (Rotation == REC.Rotation) && (RootMesh == REC.RootMesh) && (ClassPath == REC.ClassPath) && (Destroyed == REC.Destroyed) && (HiddenInGame == REC.HiddenInGame) && (LinearVelocity == REC.LinearVelocity) && (AngularVelocity == REC.AngularVelocity));
	}
	//
	//
	friend inline uint32 GetTypeHash(const FSaviorRecord& REC)
	{
		return FCrc::MemCrc32(&REC, sizeof(FSaviorRecord));
	}
};

USTRUCT(BlueprintType, meta = (BlueprintInternalUseOnly = true))
struct RTS_SURVIVAL_API FSaviorMinimal
{
	GENERATED_USTRUCT_BODY()
	//
	//
	UPROPERTY()
	FString Buffer;
	UPROPERTY()
	FVector Location;
	UPROPERTY()
	FRotator Rotation;
	//
	UPROPERTY()
	bool Destroyed;
	//
	//
	inline FSaviorMinimal& operator=(const FSaviorMinimal& REC)
	{
		Buffer = REC.Buffer;
		Location = REC.Location;
		Rotation = REC.Rotation;
		Destroyed = REC.Destroyed;
		return *this;
	}
	//
	friend inline uint32 GetTypeHash(const FSaviorMinimal& REC)
	{
		return FCrc::MemCrc32(&REC, sizeof(FSaviorMinimal));
	}
	//
	//
	FSaviorMinimal()
		: Buffer(TEXT(""))
		, Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, Destroyed(false)
	{
	}
};

USTRUCT(meta = (BlueprintInternalUseOnly = true))
struct RTS_SURVIVAL_API FSaviorRedirector
{
	GENERATED_USTRUCT_BODY()
	//
	UPROPERTY(Category = "Versioning", EditAnywhere)
	TMap<FName, FName> PropertyRedirect;
	//
	//
	inline FSaviorRedirector& operator=(const FSaviorRedirector& RED)
	{
		PropertyRedirect = RED.PropertyRedirect;
		return *this;
	}
	//
	friend inline uint32 GetTypeHash(const FSaviorRedirector& RED)
	{
		return FCrc::MemCrc32(&RED, sizeof(FSaviorRedirector));
	}
	//
	//
	FSaviorRedirector()
		: PropertyRedirect(TMap<FName, FName>{})
	{
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MUTABLE PARAMETERS OPERATORS

inline FArchive& operator<<(FArchive& AR, FCustomizableObjectBoolParameterValue& PV)
{
	AR << PV.ParameterName;
	AR << PV.ParameterValue;
	AR << PV.Id;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FCustomizableObjectIntParameterValue& PV)
{
	AR << PV.ParameterRangeValueNames;
	AR << PV.ParameterName;
	AR << PV.Id;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FCustomizableObjectFloatParameterValue& PV)
{
	AR << PV.ParameterRangeValues;
	AR << PV.ParameterName;
	AR << PV.ParameterValue;
	AR << PV.Id;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FCustomizableObjectVectorParameterValue& PV)
{
	AR << PV.ParameterName;
	AR << PV.ParameterValue;
	AR << PV.Id;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FCustomizableObjectTransformParameterValue& PV)
{
	AR << PV.ParameterName;
	AR << PV.ParameterValue;
	AR << PV.Id;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FCustomizableObjectProjectorParameterValue& PV)
{
	AR << PV.RangeValues;
	AR << PV.ParameterName;
	AR << PV.Value;
	AR << PV.Id;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FCustomizableObjectTextureParameterValue& PV)
{
	AR << PV.ParameterRangeValues;
	AR << PV.ParameterName;
	AR << PV.ParameterValue;
	AR << PV.Id;
	//
	return AR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SAVIOR TYPE OPERATORS

inline FArchive& operator<<(FArchive& AR, FSaviorRecord& SR)
{
	AR << SR.GUID;
	AR << SR.Scale;
	AR << SR.Buffer;
	AR << SR.FullName;
	AR << SR.Location;
	AR << SR.Rotation;
	AR << SR.RootMesh;
	AR << SR.ClassPath;
	AR << SR.Destroyed;
	AR << SR.HiddenInGame;
	AR << SR.LinearVelocity;
	AR << SR.AngularVelocity;
	AR << SR.Socket;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FSaviorMinimal& SR)
{
	AR << SR.Buffer;
	AR << SR.Location;
	AR << SR.Rotation;
	AR << SR.Destroyed;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FSaviorRedirector& SR)
{
	AR << SR.PropertyRedirect;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FMaterialRecord& MR)
{
	AR << MR.ScalarParams;
	AR << MR.VectorParams;
	AR << MR.TextureParams;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FMatScalarRecord& MR)
{
	AR << MR.ParamName;
	AR << MR.ParamValue;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FMatVectorRecord& MR)
{
	AR << MR.ParamName;
	AR << MR.ParamValue;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FMatTextureRecord& MR)
{
	AR << MR.ParamName;
	AR << MR.ParamValue;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FChaosGeometryRecord& GR)
{
	AR << GR.Transforms;
	AR << GR.CollectionID;
	AR << GR.ActiveStates;
	AR << GR.DynamicStates;
	AR << GR.SimulatableParticles;
	//
	return AR;
}

inline FArchive& operator<<(FArchive& AR, FMutableRecord& MR)
{
	AR << MR.State;
	AR << MR.BoolParams;
	AR << MR.IntParams;
	AR << MR.FloatParams;
	AR << MR.VectorParams;
	AR << MR.TextureParams;
	AR << MR.ProjectorParams;
	AR << MR.TransformParams;
	//
	return AR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
