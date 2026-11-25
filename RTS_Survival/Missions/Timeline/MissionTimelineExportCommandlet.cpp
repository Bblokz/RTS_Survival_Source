// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionTimelineExportCommandlet.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "K2Node_MacroInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMissionTimelineExport, Log, All);

UMissionTimelineExportCommandlet::UMissionTimelineExportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace MissionTimelineExportPrivate
{
	// Actors whose properties are dumped (by class name anywhere in their class chain); every other actor only gets its
	// label, class, transform and tags. Overridden with -PropClasses=.
	const TCHAR* DefaultPropClasses = TEXT("EnemyPlacementActor,EnemyArtilleryBattery,ConvoyPressureDirector,"
		"PlayerStartLocation,ScriptedTriggerActor,LookoutTowerCaptureActor,CaptureActor,DormantDefenceSlot,"
		"BuriedVehicleSite,OreHeapStation,TriggerArea");
	// Data assets referenced by referenced data assets are followed this deep (stage -> convoy definition -> sound).
	constexpr int32 MaxAssetDepth = 3;
	constexpr int32 SuccessCode = 0;
	constexpr int32 FailureCode = 1;

	/**
	 * @brief Collects the model while it is written: Blueprints still to dump (parents, macro libraries) and the data
	 * assets and sounds that exported properties point at.
	 */
	class FExporter
	{
	public:
		explicit FExporter(const TArray<FString>& InPropClasses)
			: M_PropClasses(InPropClasses)
		{
			M_ExportCallback.BindRaw(this, &FExporter::ExportObjectReference);
		}

		TSharedRef<FJsonObject> ExportMap(UWorld& World);

	private:
		TArray<FString> M_PropClasses;
		FJsonObjectConverter::CustomExportCallback M_ExportCallback;
		TArray<TWeakObjectPtr<UBlueprint>> M_BlueprintQueue;
		TSet<FString> M_QueuedBlueprintPaths;
		TArray<TWeakObjectPtr<UObject>> M_AssetQueue;
		TMap<FString, int32> M_AssetDepths;
		int32 M_CurrentAssetDepth = 0;
		// Enums seen on graph pins: pin literals hold their index ("1"), events hand over names ("Escaped").
		TMap<FString, TArray<FString>> M_Enums;

		TSharedPtr<FJsonValue> ExportObjectReference(FProperty* Property, const void* Value);
		TSharedRef<FJsonObject> ExportObjectProperties(const UObject& Object);
		void QueueBlueprintChain(const UClass* Class);
		void QueueAsset(UObject* Asset);

		TArray<TSharedPtr<FJsonValue>> ExportMissions(const AMissionManager& Manager);
		TSharedRef<FJsonObject> ExportMission(const UMissionBase& Mission, int32 Index);
		TArray<TSharedPtr<FJsonValue>> ExportActors(UWorld& World);
		TSharedRef<FJsonObject> ExportActor(const AActor& Actor);
		bool GetShouldExportActorProperties(const AActor& Actor) const;
		TSharedRef<FJsonObject> ExportBlueprints();
		TSharedRef<FJsonObject> ExportBlueprint(const UBlueprint& Blueprint);
		TSharedRef<FJsonObject> ExportGraph(const UEdGraph& Graph, const FString& GraphKind);
		TSharedRef<FJsonObject> ExportNode(const UEdGraphNode& Node);
		TSharedRef<FJsonObject> ExportPin(const UEdGraphPin& Pin);
		void RecordEnum(const UEnum& Enum);
		TSharedRef<FJsonObject> ExportEnums() const;
		TSharedRef<FJsonObject> ExportAssets();

		static void GatherLevels(UWorld& World, TArray<ULevel*>& OutLevels);
		static AMissionManager* FindMissionManager(const TArray<ULevel*>& Levels);
		static TSharedPtr<FJsonValue> VectorToJson(const FVector& Vector);
	};

	TSharedPtr<FJsonValue> FExporter::ExportObjectReference(FProperty* Property, const void* Value)
	{
		const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
		if (ObjectProperty == nullptr || Value == nullptr)
		{
			return nullptr;
		}
		UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
		if (not IsValid(Object))
		{
			return nullptr;
		}
		// Actors and components are referenced by name only: following them would dump whole levels.
		if (const AActor* Actor = Cast<AActor>(Object))
		{
			return MakeShared<FJsonValueString>(TEXT("Actor:") + Actor->GetActorLabel());
		}
		if (Object->IsA<UActorComponent>())
		{
			return MakeShared<FJsonValueString>(Object->GetPathName());
		}
		if (Object->IsA<USoundBase>() || Object->IsA<UDataAsset>())
		{
			QueueAsset(Object);
		}
		return nullptr;
	}

	TSharedRef<FJsonObject> FExporter::ExportObjectProperties(const UObject& Object)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		FJsonObjectConverter::UStructToJsonObject(Object.GetClass(), &Object, Json, 0, CPF_Transient,
		                                          &M_ExportCallback);
		return Json;
	}

	void FExporter::QueueBlueprintChain(const UClass* Class)
	{
		for (const UClass* Current = Class; Current != nullptr; Current = Current->GetSuperClass())
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Current->ClassGeneratedBy);
			if (Blueprint == nullptr)
			{
				return;
			}
			const FString Path = Blueprint->GetPathName();
			if (M_QueuedBlueprintPaths.Contains(Path))
			{
				continue;
			}
			M_QueuedBlueprintPaths.Add(Path);
			M_BlueprintQueue.Add(Blueprint);
		}
	}

	void FExporter::QueueAsset(UObject* Asset)
	{
		const FString Path = Asset->GetPathName();
		if (M_AssetDepths.Contains(Path) || M_CurrentAssetDepth >= MaxAssetDepth)
		{
			return;
		}
		M_AssetDepths.Add(Path, M_CurrentAssetDepth + 1);
		M_AssetQueue.Add(Asset);
	}

	TSharedRef<FJsonObject> FExporter::ExportMap(UWorld& World)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("map"), World.GetOutermost()->GetName());
		Root->SetStringField(TEXT("exported_utc"), FDateTime::UtcNow().ToIso8601());

		TArray<ULevel*> Levels;
		GatherLevels(World, Levels);
		const AMissionManager* Manager = FindMissionManager(Levels);
		if (Manager == nullptr)
		{
			UE_LOG(LogMissionTimelineExport, Error, TEXT("No AMissionManager in %s."), *World.GetName());
		}
		else
		{
			Root->SetStringField(TEXT("mission_manager"), Manager->GetActorLabel());
			Root->SetArrayField(TEXT("missions"), ExportMissions(*Manager));
		}
		Root->SetArrayField(TEXT("actors"), ExportActors(World));
		Root->SetObjectField(TEXT("blueprints"), ExportBlueprints());
		Root->SetObjectField(TEXT("assets"), ExportAssets());
		Root->SetObjectField(TEXT("enums"), ExportEnums());
		return Root;
	}

	TArray<TSharedPtr<FJsonValue>> FExporter::ExportMissions(const AMissionManager& Manager)
	{
		TArray<TSharedPtr<FJsonValue>> Missions;
		for (int32 Index = 0; Index < Manager.Missions.Num(); ++Index)
		{
			const UMissionBase* Mission = Manager.Missions[Index];
			if (not IsValid(Mission))
			{
				continue;
			}
			Missions.Add(MakeShared<FJsonValueObject>(ExportMission(*Mission, Index)));
		}
		return Missions;
	}

	TSharedRef<FJsonObject> FExporter::ExportMission(const UMissionBase& Mission, const int32 Index)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("index"), Index);
		Json->SetStringField(TEXT("class"), Mission.GetClass()->GetName());
		Json->SetStringField(TEXT("class_path"), Mission.GetClass()->GetPathName());
		QueueBlueprintChain(Mission.GetClass());
		// The instance, not the class defaults: a map instance can override what its Blueprint says.
		Json->SetObjectField(TEXT("properties"), ExportObjectProperties(Mission));
		const UMissionTrigger* Trigger = Mission.MissionTrigger;
		if (IsValid(Trigger))
		{
			TSharedRef<FJsonObject> TriggerJson = ExportObjectProperties(*Trigger);
			TriggerJson->SetStringField(TEXT("_class"), Trigger->GetClass()->GetName());
			Json->SetObjectField(TEXT("trigger"), TriggerJson);
		}
		return Json;
	}

	TArray<TSharedPtr<FJsonValue>> FExporter::ExportActors(UWorld& World)
	{
		TArray<ULevel*> Levels;
		GatherLevels(World, Levels);
		TArray<TSharedPtr<FJsonValue>> Actors;
		for (const ULevel* Level : Levels)
		{
			for (const AActor* Actor : Level->Actors)
			{
				if (not IsValid(Actor))
				{
					continue;
				}
				Actors.Add(MakeShared<FJsonValueObject>(ExportActor(*Actor)));
			}
		}
		return Actors;
	}

	TSharedRef<FJsonObject> FExporter::ExportActor(const AActor& Actor)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("label"), Actor.GetActorLabel());
		Json->SetStringField(TEXT("class"), Actor.GetClass()->GetName());
		TArray<TSharedPtr<FJsonValue>> ClassChain;
		for (const UClass* Class = Actor.GetClass(); Class != nullptr; Class = Class->GetSuperClass())
		{
			ClassChain.Add(MakeShared<FJsonValueString>(Class->GetName()));
			if (Class == AActor::StaticClass())
			{
				break;
			}
		}
		Json->SetArrayField(TEXT("class_chain"), ClassChain);
		// Components are not registered in a commandlet, so the world transform is never computed; the root's relative
		// transform is the world transform for an unattached actor.
		const USceneComponent* Root = Actor.GetRootComponent();
		Json->SetField(TEXT("location"), VectorToJson(Root ? Root->GetRelativeLocation() : FVector::ZeroVector));
		Json->SetNumberField(TEXT("yaw"), Root ? Root->GetRelativeRotation().Yaw : 0.0);
		if (Root != nullptr && Root->GetAttachParent() != nullptr)
		{
			Json->SetStringField(TEXT("attached_to"), Root->GetAttachParent()->GetPathName());
		}
		TArray<TSharedPtr<FJsonValue>> Tags;
		for (const FName& Tag : Actor.Tags)
		{
			Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		Json->SetArrayField(TEXT("tags"), Tags);
		if (GetShouldExportActorProperties(Actor))
		{
			Json->SetObjectField(TEXT("properties"), ExportObjectProperties(Actor));
		}
		return Json;
	}

	bool FExporter::GetShouldExportActorProperties(const AActor& Actor) const
	{
		for (const UClass* Class = Actor.GetClass(); Class != nullptr; Class = Class->GetSuperClass())
		{
			if (M_PropClasses.Contains(Class->GetName()))
			{
				return true;
			}
		}
		return false;
	}

	TSharedRef<FJsonObject> FExporter::ExportBlueprints()
	{
		TSharedRef<FJsonObject> Blueprints = MakeShared<FJsonObject>();
		// The queue grows while it is walked (macro libraries found in graphs), so index, never range-for.
		for (int32 Index = 0; Index < M_BlueprintQueue.Num(); ++Index)
		{
			const UBlueprint* Blueprint = M_BlueprintQueue[Index].Get();
			if (Blueprint == nullptr)
			{
				continue;
			}
			Blueprints->SetObjectField(Blueprint->GetPathName(), ExportBlueprint(*Blueprint));
		}
		return Blueprints;
	}

	TSharedRef<FJsonObject> FExporter::ExportBlueprint(const UBlueprint& Blueprint)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("generated_class"),
		                     Blueprint.GeneratedClass ? Blueprint.GeneratedClass->GetPathName() : FString());
		Json->SetStringField(TEXT("parent_class"), Blueprint.ParentClass ? Blueprint.ParentClass->GetPathName() : FString());
		TArray<TSharedPtr<FJsonValue>> Graphs;
		const TPair<const TArray<TObjectPtr<UEdGraph>>*, const TCHAR*> GraphSets[] = {
			{&Blueprint.UbergraphPages, TEXT("event")},
			{&Blueprint.FunctionGraphs, TEXT("function")},
			{&Blueprint.MacroGraphs, TEXT("macro")}
		};
		for (const TPair<const TArray<TObjectPtr<UEdGraph>>*, const TCHAR*>& GraphSet : GraphSets)
		{
			for (const UEdGraph* Graph : *GraphSet.Key)
			{
				if (Graph != nullptr)
				{
					Graphs.Add(MakeShared<FJsonValueObject>(ExportGraph(*Graph, GraphSet.Value)));
				}
			}
		}
		Json->SetArrayField(TEXT("graphs"), Graphs);
		return Json;
	}

	TSharedRef<FJsonObject> FExporter::ExportGraph(const UEdGraph& Graph, const FString& GraphKind)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Graph.GetName());
		Json->SetStringField(TEXT("kind"), GraphKind);
		TArray<TSharedPtr<FJsonValue>> Nodes;
		for (const UEdGraphNode* Node : Graph.Nodes)
		{
			if (Node != nullptr)
			{
				Nodes.Add(MakeShared<FJsonValueObject>(ExportNode(*Node)));
			}
		}
		Json->SetArrayField(TEXT("nodes"), Nodes);
		return Json;
	}

	TSharedRef<FJsonObject> FExporter::ExportNode(const UEdGraphNode& Node)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("id"), Node.NodeGuid.ToString());
		Json->SetStringField(TEXT("class"), Node.GetClass()->GetName());
		Json->SetStringField(TEXT("title"), Node.GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		// Reflected node settings: FunctionReference, EventReference, CustomFunctionName, VariableReference,
		// StructType, TargetType, MacroGraphReference, DelegateReference, ... The simulator reads what it knows.
		Json->SetObjectField(TEXT("settings"), ExportObjectProperties(Node));
		if (const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(&Node))
		{
			const UEdGraph* MacroGraph = Macro->GetMacroGraph();
			if (MacroGraph != nullptr)
			{
				Json->SetStringField(TEXT("macro_graph"), MacroGraph->GetName());
				if (UBlueprint* MacroBlueprint = MacroGraph->GetTypedOuter<UBlueprint>())
				{
					Json->SetStringField(TEXT("macro_blueprint"), MacroBlueprint->GetPathName());
					if (not M_QueuedBlueprintPaths.Contains(MacroBlueprint->GetPathName()))
					{
						M_QueuedBlueprintPaths.Add(MacroBlueprint->GetPathName());
						M_BlueprintQueue.Add(MacroBlueprint);
					}
				}
			}
		}
		TArray<TSharedPtr<FJsonValue>> Pins;
		for (const UEdGraphPin* Pin : Node.Pins)
		{
			if (Pin != nullptr)
			{
				Pins.Add(MakeShared<FJsonValueObject>(ExportPin(*Pin)));
			}
		}
		Json->SetArrayField(TEXT("pins"), Pins);
		return Json;
	}

	TSharedRef<FJsonObject> FExporter::ExportPin(const UEdGraphPin& Pin)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Pin.PinName.ToString());
		Json->SetStringField(TEXT("dir"), Pin.Direction == EGPD_Input ? TEXT("in") : TEXT("out"));
		Json->SetStringField(TEXT("category"), Pin.PinType.PinCategory.ToString());
		Json->SetStringField(TEXT("sub_category"), Pin.PinType.PinSubCategory.ToString());
		if (const UObject* SubObject = Pin.PinType.PinSubCategoryObject.Get())
		{
			Json->SetStringField(TEXT("sub_object"), SubObject->GetName());
			if (const UEnum* Enum = Cast<UEnum>(SubObject))
			{
				RecordEnum(*Enum);
			}
		}
		Json->SetNumberField(TEXT("container"), static_cast<int32>(Pin.PinType.ContainerType));
		if (not Pin.DefaultValue.IsEmpty())
		{
			Json->SetStringField(TEXT("default"), Pin.DefaultValue);
		}
		if (Pin.DefaultObject != nullptr)
		{
			Json->SetStringField(TEXT("default_object"), Pin.DefaultObject->GetPathName());
		}
		if (not Pin.DefaultTextValue.IsEmpty())
		{
			Json->SetStringField(TEXT("default_text"), Pin.DefaultTextValue.ToString());
		}
		TArray<TSharedPtr<FJsonValue>> Links;
		for (const UEdGraphPin* Linked : Pin.LinkedTo)
		{
			if (Linked == nullptr || Linked->GetOwningNodeUnchecked() == nullptr)
			{
				continue;
			}
			TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
			Link->SetStringField(TEXT("node"), Linked->GetOwningNode()->NodeGuid.ToString());
			Link->SetStringField(TEXT("pin"), Linked->PinName.ToString());
			Links.Add(MakeShared<FJsonValueObject>(Link));
		}
		Json->SetArrayField(TEXT("links"), Links);
		return Json;
	}

	void FExporter::RecordEnum(const UEnum& Enum)
	{
		if (M_Enums.Contains(Enum.GetName()))
		{
			return;
		}
		TArray<FString> Names;
		// The last entry is the generated _MAX.
		for (int32 Index = 0; Index < Enum.NumEnums() - 1; ++Index)
		{
			Names.Add(Enum.GetNameStringByIndex(Index));
		}
		M_Enums.Add(Enum.GetName(), Names);
	}

	TSharedRef<FJsonObject> FExporter::ExportEnums() const
	{
		TSharedRef<FJsonObject> Enums = MakeShared<FJsonObject>();
		for (const TPair<FString, TArray<FString>>& Pair : M_Enums)
		{
			TArray<TSharedPtr<FJsonValue>> Names;
			for (const FString& Name : Pair.Value)
			{
				Names.Add(MakeShared<FJsonValueString>(Name));
			}
			Enums->SetArrayField(Pair.Key, Names);
		}
		return Enums;
	}

	TSharedRef<FJsonObject> FExporter::ExportAssets()
	{
		TSharedRef<FJsonObject> Assets = MakeShared<FJsonObject>();
		// The queue grows while it is walked (assets referenced by assets), so index, never range-for.
		for (int32 Index = 0; Index < M_AssetQueue.Num(); ++Index)
		{
			UObject* Asset = M_AssetQueue[Index].Get();
			if (Asset == nullptr)
			{
				continue;
			}
			const FString Path = Asset->GetPathName();
			M_CurrentAssetDepth = M_AssetDepths.FindRef(Path);
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
			if (const USoundBase* Sound = Cast<USoundBase>(Asset))
			{
				Json->SetNumberField(TEXT("duration"), Sound->GetDuration());
			}
			else
			{
				Json->SetObjectField(TEXT("properties"), ExportObjectProperties(*Asset));
			}
			Assets->SetObjectField(Path, Json);
		}
		return Assets;
	}

	void FExporter::GatherLevels(UWorld& World, TArray<ULevel*>& OutLevels)
	{
		if (World.PersistentLevel != nullptr)
		{
			OutLevels.Add(World.PersistentLevel);
		}
		for (const ULevelStreaming* Streaming : World.GetStreamingLevels())
		{
			if (Streaming == nullptr)
			{
				continue;
			}
			UPackage* Package = LoadPackage(nullptr, *Streaming->GetWorldAssetPackageName(), LOAD_None);
			const UWorld* SubWorld = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
			if (SubWorld != nullptr && SubWorld->PersistentLevel != nullptr)
			{
				OutLevels.AddUnique(SubWorld->PersistentLevel);
			}
		}
	}

	AMissionManager* FExporter::FindMissionManager(const TArray<ULevel*>& Levels)
	{
		for (const ULevel* Level : Levels)
		{
			for (AActor* Actor : Level->Actors)
			{
				if (AMissionManager* Manager = Cast<AMissionManager>(Actor))
				{
					return Manager;
				}
			}
		}
		return nullptr;
	}

	TSharedPtr<FJsonValue> FExporter::VectorToJson(const FVector& Vector)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
		Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
		Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
		return MakeShared<FJsonValueArray>(Values);
	}
}
#endif

int32 UMissionTimelineExportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace MissionTimelineExportPrivate;
	FString MapPath;
	FString OutPath;
	FString PropClassList = DefaultPropClasses;
	if (not FParse::Value(*Params, TEXT("Map="), MapPath) || not FParse::Value(*Params, TEXT("Out="), OutPath))
	{
		UE_LOG(LogMissionTimelineExport, Error, TEXT("Usage: -run=MissionTimelineExport -Map=/Game/... -Out=<file.json> "
			"[-PropClasses=A,B]"));
		return FailureCode;
	}
	FParse::Value(*Params, TEXT("PropClasses="), PropClassList, false);
	TArray<FString> PropClasses;
	PropClassList.ParseIntoArray(PropClasses, TEXT(","), true);

	UPackage* Package = LoadPackage(nullptr, *MapPath, LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (World == nullptr)
	{
		UE_LOG(LogMissionTimelineExport, Error, TEXT("Could not load the map %s."), *MapPath);
		return FailureCode;
	}

	FExporter Exporter(PropClasses);
	const TSharedRef<FJsonObject> Model = Exporter.ExportMap(*World);
	FString Text;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(Model, Writer);
	if (not FFileHelper::SaveStringToFile(Text, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogMissionTimelineExport, Error, TEXT("Could not write %s."), *OutPath);
		return FailureCode;
	}
	UE_LOG(LogMissionTimelineExport, Display, TEXT("MISSION_TIMELINE_EXPORT_OK %s -> %s (%d chars)"), *MapPath, *OutPath,
	       Text.Len());
	return SuccessCode;
#else
	return 1;
#endif
}
