//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2025 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Savior.h"

#include "AbilitySystemComponent.h"
#include "Savior_Shared.h"
#include "LoadScreen/HUD_SaveLoadScreen.h"

#include "Runtime/Core/Public/Misc/App.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"

#include "Runtime/Engine/Classes/Engine/Font.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Public/TimerManager.h"

#include "GameFeatureTypesFwd.h"
#include "MuCO/CustomizableObject.h"
#include "GeometryCollectionProxyData.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"

#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Runtime/AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Class.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SerializeEntity"), STAT_FSimpleDelegateGraphTask_SerializeEntity, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DeserializeEntity"), STAT_FSimpleDelegateGraphTask_DeserializeEntity, STATGROUP_TaskGraphTasks);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

EThreadSafety USavior::LastThreadState = EThreadSafety::IsCurrentlyThreadSafe;
EThreadSafety USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;

FTimerHandle USavior::TH_LoadScreen;

float USavior::SS_Progress = 100.f;
float USavior::SL_Progress = 100.f;
float USavior::SS_Workload = 0.f;
float USavior::SL_Workload = 0.f;
float USavior::SS_Complete = 0.f;
float USavior::SL_Complete = 0.f;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Constructors:

USaviorSettings::USaviorSettings()
{
	DefaultPlayerName = TEXT("Player One");
	DefaultChapter = TEXT("Chapter 1");
	DefaultLocation = TEXT("???");
	DefaultPlayerLevel = 1;
	DefaultPlayerID = 0;
	//
	AccurateDynamicRespawnChecks = true;
	RespawnDynamicComponents = true;
	RespawnDynamicActors = true;
	//
	//
	InstanceScope.Add(UAutoInstanced::StaticClass());
	RespawnScope.Add(UActorComponent::StaticClass());
	RespawnScope.Add(AActor::StaticClass());
	//
	//
	LoadConfig();
}

USavior::USavior()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		AtomicallySetFlags(GetFlags() | RF_MarkAsRootSet);
	}
	//
	if (GetOuter() && GetOuter()->GetWorld())
	{
		World = GetOuter()->GetWorld();
	}
	//
	SlotThumbnail = FSoftObjectPath(TEXT("/Savior/Icons/ICO_Thumbnail.ICO_Thumbnail"));
	if ((!IsRunningDedicatedServer()) && (!FeedbackFont.HasValidFont()))
	{
		ConstructorHelpers::FObjectFinder<UFont> FeedFontOBJ(TEXT("/Engine/EngineFonts/Roboto"));
		if (FeedFontOBJ.Succeeded())
		{
			FeedbackFont = FSlateFontInfo(FeedFontOBJ.Object, 34, FName("Bold"));
		}
	}
	//
	const auto& Settings = GetDefault<USaviorSettings>();
	FeedbackLOAD = FText::FromString(TEXT("LOADING..."));
	FeedbackSAVE = FText::FromString(TEXT("SAVING..."));
	LoadScreenMode = ELoadScreenMode::NoLoadScreen;
	RuntimeMode = ERuntimeMode::BackgroundTask;
	ProgressBarTint = FLinearColor::White;
	SplashStretch = EStretch::Fill;
	ProgressBarOnMovie = true;
	LoadScreenTimer = 1.f;
	BackBlurPower = 25.f;
	//
	ComponentScope.Add(UActorComponent::StaticClass());
	ObjectScope.Add(UAutoInstanced::StaticClass());
	ActorScope.Add(AActor::StaticClass());
	//
	WriteMetaOnSave = true;
	DeepLogs = false;
	Debug = false;
	//
	SlotMeta = FSlotMeta{};
	SlotData = FSlotData{};
}
USavior::~USavior()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Routines:

static void StaticRemoveLoadScreen()
{
	AHUD_SaviorUI::StaticRemoveLoadScreen();
}

UWorld* USavior::GetWorld() const
{
	const auto Owner = GetOuter();
	//
	if (Owner)
	{
		return Owner->GetWorld();
	}
	//
	return World;
}

bool USavior::IsInstance() const
{
	return (GetWorld() && GetWorld()->IsGameWorld());
}

FString USavior::GetSlotName() const
{
	if (SlotFileName.IsEmpty())
	{
		return FString::Printf(TEXT("%s_%s"), FApp::GetProjectName(), *GetName());
	}
	else
	{
		return SlotFileName.ToString();
	}
}

void USavior::GetDataLayers(TArray<const UDataLayerInstance*>& OutLayers) const
{
	TArray<const UDataLayerInstance*> DataLayers{};
	//
	for (TObjectIterator<AWorldDataLayers> WDT; WDT; ++WDT)
	{
		if ((*WDT)->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed))
		{
			continue;
		}
		if ((*WDT)->GetTypedOuter<UWorld>() != GetWorld())
		{
			continue;
		}
		if (!IsValid(*WDT))
		{
			continue;
		}
		//
		for (TObjectIterator<UDataLayerInstance> DAT; DAT; ++DAT)
		{
			if (!WDT->ContainsDataLayer(*DAT))
			{
				continue;
			}
			if (!DAT->IsRuntime())
			{
				continue;
			}
			//
			DataLayers.Add(*DAT);
		}
	}
	//
	OutLayers = DataLayers;
}

void USavior::AbortCurrentTask()
{
	USavior::LastThreadState = USavior::ThreadSafety;
	USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;
	//
	USavior::SS_Workload = 0;
	USavior::SS_Complete = 0;
	USavior::SS_Progress = 100.f;
	USavior::SL_Workload = 0;
	USavior::SL_Complete = 0;
	USavior::SL_Progress = 100.f;
	//
	if (World)
	{
		static FTimerDelegate TimerDelegate = FTimerDelegate::CreateStatic(&StaticRemoveLoadScreen);
		World->GetTimerManager().SetTimer(USavior::TH_LoadScreen, TimerDelegate, LoadScreenTimer, false);
	}
	else
	{
		RemoveLoadScreen();
	}
}

void USavior::SetWorld(UWorld* InWorld)
{
	World = InWorld;
}

void USavior::PostLoad()
{
	Super::PostLoad();
}

void USavior::BeginDestroy()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::BeginDestroy();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Serializer Interface:

void USavior::WriteSlotToFile(const int32 PlayerID, ESaviorResult& Result)
{
	const FString MetaName = FString::Printf(TEXT("%s___Meta"), *GetSlotName());
	//
	auto Meta = (WriteMetaOnSave) ? UGameplayStatics::SaveGameToSlot(SlotMetaToOBJ(), MetaName, 0) : true;
	auto Saved = UGameplayStatics::SaveGameToSlot(SlotDataToOBJ(), GetSlotName(), PlayerID);
	//
	if (Meta && Saved)
	{
		Result = ESaviorResult::Success;
		return;
	}
	//
	Result = ESaviorResult::Failed;
}

void USavior::ReadSlotFromFile(const int32 PlayerID, ESaviorResult& Result)
{
	const FString MetaName = FString::Printf(TEXT("%s___Meta"), *GetSlotName());
	//
	auto LoadedMeta = Cast<UMetaSlot>(UGameplayStatics::LoadGameFromSlot(MetaName, 0));
	auto LoadedData = Cast<UDataSlot>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), PlayerID));
	//
	if ((LoadedMeta) && (LoadedData))
	{
		Result = ESaviorResult::Success;
		SlotMeta = LoadedMeta;
		SlotData = LoadedData;
		return;
	}
	//
	Result = ESaviorResult::Failed;
}

void USavior::DeleteSlotFile(const int32 PlayerID, ESaviorResult& Result)
{
	const FString MetaName = FString::Printf(TEXT("%s___Meta"), *GetSlotName());
	//
	if (UGameplayStatics::DoesSaveGameExist(MetaName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(MetaName, 0);
	}
	//
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(), PlayerID))
	{
		UGameplayStatics::DeleteGameInSlot(GetSlotName(), PlayerID);
		Result = ESaviorResult::Success;
		return;
	}
	//
	Result = ESaviorResult::Failed;
}

void USavior::FindSlotFile(ESaviorResult& Result)
{
	LOG_SV(true, ESeverity::Warning, GetSlotName());
	//
	Result = ESaviorResult::Success;
	if (!UGameplayStatics::DoesSaveGameExist(GetSlotName(), 0))
	{
		Result = ESaviorResult::Failed;
	}
}

void USavior::SaveSlotMetaData(ESaviorResult& Result)
{
	const FString MetaName = FString::Printf(TEXT("%s___Meta"), *GetSlotName());
	//
	auto Meta = UGameplayStatics::SaveGameToSlot(SlotMetaToOBJ(), MetaName, 0);
	if (Meta)
	{
		Result = ESaviorResult::Success;
		return;
	}
	//
	Result = ESaviorResult::Failed;
}

void USavior::LoadSlotMetaData(ESaviorResult& Result)
{
	const FString MetaName = FString::Printf(TEXT("%s___Meta"), *GetSlotName());
	//
	if (!UGameplayStatics::DoesSaveGameExist(MetaName, 0))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	auto LoadedMeta = Cast<UMetaSlot>(UGameplayStatics::LoadGameFromSlot(MetaName, 0));
	//
	if (LoadedMeta)
	{
		SlotMeta = LoadedMeta;
		Result = ESaviorResult::Success;
		return;
	}
	//
	Result = ESaviorResult::Failed;
}

const ESaviorResult USavior::PrepareSlotToSave(const UObject* Context)
{
	if (((GetSaveProgress() < 100.f) && (GetSavesDone() > 0)) || ((GetLoadProgress() < 100.f) && (GetLoadsDone() > 0)))
	{
		LOG_SV(Debug, ESeverity::Warning,
			TEXT("Save or Load action already in progress... Cannot begin another Save action. Save aborted!"));
		EVENT_OnFinishDataSAVE.Broadcast(false);
		return ESaviorResult::Failed;
	}
	//
	if (Context == nullptr || !IsValid(Context) || Context->GetWorld() == nullptr)
	{
		LOG_SV(Debug, ESeverity::Warning, TEXT("Context Object is invalid to start a Save process... Save aborted!"));
		EVENT_OnFinishDataSAVE.Broadcast(false);
		return ESaviorResult::Failed;
	}
	//
	//
	USavior::ThreadSafety = EThreadSafety::IsPreparingToSaveOrLoad;
	World = Context->GetWorld();
	//
	double TimeSpan = SlotMeta.PlayTime + FMath::Abs(World->RealTimeSeconds);
	SlotMeta.PlayTime = FTimespan::FromSeconds(TimeSpan).GetHours();
	SlotMeta.SaveLocation = World->GetName();
	SlotMeta.SaveDate = FDateTime::Now();
	//
	//
	if (LoadScreenTrigger != ELoadScreenTrigger::WhileLoading)
	{
		switch (RuntimeMode)
		{
			case ERuntimeMode::SynchronousTask:
				LaunchLoadScreen(EThreadSafety::SynchronousSaving, FeedbackSAVE);
				break;
			case ERuntimeMode::BackgroundTask:
				LaunchLoadScreen(EThreadSafety::AsynchronousSaving, FeedbackSAVE);
				break;
			default:
				break;
		}
	}
	//
	//
	USavior::SS_Workload = CalculateWorkload();
	USavior::SS_Progress = 0.f;
	USavior::SS_Complete = 0.f;
	//
	EVENT_OnBeginDataSAVE.Broadcast();
	OnPreparedToSave();
	//
	if (PauseGameOnLoad)
	{
		if (auto PC = World->GetFirstPlayerController())
		{
			PC->Pause();
		}
	}
	//
	return ESaviorResult::Success;
}

const ESaviorResult USavior::PrepareSlotToLoad(const UObject* Context)
{
	if (((GetSaveProgress() < 100.f) && (GetSavesDone() > 0)) || ((GetLoadProgress() < 100.f) && (GetLoadsDone() > 0)))
	{
		LOG_SV(Debug, ESeverity::Warning,
			TEXT("Save or Load action already in progress... Cannot begin another Load action. Load aborted!"));
		EVENT_OnFinishDataLOAD.Broadcast(false);
		return ESaviorResult::Failed;
	}
	//
	if (Context == nullptr || !IsValid(Context) || Context->GetWorld() == nullptr)
	{
		LOG_SV(Debug, ESeverity::Warning, TEXT("Context Object is invalid to start a Load process... Load aborted!"));
		EVENT_OnFinishDataLOAD.Broadcast(false);
		return ESaviorResult::Failed;
	}
	//
	//
	USavior::ThreadSafety = EThreadSafety::IsPreparingToSaveOrLoad;
	World = Context->GetWorld();
	SlotMeta.SaveLocation = World->GetName();
	//
	if (LoadScreenTrigger != ELoadScreenTrigger::WhileSaving)
	{
		switch (RuntimeMode)
		{
			case ERuntimeMode::SynchronousTask:
				LaunchLoadScreen(EThreadSafety::SynchronousLoading, FeedbackLOAD);
				break;
			case ERuntimeMode::BackgroundTask:
				LaunchLoadScreen(EThreadSafety::AsynchronousLoading, FeedbackLOAD);
				break;
			default:
				break;
		}
	}
	//
	USavior::SL_Workload = CalculateWorkload();
	USavior::SL_Progress = 0.f;
	USavior::SL_Complete = 0.f;
	//
	EVENT_OnFinishRespawn.Clear();
	EVENT_OnFinishRespawn.BindDynamic(this, &USavior::OnFinishRespawnDynamicActors);
	//
	EVENT_OnBeginDataLOAD.Broadcast();
	OnPreparedToLoad();
	//
	if (PauseGameOnLoad)
	{
		if (auto PC = World->GetFirstPlayerController())
		{
			PC->Pause();
		}
	}
	//
	return ESaviorResult::Success;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Object Serialization Interface:

void USavior::SaveObject(UObject* Object, ESaviorResult& Result)
{
	if ((USavior::ThreadSafety == EThreadSafety::SynchronousSaving) || (USavior::ThreadSafety == EThreadSafety::AsynchronousSaving))
	{
		USavior::SS_Complete++;
		USavior::SS_Progress = ((USavior::SS_Complete / USavior::SS_Workload) * 100);
	}
	//
	Result = ESaviorResult::Success;
	if (Object == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (Object->IsA(UCustomizableObjectInstance::StaticClass()))
	{
		SaveMutableObject(CastChecked<UCustomizableObjectInstance>(Object), Result);
		return;
	}
	//
	const auto ObjectID = Reflector::MakeObjectID(Object);
	const auto Record = GenerateRecord_Object(Object);
	SlotData.Records.Add(ObjectID, Record);
	//
	if (IsInGameThread())
	{
		OnObjectSaved(Object);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnObjectSaved, Object),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::LoadObject(UObject* Object, ESaviorResult& Result)
{
	if ((USavior::ThreadSafety == EThreadSafety::SynchronousLoading) || (USavior::ThreadSafety == EThreadSafety::AsynchronousLoading))
	{
		USavior::SL_Complete++;
		USavior::SL_Progress = ((USavior::SL_Complete / USavior::SL_Workload) * 100);
	}
	//
	Result = ESaviorResult::Success;
	if (Object == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (Object->IsA(UBehaviorTreeComponent::StaticClass()))
	{
		if (TAG_CNOBT(CastChecked<UBehaviorTreeComponent>(Object)))
		{
			Result = ESaviorResult::Failed;
			return;
		}
	}
	//
	if (Object->IsA(UCustomizableObjectInstance::StaticClass()))
	{
		LoadMutableObject(CastChecked<UCustomizableObjectInstance>(Object), Result);
		return;
	}
	//
	const auto ObjectID = Reflector::MakeObjectID(Object);
	if (!SlotData.Records.Contains(ObjectID))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	const auto Record = SlotData.Records.FindRef(ObjectID);
	//
	if (Record.Destroyed)
	{
		MarkObjectAutoDestroyed(Object);
	}
	else
	{
		UnpackRecord_Object(Record, Object, Result);
	}
	//
	if (IsInGameThread())
	{
		OnObjectLoaded(SlotMeta, Object);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnObjectLoaded, SlotMeta, Object),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::SaveMutableObject(UCustomizableObjectInstance* Mutable, ESaviorResult& Result)
{
	Result = ESaviorResult::Success;
	if (Mutable == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Mutable->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto ObjectID = Reflector::MakeObjectID(Mutable);
	const auto Record = GenerateRecord_Mutable(Mutable);
	SlotData.Mutables.Add(ObjectID, Record);
	//
	if (IsInGameThread())
	{
		OnObjectSaved(Mutable);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnObjectSaved, CastChecked<UObject>(Mutable)),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::LoadMutableObject(UCustomizableObjectInstance* Mutable, ESaviorResult& Result)
{
	Result = ESaviorResult::Success;
	//
	if (Mutable == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Mutable->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto ObjectID = Reflector::MakeObjectID(Mutable);
	if (!SlotData.Mutables.Contains(ObjectID))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	const auto Record = SlotData.Mutables.FindRef(ObjectID);
	UnpackRecord_Mutable(Record, Mutable, Result);
	//
	if (IsInGameThread())
	{
		OnObjectLoaded(SlotMeta, CastChecked<UObject>(Mutable));
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnObjectLoaded, SlotMeta, CastChecked<UObject>(Mutable)),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::SaveObjectHierarchy(UObject* Object, ESaviorResult& Result, ESaviorResult& HierarchyResult)
{
	if (Object == nullptr || GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Object->IsA(UActorComponent::StaticClass()) || Object->IsA(UActorComponent::StaticClass()))
	{
		LOG_SV(Debug, ESeverity::Warning,
			TEXT("SaveObjectHierarchy() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	SaveObject(Object, Result);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	TArray<UObject*> Children;
	ForEachObjectWithOuter(
		Object,
		[&Children](UObject* Chid) {
			if (!Chid->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
			{
				Children.Add(Chid);
			}
		},
		true);
	//
	for (const auto& OBJ : Children)
	{
		SaveObject(OBJ, HierarchyResult);
	}
}

void USavior::LoadObjectHierarchy(UObject* Object, ESaviorResult& Result, ESaviorResult& HierarchyResult)
{
	if (Object == nullptr || GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Object->IsA(UActorComponent::StaticClass()) || Object->IsA(UActorComponent::StaticClass()))
	{
		LOG_SV(Debug, ESeverity::Warning,
			TEXT("LoadObjectHierarchy() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	LoadObject(Object, Result);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	TArray<UObject*> Children;
	ForEachObjectWithOuter(
		Object,
		[&Children](UObject* Chid) {
			if (!Chid->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
			{
				Children.Add(Chid);
			}
		},
		true);
	//
	for (const auto& OBJ : Children)
	{
		LoadObject(OBJ, HierarchyResult);
	}
}

void USavior::SaveObjectsOfClass(TSubclassOf<UObject> Class, const bool SaveHierarchy, ESaviorResult& Result,
	ESaviorResult& HierarchyResult)
{
	if (Class.Get() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Class.Get()->IsChildOf(UActorComponent::StaticClass()) || Class.Get()->IsChildOf(UActorComponent::StaticClass()))
	{
		LOG_SV(Debug, ESeverity::Warning,
			TEXT("SaveObjectsOfClass() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed;
		return;
	}
	//
	for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
	{
		if (!OBJ->IsValidLowLevelFast())
		{
			continue;
		}
		if (!OBJ->IsA(Class.Get()))
		{
			continue;
		}
		//
		if ((*OBJ) == GetWorld())
		{
			continue;
		}
		if (!OBJ->GetOutermost()->ContainsMap())
		{
			continue;
		}
		if (OBJ->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		//
		if (!SaveHierarchy)
		{
			SaveObject(*OBJ, Result);
		}
		else
		{
			SaveObjectHierarchy(*OBJ, Result, HierarchyResult);
		}
	}
}

void USavior::LoadObjectsOfClass(TSubclassOf<UObject> Class, const bool LoadHierarchy, ESaviorResult& Result,
	ESaviorResult& HierarchyResult)
{
	if (Class.Get() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Class.Get()->IsChildOf(UActorComponent::StaticClass()) || Class.Get()->IsChildOf(UActorComponent::StaticClass()))
	{
		LOG_SV(Debug, ESeverity::Warning,
			TEXT("LoadObjectsOfClass() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed;
		return;
	}
	//
	for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
	{
		if (!OBJ->IsValidLowLevelFast())
		{
			continue;
		}
		if (!OBJ->IsA(Class.Get()))
		{
			continue;
		}
		//
		if ((*OBJ) == GetWorld())
		{
			continue;
		}
		if (!OBJ->GetOutermost()->ContainsMap())
		{
			continue;
		}
		if (OBJ->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		//
		if (!LoadHierarchy)
		{
			LoadObject(*OBJ, Result);
		}
		else
		{
			LoadObjectHierarchy(*OBJ, Result, HierarchyResult);
		}
	}
}

void USavior::SaveComponent(UObject* Context, UActorComponent* Component, ESaviorResult& Result)
{
	if ((USavior::ThreadSafety == EThreadSafety::SynchronousSaving) || (USavior::ThreadSafety == EThreadSafety::AsynchronousSaving))
	{
		USavior::SS_Complete++;
		USavior::SS_Progress = ((USavior::SS_Complete / USavior::SS_Workload) * 100);
	}
	//
	Result = ESaviorResult::Success;
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	World = Context->GetWorld();
	//
	if (Component == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (TAG_CNOSAVE(Component))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Component->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto ComponentID = Reflector::MakeComponentID(Component, true);
	const auto Record = GenerateRecord_Component(Component);
	SlotData.Records.Add(ComponentID, Record);
	//
	if (Component->IsA(UBlackboardComponent::StaticClass()))
	{
		if (TAG_CNOBT(Component))
		{
			Result = ESaviorResult::Failed;
			return;
		}
		//
		const UBlackboardComponent* BlackboardComponent = CastChecked<UBlackboardComponent>(Component);
		if (UBlackboardData* Blackboard = BlackboardComponent->GetBlackboardAsset())
		{
			const auto BlackboardID = Reflector::MakeObjectID(Blackboard, true);
			const auto BlackboardData = GenerateRecord_Blackboard(BlackboardComponent, Blackboard);
			SlotData.Records.Add(BlackboardID, BlackboardData);
		}
	}
	//
	if (Component->IsA(UGeometryCollectionComponent::StaticClass()))
	{
		const UGeometryCollectionComponent* ChaosComponent = CastChecked<UGeometryCollectionComponent>(Component);
		if (const FGeometryDynamicCollection* DynamicCollection = ChaosComponent->GetDynamicCollection())
		{
			const auto GeometryData = GenerateRecord_ChaosGeometry(ChaosComponent);
			SlotData.ChaosObjects.Add(ComponentID, GeometryData);
		}
	}
	//
	if ((Component->IsA(UAbilitySystemComponent::StaticClass())))
	{
		const TArray<UAttributeSet*>& ATTs = CastChecked<UAbilitySystemComponent>(Component)->GetSpawnedAttributes();
		ESaviorResult				  HierarchyResult = ESaviorResult::Success;
		for (const auto& ATS : ATTs)
		{
			SaveObject(ATS, HierarchyResult);
		}
	}
	//
	if (IsInGameThread())
	{
		OnComponentSaved(Component);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnComponentSaved, Component),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::LoadComponent(UObject* Context, UActorComponent* Component, ESaviorResult& Result)
{
	if ((USavior::ThreadSafety == EThreadSafety::SynchronousLoading) || (USavior::ThreadSafety == EThreadSafety::AsynchronousLoading))
	{
		USavior::SL_Complete++;
		USavior::SL_Progress = ((USavior::SL_Complete / USavior::SL_Workload) * 100);
	}
	//
	Result = ESaviorResult::Success;
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	World = Context->GetWorld();
	//
	if (Component == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (TAG_CNOLOAD(Component))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Component->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	FName ComponentID = NAME_None;
	ComponentID = Reflector::MakeComponentID(Component, true);
	if (!SlotData.Records.Contains(ComponentID))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto Record = SlotData.Records.FindRef(ComponentID);
	if (!TAG_CNOKILL(Component) && (Record.Destroyed))
	{
		MarkComponentAutoDestroyed(Component);
	}
	else
	{
		UnpackRecord_Component(Record, Component, Result);
	}
	//
	if (Component->IsA(UBlackboardComponent::StaticClass()))
	{
		ESaviorResult		  BlackboardResult;
		UBlackboardComponent* BlackboardComponent = CastChecked<UBlackboardComponent>(Component);
		//
		if (UBlackboardData* Blackboard = BlackboardComponent->GetBlackboardAsset())
		{
			FName BlackboardID = NAME_None;
			BlackboardID = Reflector::MakeObjectID(Blackboard, true);
			//
			if (SlotData.Records.Contains(ComponentID))
			{
				const auto BlackboardData = SlotData.Records.FindRef(ComponentID);
				UnpackRecord_Blackboard(BlackboardData, BlackboardComponent, Blackboard, BlackboardResult);
			}
		}
	}
	//
	if (Component->IsA(UGeometryCollectionComponent::StaticClass()))
	{
		UGeometryCollectionComponent* ChaosComponent = CastChecked<UGeometryCollectionComponent>(Component);
		if (const FGeometryDynamicCollection* DynamicCollection = ChaosComponent->GetDynamicCollection())
		{
			ESaviorResult GeometryResult;
			if (SlotData.ChaosObjects.Contains(ComponentID))
			{
				const auto GeometryData = SlotData.ChaosObjects.FindRef(ComponentID);
				UnpackRecord_ChaosGeometry(GeometryData, ChaosComponent, GeometryResult);
			}
		}
	}
	//
	if ((Component->IsA(UAbilitySystemComponent::StaticClass())))
	{
		const TArray<UAttributeSet*>& ATTs = CastChecked<UAbilitySystemComponent>(Component)->GetSpawnedAttributes();
		ESaviorResult				  HierarchyResult = ESaviorResult::Success;
		for (const auto& ATS : ATTs)
		{
			LoadObject(ATS, HierarchyResult);
		}
	}
	//
	if (IsInGameThread())
	{
		OnComponentLoaded(SlotMeta, Component);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnComponentLoaded, SlotMeta, Component),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::LoadComponentWithGUID(UObject* Context, const FGuid& SGUID, ESaviorResult& Result)
{
	if (!SGUID.IsValid())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	World = Context->GetWorld();
	//
	if (World == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	UActorComponent* Component = nullptr;
	for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
	{
		if (!OBJ->IsA(UActorComponent::StaticClass()))
		{
			continue;
		}
		//
		if ((*OBJ) == World)
		{
			continue;
		}
		if (OBJ->GetWorld() != World)
		{
			continue;
		}
		if (!OBJ->GetOutermost()->ContainsMap())
		{
			continue;
		}
		if (OBJ->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		//
		const FGuid UID = Reflector::FindGUID(*OBJ);
		if (UID == SGUID)
		{
			Component = (CastChecked<UActorComponent>(*OBJ));
			break;
		}
	}
	//
	if (Component == nullptr)
	{
		Result = ESaviorResult::Failed;
		LOG_SV(true, ESeverity::Warning,
			FString::Printf(
				TEXT("[Load Component by GUID]: Couldn't find in World any Component owner of this SGUID:  %s"),
				*SGUID.ToString()));
		return;
	}
	//
	for (auto IT = SlotData.Records.CreateConstIterator(); IT; ++IT)
	{
		const FSaviorRecord& Record = IT->Value;
		//
		if (Record.GUID == SGUID)
		{
			if (TAG_CNOLOAD(Component))
			{
				Result = ESaviorResult::Failed;
				return;
			}
			if (!TAG_CNOKILL(Component) && (Record.Destroyed))
			{
				MarkComponentAutoDestroyed(Component);
			}
			else
			{
				UnpackRecord_Component(Record, Component, Result);
			}
		}
	}
	//
	if (IsInGameThread())
	{
		OnComponentLoaded(SlotMeta, Component);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnComponentLoaded, SlotMeta, Component),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::SaveAnimation(UObject* Context, ACharacter* Character, ESaviorResult& Result)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (const auto& Skel = Character->GetMesh())
	{
		if (const auto& Anim = Skel->GetAnimInstance())
		{
			const auto& Interface = Cast<ISAVIOR_Serializable>(Anim);
			if (Interface || Anim->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass()))
			{
				const auto ActorAnimID = Reflector::MakeObjectID(Anim);
				const auto AnimRecord = GenerateRecord_Object(Anim);
				//
				SlotData.Records.Add(ActorAnimID, AnimRecord);
				if (IsInGameThread())
				{
					OnAnimationSaved(Anim);
				}
				else
				{
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
						FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnAnimationSaved, Anim),
						GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity), nullptr, ENamedThreads::GameThread);
				}
			}
			else
			{
				Result = ESaviorResult::Failed;
				return;
			}
		}
		else
		{
			Result = ESaviorResult::Failed;
			return;
		}
	}
	else
	{
		Result = ESaviorResult::Failed;
		return;
	}
}

void USavior::LoadAnimation(UObject* Context, ACharacter* Character, ESaviorResult& Result)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	Result = ESaviorResult::Success;
	//
	if (const auto& Skel = Character->GetMesh())
	{
		if (const auto& Anim = Skel->GetAnimInstance())
		{
			const auto& Interface = Cast<ISAVIOR_Serializable>(Anim);
			if (Interface || Anim->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass()))
			{
				const auto ActorAnimID = Reflector::MakeObjectID(Anim);
				//
				if (!SlotData.Records.Contains(ActorAnimID))
				{
					Result = ESaviorResult::Failed;
					return;
				}
				const auto AnimRecord = SlotData.Records.FindRef(ActorAnimID);
				UnpackRecord_Object(AnimRecord, Anim, Result);
				//
				if (IsInGameThread())
				{
					OnAnimationLoaded(SlotMeta, Anim);
				}
				else
				{
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
						FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnAnimationLoaded, SlotMeta, Anim),
						GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr,
						ENamedThreads::GameThread);
				}
			}
			else
			{
				Result = ESaviorResult::Failed;
				return;
			}
		}
		else
		{
			Result = ESaviorResult::Failed;
			return;
		}
	}
	else
	{
		Result = ESaviorResult::Failed;
		return;
	}
}

void USavior::SaveActorMaterials(UObject* Context, AActor* Actor, ESaviorResult& Result)
{
	if (!IsValid(Actor) || Actor->GetWorld() == nullptr || !Actor->GetWorld()->IsGameWorld())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Context == nullptr || !Context->IsValidLowLevel())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	const auto& Interface = Cast<ISAVIOR_Serializable>(Actor);
	if ((Interface == nullptr) && (!Actor->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto ActorID = Reflector::MakeActorID(Actor);
	if (ActorID.IsEqual(TEXT("ERROR:ID")))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	TArray<USkeletalMeshComponent*> Skins;
	TArray<UStaticMeshComponent*>	Meshes;
	//
	Actor->GetComponents<UStaticMeshComponent>(Meshes);
	Actor->GetComponents<USkeletalMeshComponent>(Skins);
	//
	for (auto CMP : Meshes)
	{
		const auto					Mesh = CastChecked<UStaticMeshComponent>(CMP);
		TArray<UMaterialInterface*> Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats)
		{
			if (!Mat->IsValidLowLevel())
			{
				continue;
			}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass()))
			{
				continue;
			}
			//
			FMaterialRecord MatInfo = CaptureMaterialSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat));
			const FName		MatID = *FString::Printf(TEXT("%s.%s"), *ActorID.ToString(), *Mat->GetName().Replace(TEXT(" "), TEXT("")));
			SlotData.Materials.Add(MatID, MatInfo);
		}
	}
	//
	for (auto CMP : Skins)
	{
		const auto					Mesh = CastChecked<USkeletalMeshComponent>(CMP);
		TArray<UMaterialInterface*> Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats)
		{
			if (!Mat->IsValidLowLevel())
			{
				continue;
			}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass()))
			{
				continue;
			}
			//
			FMaterialRecord MatInfo = CaptureMaterialSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat));
			const FName		MatID = *FString::Printf(TEXT("%s.%s"), *ActorID.ToString(), *Mat->GetName());
			SlotData.Materials.Add(MatID, MatInfo);
		}
	}
}

void USavior::LoadActorMaterials(UObject* Context, AActor* Actor, ESaviorResult& Result)
{
	if (!IsValid(Actor) || Actor->GetWorld() == nullptr || !Actor->GetWorld()->IsGameWorld())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Context == nullptr || !Context->IsValidLowLevel())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	const auto& Interface = Cast<ISAVIOR_Serializable>(Actor);
	if ((Interface == nullptr) && (!Actor->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto ActorID = Reflector::MakeActorID(Actor);
	if (ActorID.IsEqual(TEXT("ERROR:ID")))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	TArray<UStaticMeshComponent*>	Meshes;
	TArray<USkeletalMeshComponent*> Skins;
	//
	Actor->GetComponents<UStaticMeshComponent>(Meshes);
	Actor->GetComponents<USkeletalMeshComponent>(Skins);
	//
	for (auto CMP : Meshes)
	{
		const auto					Mesh = CastChecked<UStaticMeshComponent>(CMP);
		TArray<UMaterialInterface*> Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats)
		{
			if (!Mat->IsValidLowLevel())
			{
				continue;
			}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass()))
			{
				continue;
			}
			RestoreMaterialFromSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat), ActorID.ToString(), SlotData);
		}
	}
	//
	for (auto CMP : Skins)
	{
		const auto					Mesh = CastChecked<USkeletalMeshComponent>(CMP);
		TArray<UMaterialInterface*> Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats)
		{
			if (!Mat->IsValidLowLevel())
			{
				continue;
			}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass()))
			{
				continue;
			}
			RestoreMaterialFromSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat), ActorID.ToString(), SlotData);
		}
	}
}

void USavior::SaveActor(UObject* Context, AActor* Actor, ESaviorResult& Result)
{
	if ((USavior::ThreadSafety == EThreadSafety::SynchronousSaving) || (USavior::ThreadSafety == EThreadSafety::AsynchronousSaving))
	{
		USavior::SS_Complete++;
		USavior::SS_Progress = ((USavior::SS_Complete / USavior::SS_Workload) * 100);
	}
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	World = Context->GetWorld();
	//
	if (Actor == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (TAG_ANOSAVE(Actor))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed) || Actor->IsPendingKillPending())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto ActorID = Reflector::MakeActorID(Actor, true);
	const auto Record = GenerateRecord_Actor(Actor);
	SlotData.Records.Add(ActorID, Record);
	//
	if (IsInGameThread())
	{
		SerializeActorMaterials(this, Actor);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SerializeActorMaterials, this, Actor),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity), nullptr, ENamedThreads::GameThread);
	}
	//
	ESaviorResult AnimResult;
	if (Actor->IsA(ACharacter::StaticClass()))
	{
		SaveAnimation(Context, CastChecked<ACharacter>(Actor), AnimResult);
	}
	//
	if (IsInGameThread())
	{
		OnActorSaved(Actor);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnActorSaved, Actor),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::LoadActor(UObject* Context, AActor* Actor, ESaviorResult& Result)
{
	if ((USavior::ThreadSafety == EThreadSafety::SynchronousLoading) || (USavior::ThreadSafety == EThreadSafety::AsynchronousLoading))
	{
		USavior::SL_Complete++;
		USavior::SL_Progress = ((USavior::SL_Complete / USavior::SL_Workload) * 100);
	}
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (Actor == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (TAG_ANOLOAD(Actor))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed) || Actor->IsPendingKillPending())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	//
	FName ActorID = NAME_None;
	ActorID = Reflector::MakeActorID(Actor, true);
	if (!SlotData.Records.Contains(ActorID))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	auto Record = SlotData.Records.FindRef(ActorID);
	//
	if (Actor->IsA(APawn::StaticClass()))
	{
		Record.IgnoreTransform = IgnorePawnTransformOnLoad;
	}
	//
	if (!TAG_ANOKILL(Actor) && (Record.Destroyed))
	{
		MarkActorAutoDestroyed(Actor);
	}
	else
	{
		UnpackRecord_Actor(Record, Actor, Result);
		//
		if (IsInGameThread())
		{
			DeserializeActorMaterials(this, Actor);
		}
		else
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeActorMaterials, this, Actor),
				GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity), nullptr, ENamedThreads::GameThread);
		}
		//
		if (Actor->IsA(ACharacter::StaticClass()))
		{
			ESaviorResult AnimResult = ESaviorResult::Success;
			LoadAnimation(Context, CastChecked<ACharacter>(Actor), AnimResult);
		}
	}
	//
	if (IsInGameThread())
	{
		OnActorLoaded(SlotMeta, Actor);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnActorLoaded, SlotMeta, Actor),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

AActor* USavior::LoadActorWithGUID(UObject* Context, const FGuid& SGUID, ESaviorResult& Result)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return nullptr;
	}
	if (!SGUID.IsValid())
	{
		Result = ESaviorResult::Failed;
		return nullptr;
	}
	//
	World = Context->GetWorld();
	Result = ESaviorResult::Success;
	if (World == nullptr)
	{
		Result = ESaviorResult::Failed;
		return nullptr;
	}
	//
	AActor* Actor = nullptr;
	for (TActorIterator<AActor> ACT(World); ACT; ++ACT)
	{
		if (!(*ACT)->GetOutermost()->ContainsMap())
		{
			continue;
		}
		if (!(*ACT)->IsValidLowLevelFast() || !IsValid(*ACT))
		{
			continue;
		}
		if ((*ACT)->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		//
		if (TAG_ANOLOAD(*ACT))
		{
			continue;
		}
		//
		const FGuid UID = Reflector::FindGUID(*ACT);
		if (UID == SGUID)
		{
			Actor = (*ACT);
			break;
		}
	}
	//
	if (Actor == nullptr)
	{
		Result = ESaviorResult::Failed;
		LOG_SV(true, ESeverity::Warning,
			FString::Printf(TEXT("[Load Actor by GUID]: Couldn't find in World any Actor owner of this SGUID:  %s"),
				*SGUID.ToString()));
		return Actor;
	}
	//
	for (auto IT = SlotData.Records.CreateConstIterator(); IT; ++IT)
	{
		const FSaviorRecord& Record = IT->Value;
		//
		if (Record.GUID == SGUID)
		{
			if (!TAG_ANOKILL(Actor) && (Record.Destroyed))
			{
				MarkActorAutoDestroyed(Actor);
			}
			else
			{
				UnpackRecord_Actor(Record, Actor, Result);
			}
		}
	}
	//
	if (IsInGameThread())
	{
		OnActorLoaded(SlotMeta, Actor);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnActorLoaded, SlotMeta, Actor),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
	//
	return Actor;
}

void USavior::SaveActorHierarchy(UObject* Context, AActor* Actor, ESaviorResult& Result, ESaviorResult& HierarchyResult)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Actor == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	World = Context->GetWorld();
	SaveActor(World, Actor, Result);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	TArray<AActor*>		   Children;
	TArray<AActor*>		   Attached;
	TSet<UActorComponent*> Components;
	//
	Actor->GetAllChildActors(Children);
	Actor->GetAttachedActors(Attached);
	Components = Actor->GetComponents();
	//
	for (auto CMP : Components.Array())
	{
		SaveComponent(World, CMP, HierarchyResult);
	}
	for (auto Sub : Attached)
	{
		SaveActor(World, Sub, HierarchyResult);
		Components = Sub->GetComponents();
		for (auto CMP : Components.Array())
		{
			SaveComponent(World, CMP, HierarchyResult);
		}
	}
	for (auto Child : Children)
	{
		SaveActor(World, Child, HierarchyResult);
		Components = Child->GetComponents();
		for (auto CMP : Components.Array())
		{
			SaveComponent(World, CMP, HierarchyResult);
		}
	}
}

void USavior::LoadActorHierarchy(UObject* Context, AActor* Actor, const bool IgnoreTransform, ESaviorResult& Result,
	ESaviorResult& HierarchyResult)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Actor == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	bool TaggedIgnore = TAG_ANOTRAN(Actor);
	if ((!TaggedIgnore) && IgnoreTransform)
	{
		Actor->Tags.Add(TEXT("!Tran"));
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	World = Context->GetWorld();
	LoadActor(World, Actor, Result);
	//
	if ((!TaggedIgnore) && IgnoreTransform)
	{
		Actor->Tags.Remove(TEXT("!Tran"));
	}
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	TArray<AActor*>		   Children;
	TArray<AActor*>		   Attached;
	TSet<UActorComponent*> Components;
	//
	Actor->GetAllChildActors(Children);
	Actor->GetAttachedActors(Attached);
	Components = Actor->GetComponents();
	//
	for (auto CMP : Components.Array())
	{
		LoadComponent(World, CMP, HierarchyResult);
	}
	for (auto Sub : Attached)
	{
		LoadActor(World, Sub, HierarchyResult);
		Components = Sub->GetComponents();
		for (auto CMP : Components.Array())
		{
			LoadComponent(World, CMP, HierarchyResult);
		}
	}
	for (auto Child : Children)
	{
		LoadActor(World, Child, HierarchyResult);
		Components = Child->GetComponents();
		for (auto CMP : Components.Array())
		{
			LoadComponent(World, CMP, HierarchyResult);
		}
	}
}

void USavior::LoadActorHierarchyWithGUID(UObject* Context, const FGuid& SGUID, ESaviorResult& Result,
	ESaviorResult& HierarchyResult)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (!SGUID.IsValid())
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	World = Context->GetWorld();
	AActor* Actor = LoadActorWithGUID(World, SGUID, Result);
	//
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	TArray<AActor*>		   Children;
	TArray<AActor*>		   Attached;
	TSet<UActorComponent*> Components;
	//
	Actor->GetAllChildActors(Children);
	Actor->GetAttachedActors(Attached);
	Components = Actor->GetComponents();
	//
	for (auto CMP : Components.Array())
	{
		const FGuid UID = Reflector::FindGUID(CMP);
		if (!UID.IsValid())
		{
			continue;
		}
		LoadComponentWithGUID(World, UID, HierarchyResult);
	}
	//
	for (auto Sub : Attached)
	{
		const FGuid UID = Reflector::FindGUID(Sub);
		if (!UID.IsValid())
		{
			continue;
		}
		//
		for (auto IT = SlotData.Records.CreateConstIterator(); IT; ++IT)
		{
			const FSaviorRecord& Record = IT->Value;
			//
			if (Record.GUID == UID)
			{
				LoadActorHierarchyWithGUID(World, UID, Result, HierarchyResult);
				break;
			}
		}
		//
		TSet<UActorComponent*> SubCMP;
		SubCMP = Sub->GetComponents();
		//
		for (auto CMP : SubCMP.Array())
		{
			const FGuid CUID = Reflector::FindGUID(CMP);
			if (!CUID.IsValid())
			{
				continue;
			}
			LoadComponentWithGUID(World, CUID, HierarchyResult);
		}
	}
	//
	for (auto Child : Children)
	{
		const FGuid UID = Reflector::FindGUID(Child);
		if (!UID.IsValid())
		{
			continue;
		}
		//
		for (auto IT = SlotData.Records.CreateConstIterator(); IT; ++IT)
		{
			const FSaviorRecord& Record = IT->Value;
			//
			if (Record.GUID == UID)
			{
				LoadActorHierarchyWithGUID(World, UID, Result, HierarchyResult);
				break;
			}
		}
		//
		TSet<UActorComponent*> SubCMP;
		SubCMP = Child->GetComponents();
		//
		for (auto CMP : SubCMP.Array())
		{
			const FGuid CUID = Reflector::FindGUID(CMP);
			if (!CUID.IsValid())
			{
				continue;
			}
			//
			LoadComponentWithGUID(World, CUID, HierarchyResult);
		}
	}
}

void USavior::SaveActorsOfClass(UObject* Context, TSubclassOf<AActor> Class, const bool SaveHierarchy,
	ESaviorResult& Result, ESaviorResult& HierarchyResult)
{
	ESaviorResult AnySuccess = ESaviorResult::Failed;
	//
	if (Class.Get() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Failed;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	World = Context->GetWorld();
	//
	for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
	{
		if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		if (!Actor->IsA(Class.Get()))
		{
			continue;
		}
		//
		if (!SaveHierarchy)
		{
			SaveActor(World, (*Actor), AnySuccess);
		}
		else
		{
			SaveActorHierarchy(World, (*Actor), AnySuccess, HierarchyResult);
		}
		//
		Result = (AnySuccess == ESaviorResult::Success) ? AnySuccess : Result;
		HierarchyResult = (AnySuccess == ESaviorResult::Success) ? AnySuccess : HierarchyResult;
		if (SaveHierarchy)
		{
			Result = HierarchyResult;
		}
	}
}

void USavior::LoadActorsOfClass(UObject* Context, TSubclassOf<AActor> Class, const bool LoadHierarchy,
	ESaviorResult& Result, ESaviorResult& HierarchyResult)
{
	if (Class.Get() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	World = Context->GetWorld();
	//
	for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
	{
		if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		if (!Actor->IsA(Class.Get()))
		{
			continue;
		}
		//
		if (!LoadHierarchy)
		{
			LoadActor(World, *Actor, Result);
		}
		else
		{
			LoadActorHierarchy(World, *Actor, false, Result, HierarchyResult);
		}
	}
}

void USavior::SavePlayerState(UObject* Context, APlayerController* Controller, ESaviorResult& Result)
{
	if (Controller == nullptr || !IsValid(Controller))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (Controller->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto& Pawn = Controller->GetPawn();
	const auto& PlayerHUD = Controller->MyHUD;
	const auto& PlayerState = Controller->PlayerState;
	//
	Result = ESaviorResult::Success;
	SaveActor(Context->GetWorld(), Controller, Result);
	//
	if ((Pawn) && Pawn->IsValidLowLevelFast())
	{
		SaveActor(Context->GetWorld(), Pawn, Result);
	}
	if ((PlayerHUD) && PlayerHUD->IsValidLowLevelFast())
	{
		SaveActor(Context->GetWorld(), PlayerHUD, Result);
	}
	if ((PlayerState) && PlayerState->IsValidLowLevelFast())
	{
		SaveActor(Context->GetWorld(), PlayerState, Result);
	}
}

void USavior::LoadPlayerState(UObject* Context, APlayerController* Controller, ESaviorResult& Result)
{
	if (Controller == nullptr || !IsValid(Controller))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (Controller->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto& Pawn = Controller->GetPawn();
	const auto& PlayerHUD = Controller->MyHUD;
	const auto& PlayerState = Controller->PlayerState;
	//
	Result = ESaviorResult::Success;
	LoadActor(Context->GetWorld(), Controller, Result);
	//
	if ((Pawn) && Pawn->IsValidLowLevelFast())
	{
		LoadActor(Context->GetWorld(), Pawn, Result);
	}
	if ((PlayerHUD) && PlayerHUD->IsValidLowLevelFast())
	{
		LoadActor(Context->GetWorld(), PlayerHUD, Result);
	}
	if ((PlayerState) && PlayerState->IsValidLowLevelFast())
	{
		LoadActor(Context->GetWorld(), PlayerState, Result);
	}
}

void USavior::SaveGameInstanceSingleTon(UGameInstance* Instance, ESaviorResult& Result)
{
	if (Instance == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (Instance->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	FString	   Patch;
	const auto ObjectID = Instance->GetFName();
	ObjectID.ToString().Split(TEXT("_C"), &Patch, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	//
	const auto Record = GenerateRecord_Object(Instance);
	SlotData.Records.Add(*Patch, Record);
	//
	if (IsInGameThread())
	{
		OnGameInstanceSaved(Instance);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnGameInstanceSaved, Instance),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::LoadGameInstanceSingleTon(UGameInstance* Instance, ESaviorResult& Result)
{
	if (Instance == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (Instance->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	FString	   Patch;
	const auto ObjectID = Instance->GetFName();
	ObjectID.ToString().Split(TEXT("_C"), &Patch, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	//
	if (!SlotData.Records.Contains(*Patch))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	const auto Record = SlotData.Records.FindRef(*Patch);
	//
	if (Record.Destroyed)
	{
		Instance->ConditionalBeginDestroy();
	}
	else
	{
		UnpackRecord_Object(Record, Instance, Result);
	}
	//
	if (IsInGameThread())
	{
		OnGameInstanceLoaded(SlotMeta, Instance);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnGameInstanceLoaded, SlotMeta, Instance),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::LoadObjectData(UObject* Object, const FSaviorRecord& Data, ESaviorResult& Result)
{
	if (Object == nullptr || Object->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	USaviorSubsystem* System = nullptr;
	Result = ESaviorResult::Success;
	//
	if (UGameInstance* const& GI = Object->GetWorld()->GetGameInstance())
	{
		System = GI->GetSubsystem<USaviorSubsystem>();
	}
	//
	if (Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (Data.Destroyed)
	{
		Object->ConditionalBeginDestroy();
	}
	else
	{
		TSharedPtr<FJsonObject>		   JSON = MakeShareable(new FJsonObject);
		TSharedRef<TJsonReader<TCHAR>> JReader = TJsonReaderFactory<TCHAR>::Create(Data.Buffer);
		//
		if (!FJsonSerializer::Deserialize(JReader, JSON))
		{
			LOG_SV(
				true, ESeverity::Warning,
				FString::Printf(TEXT("[Data<->Buffer]: Corrupted Data. Unable to unpack Data Record for Object: [%s]"),
					*Object->GetName()));
		}
		//
		for (TFieldIterator<FProperty> PIT(Object->GetClass(), EFieldIteratorFlags::IncludeSuper,
				 EFieldIteratorFlags::ExcludeDeprecated,
				 EFieldIteratorFlags::ExcludeInterfaces);
			PIT; ++PIT)
		{
			FProperty* Property = *PIT;
			//
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame))
			{
				continue;
			}
			//
			const FString ID = Property->GetName();
			const bool	  IsSet = Property->IsA<FSetProperty>();
			const bool	  IsMap = Property->IsA<FMapProperty>();
			const bool	  IsInt = Property->IsA<FIntProperty>();
			const bool	  IsBool = Property->IsA<FBoolProperty>();
			const bool	  IsByte = Property->IsA<FByteProperty>();
			const bool	  IsEnum = Property->IsA<FEnumProperty>();
			const bool	  IsName = Property->IsA<FNameProperty>();
			const bool	  IsText = Property->IsA<FTextProperty>();
			const bool	  IsString = Property->IsA<FStrProperty>();
			const bool	  IsArray = Property->IsA<FArrayProperty>();
			const bool	  IsFloat = Property->IsA<FFloatProperty>();
			const bool	  IsInt64 = Property->IsA<FInt64Property>();
			const bool	  IsClass = Property->IsA<FClassProperty>();
			const bool	  IsDouble = Property->IsA<FDoubleProperty>();
			const bool	  IsStruct = Property->IsA<FStructProperty>();
			const bool	  IsObject = Property->IsA<FObjectProperty>();
			//
			if ((IsInt) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FIntProperty>(Property), Object);
				continue;
			}
			if ((IsBool) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FBoolProperty>(Property), Object);
				continue;
			}
			if ((IsByte) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FByteProperty>(Property), Object);
				continue;
			}
			if ((IsEnum) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FEnumProperty>(Property), Object);
				continue;
			}
			if ((IsName) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FNameProperty>(Property), Object);
				continue;
			}
			if ((IsText) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FTextProperty>(Property), Object);
				continue;
			}
			if ((IsString) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStrProperty>(Property), Object);
				continue;
			}
			if ((IsFloat) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FFloatProperty>(Property), Object);
				continue;
			}
			if ((IsInt64) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FInt64Property>(Property), Object);
				continue;
			}
			if ((IsDouble) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FDoubleProperty>(Property), Object);
				continue;
			}
			//
			if ((IsSet) && (JSON->HasField(ID)))
			{
				JSONToFProperty(System->StaticSlot, JSON, ID, CastFieldChecked<FSetProperty>(Property), Object);
				continue;
			}
			if ((IsMap) && (JSON->HasField(ID)))
			{
				JSONToFProperty(System->StaticSlot, JSON, ID, CastFieldChecked<FMapProperty>(Property), Object);
				continue;
			}
			if ((IsArray) && (JSON->HasField(ID)))
			{
				JSONToFProperty(System->StaticSlot, JSON, ID, CastFieldChecked<FArrayProperty>(Property), Object);
				continue;
			}
			if ((IsClass) && (JSON->HasField(ID)))
			{
				JSONToFProperty(System->StaticSlot, JSON, ID, CastFieldChecked<FClassProperty>(Property), Object);
				continue;
			}
			if ((IsObject) && (JSON->HasField(ID)))
			{
				JSONToFProperty(System->StaticSlot, JSON, ID, CastFieldChecked<FObjectProperty>(Property), Object);
				continue;
			}
			//
			if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FGuid>::Get()) && (Property->GetName().Equals(TEXT("SGUID"), ESearchCase::IgnoreCase)))
			{
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FTransform>::Get()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Transform);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FLinearColor>::Get()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::LColor);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FVector2D>::Get()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Vector2D);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == FRuntimeFloatCurve::StaticStruct()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Curve);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FRotator>::Get()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Rotator);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FVector>::Get()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Vector3D);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FColor>::Get()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::FColor);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FInstancedStruct>::Get()))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Generic);
				continue;
			}
			else if ((IsStruct) && (JSON->HasField(ID)))
			{
				JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Struct);
				continue;
			}
		}
		//
		//
		if (Object->IsA(UActorComponent::StaticClass()))
		{
			auto Component = CastChecked<UActorComponent>(Object);
			if (IsInGameThread())
			{
				DeserializeComponent(Data, Component);
			}
			else
			{
				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeComponent, Data, Component),
					GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
			}
			//
			FString ID = Reflector::MakeComponentID(Component).ToString();
			LOG_SV(System->StaticSlot->DeepLogs, ESeverity::Info,
				FString::Printf(TEXT("UNPACKED DATA for %s :: "), *ID) + Data.Buffer);
			LOG_SV(System->StaticSlot->Debug, ESeverity::Info, FString("Deserialized :: ") + ID);
		}
		else if (Object->IsA(AActor::StaticClass()))
		{
			auto Actor = CastChecked<AActor>(Object);
			if (IsInGameThread())
			{
				DeserializeActor(Data, Actor);
			}
			else
			{
				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeActor, Data, Actor),
					GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
			}
			//
			FString ID = Reflector::MakeActorID(Actor).ToString();
			LOG_SV(System->StaticSlot->DeepLogs, ESeverity::Info,
				FString::Printf(TEXT("UNPACKED DATA for %s :: "), *ID) + Data.Buffer);
			LOG_SV(System->StaticSlot->Debug, ESeverity::Info, FString("Deserialized :: ") + ID);
		}
		else
		{
			FString ID = Reflector::MakeObjectID(Object).ToString();
			LOG_SV(System->StaticSlot->DeepLogs, ESeverity::Info,
				FString::Printf(TEXT("UNPACKED DATA for %s :: "), *ID) + Data.Buffer);
			LOG_SV(System->StaticSlot->Debug, ESeverity::Info, FString("Deserialized :: ") + ID);
		}
	}
}

void USavior::LoadComponentData(UActorComponent* Component, const FSaviorRecord& Data, ESaviorResult& Result)
{
	if (Component == nullptr || Component->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (TAG_CNOLOAD(Component))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Component->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (!TAG_CNOKILL(Component) && (Data.Destroyed))
	{
		Component->DestroyComponent();
	}
	else
	{
		LoadObjectData(Component, Data, Result);
	}
}

void USavior::LoadActorData(AActor* Actor, const FSaviorRecord& Data, ESaviorResult& Result)
{
	if (Actor == nullptr || Actor->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (TAG_ANOLOAD(Actor))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (!TAG_ANOKILL(Actor) && (Data.Destroyed))
	{
		Actor->Destroy();
	}
	else
	{
		LoadObjectData(Actor, Data, Result);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior World Serialization Interface:

void USavior::OnFinishRespawnDynamicActors(const bool Success)
{
	ESaviorResult Result = ESaviorResult::Success;
	for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
	{
		if ((*OBJ) == nullptr || !OBJ->IsValidLowLevelFast())
		{
			continue;
		}
		if (OBJ->IsA(USavior::StaticClass()))
		{
			continue;
		}
		//
		if (OBJ->IsA(UGameInstance::StaticClass()))
		{
			LoadGameInstanceSingleTon(CastChecked<UGameInstance>(*OBJ), Result);
			continue;
		}
		//
		if (OBJ->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_DefaultSubObject))
		{
			continue;
		}
		//
		bool IsTarget = false;
		for (auto ACT : ActorScope)
		{
			if (ACT.Get() && (OBJ->IsA(ACT)))
			{
				IsTarget = true;
				break;
			}
		}
		for (auto OBS : ObjectScope)
		{
			if (OBS.Get() && (OBJ->IsA(OBS)))
			{
				IsTarget = true;
				break;
			}
		}
		for (auto COM : ComponentScope)
		{
			if (COM.Get() && (OBJ->IsA(COM)))
			{
				IsTarget = true;
				break;
			}
		}
		//
		if (IsTarget)
		{
			if (OBJ->IsA(UActorComponent::StaticClass()))
			{
				LoadComponent(this, CastChecked<UActorComponent>(*OBJ), Result);
			}
			else if (OBJ->IsA(AActor::StaticClass()))
			{
				LoadActor(this, CastChecked<AActor>(*OBJ), Result);
			}
			else
			{
				LoadObject((*OBJ), Result);
			}
		}
	}
	//
	//
	if (IsInGameThread())
	{
		OnFinishDeserializeGameWorld(this, Result);
	}
	else
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnFinishDeserializeGameWorld, this, Result),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeGameWorld), nullptr, ENamedThreads::GameThread);
	}
}

void USavior::SaveGameWorld(UObject* Context, ESaviorResult& Result)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = PrepareSlotToSave(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameWorld>(this))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameWorld>(this))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::LoadGameWorld(UObject* Context, const bool ResetLevelOnLoad, bool Absolute, FString Options, ESaviorResult& Result)
{
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	const auto& Settings = GetDefault<USaviorSettings>();
	//
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	USaviorSubsystem* System = nullptr;
	if (UGameInstance* const& GI = Context->GetWorld()->GetGameInstance())
	{
		System = GI->GetSubsystem<USaviorSubsystem>();
	}
	if (System == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	//
	FindSlotFile(Result);
	if (Result == ESaviorResult::Failed)
	{
		LOG_SV(true, ESeverity::Error, TEXT("Failed to read save file."));
		EVENT_OnFinishDataLOAD.Broadcast(false);
		AbortCurrentTask();
		return;
	}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	ReadSlotFromFile(Settings->DefaultPlayerID, Result);
	//
	if (Result == ESaviorResult::Failed)
	{
		LOG_SV(true, ESeverity::Error, TEXT("Failed to read save file."));
		AbortCurrentTask();
		return;
	}
	//
	if ((SlotMeta.SaveLocation != Context->GetWorld()->GetName()) || ResetLevelOnLoad)
	{
		ShadowCopySlot(this, System->StaticSlot, Result);
		UGameplayStatics::OpenLevel(Context, *SlotMeta.SaveLocation, Absolute, Options);
		return;
	}
	//
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameWorld>(this))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameWorld>(this))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::SaveLevel(UObject* Context, TSoftObjectPtr<UWorld> LevelToSave, bool WriteFile, ESaviorResult& Result)
{
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	FString _LevelName;
	LevelToSave.ToString().Split(TEXT("."), nullptr, &_LevelName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FName LevelName = (LevelToSave.IsNull()) ? TEXT("None") : *_LevelName;
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = PrepareSlotToSave(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeLevel>(this, LevelName, WriteFile))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeLevel>(this, LevelName, WriteFile))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::LoadLevel(UObject* Context, TSoftObjectPtr<UWorld> LevelToLoad, bool ReadFile, ESaviorResult& Result)
{
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	const auto& Settings = GetDefault<USaviorSettings>();
	//
	FString _LevelName;
	Result = ESaviorResult::Success;
	LevelToLoad.ToString().Split(TEXT("."), nullptr, &_LevelName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FName LevelName = (LevelToLoad.IsNull()) ? TEXT("None") : *_LevelName;
	//
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (ReadFile)
	{
		FindSlotFile(Result);
		if (Result == ESaviorResult::Failed)
		{
			EVENT_OnFinishDataLOAD.Broadcast(false);
			AbortCurrentTask();
			return;
		}
		//
		ReadSlotFromFile(Settings->DefaultPlayerID, Result);
		if (Result == ESaviorResult::Failed)
		{
			AbortCurrentTask();
			return;
		}
	}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeLevel>(this, LevelName))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeLevel>(this, LevelName))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::SaveWorldLayers(UObject* Context, ESaviorResult& Result)
{
	if (Context == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	TArray<const UDataLayerInstance*> DataLayers;
	GetDataLayers(DataLayers);
	//
	if (const UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(Context->GetWorld()))
	{
		for (const auto& Layer : DataLayers)
		{
			if (Layer && Layer->IsRuntime())
			{
				EDataLayerRuntimeState State = DataLayerManager->GetDataLayerInstanceRuntimeState(Layer);
				const FName			   LayerName = Layer->GetDataLayerFName();
				SlotData.DataLayers.Add(LayerName, State);
			}
		};
	}
	else
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
}

void USavior::LoadWorldLayers(UObject* Context, ESaviorResult& Result)
{
	if (Context == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	ESaviorResult					  HierarchyResult = ESaviorResult::Success;
	TArray<const UDataLayerInstance*> DataLayers;
	GetDataLayers(DataLayers);
	//
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(Context->GetWorld()))
	{
		for (const auto& Layer : DataLayers)
		{
			const FName LayerName = Layer->GetDataLayerFName();
			//
			if (!SlotData.DataLayers.Contains(LayerName))
			{
				continue;
			}
			//
			if (Layer && Layer->IsRuntime())
			{
				DataLayerManager->SetDataLayerInstanceRuntimeState(Layer, SlotData.DataLayers.FindRef(LayerName));
			}
		}
	}
	else
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
}

void USavior::SaveGameFeatures(UObject* Context, ESaviorResult& Result)
{
	UGameFeaturesSubsystem::Get().ForEachGameFeature(
		[this](FGameFeatureInfo&& FeatureInfo) -> void {
			FFeatureRecord Record;
			//
			Record.PluginURL = FeatureInfo.URL;
			Record.PluginName = FeatureInfo.Name;
			Record.IsActive = UGameFeaturesSubsystem::Get().IsGameFeaturePluginActive(FeatureInfo.URL);
			//
			SlotData.GameFeatures.Add(*Record.PluginName, Record);
			LOG_SV(Debug, ESeverity::Info, FString("Serialized Game Feature state :: ") + Record.PluginURL);
		});
	Result = ESaviorResult::Success;
}

void USavior::LoadGameFeatures(UObject* Context, ESaviorResult& Result)
{
	TArray<FFeatureRecord> Records{};
	SlotData.GameFeatures.GenerateValueArray(Records);
	//
	for (const auto& Feature : Records)
	{
		const EGameFeaturePluginProtocol Protocol = UGameFeaturesSubsystem::GetPluginURLProtocol(Feature.PluginURL);
		if (Protocol != EGameFeaturePluginProtocol::Unknown && Protocol != EGameFeaturePluginProtocol::Count)
		{
			if (Feature.IsActive && !UGameFeaturesSubsystem::Get().IsGameFeaturePluginActive(Feature.PluginURL))
			{
				if (IsInGameThread())
				{
					LoadAndActivateGameFeaturePlugin(Feature);
				}
				else
				{
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
						FSimpleDelegateGraphTask::FDelegate::CreateStatic(&LoadAndActivateGameFeaturePlugin, Feature),
						GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeObject), nullptr, ENamedThreads::GameThread);
				}
			}
			else if (UGameFeaturesSubsystem::Get().IsGameFeaturePluginActive(Feature.PluginURL))
			{
				UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(Feature.PluginURL);
			}
			LOG_SV(Debug, ESeverity::Info, FString("Deserialized Game Feature state :: ") + Feature.PluginURL);
		}
	}
	Result = ESaviorResult::Success;
}

void USavior::SaveGameMode(UObject* Context, bool WithGameInstance, ESaviorResult& Result)
{
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = PrepareSlotToSave(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameMode>(this, WithGameInstance))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameMode>(this, WithGameInstance))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::LoadGameMode(UObject* Context, bool WithGameInstance, ESaviorResult& Result)
{
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	const auto& Settings = GetDefault<USaviorSettings>();
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	FindSlotFile(Result);
	if (Result == ESaviorResult::Failed)
	{
		EVENT_OnFinishDataLOAD.Broadcast(false);
		AbortCurrentTask();
		return;
	}
	//
	ReadSlotFromFile(Settings->DefaultPlayerID, Result);
	if (Result == ESaviorResult::Failed)
	{
		AbortCurrentTask();
		return;
	}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameMode>(this, WithGameInstance))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameMode>(this, WithGameInstance))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::SaveGameInstance(UObject* Context, ESaviorResult& Result)
{
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = PrepareSlotToSave(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameInstance>(this))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameInstance>(this))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::LoadGameInstance(UObject* Context, ESaviorResult& Result)
{
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	const auto& Settings = GetDefault<USaviorSettings>();
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety != EThreadSafety::IsCurrentlyThreadSafe)
	{
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
		return;
	}
	//
	FindSlotFile(Result);
	if (Result == ESaviorResult::Failed)
	{
		EVENT_OnFinishDataLOAD.Broadcast(false);
		AbortCurrentTask();
		return;
	}
	//
	ReadSlotFromFile(Settings->DefaultPlayerID, Result);
	if (Result == ESaviorResult::Failed)
	{
		AbortCurrentTask();
		return;
	}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result == ESaviorResult::Failed)
	{
		return;
	}
	//
	//
	switch (RuntimeMode)
	{
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameInstance>(this))->StartSynchronousTask();
		}
		break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameInstance>(this))->StartBackgroundTask();
		}
		break;
		default:
			break;
	}
}

void USavior::StaticLoadGameWorld(UWorld* InWorld)
{
	if (!IsValid(InWorld))
	{
		return;
	}
	//
	USaviorSubsystem* System = nullptr;
	if (UGameInstance* const& GI = InWorld->GetGameInstance())
	{
		System = GI->GetSubsystem<USaviorSubsystem>();
	}
	if (System == nullptr)
	{
		return;
	}
	//
	ESaviorResult Result = ESaviorResult::Success;
	USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;
	if (System->StaticSlot->GetSaveLocation() == InWorld->GetName())
	{
		System->StaticSlot->SetWorld(InWorld);
		System->StaticSlot->LoadGameWorld(InWorld, false, false, FString{}, Result);
	}
}

void USavior::StaticLoadObject(UObject* Context, UObject* Object, ESaviorResult& Result)
{
	if (Object == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (Context == nullptr || Context->GetWorld() == nullptr || !IsValid(Context))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Object->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (UGameInstance* const& GI = Context->GetWorld()->GetGameInstance())
	{
		if (USaviorSubsystem* System = GI->GetSubsystem<USaviorSubsystem>())
		{
			System->StaticSlot->LoadObject(Object, Result);
		}
	}
}

void USavior::StaticLoadComponent(UObject* Context, UActorComponent* Component, ESaviorResult& Result)
{
	if (Component == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (TAG_CNOLOAD(Component))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Component->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (UGameInstance* const& GI = Context->GetWorld()->GetGameInstance())
	{
		if (USaviorSubsystem* System = GI->GetSubsystem<USaviorSubsystem>())
		{
			System->StaticSlot->LoadComponent(Context, Component, Result);
		}
	}
}

void USavior::StaticLoadActor(UObject* Context, AActor* Actor, ESaviorResult& Result)
{
	if (Actor == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	Result = ESaviorResult::Success;
	if (TAG_ANOLOAD(Actor))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (UGameInstance* const& GI = Context->GetWorld()->GetGameInstance())
	{
		if (USaviorSubsystem* System = GI->GetSubsystem<USaviorSubsystem>())
		{
			System->StaticSlot->LoadActor(Context, Actor, Result);
		}
	}
}

UMetaSlot* USavior::SlotMetaToOBJ()
{
	auto Meta = NewObject<UMetaSlot>(GetWorld(), TEXT("SlotMeta"));
	//
	Meta->Chapter = SlotMeta.Chapter;
	Meta->Progress = SlotMeta.Progress;
	Meta->PlayTime = SlotMeta.PlayTime;
	Meta->SaveDate = SlotMeta.SaveDate;
	Meta->PlayerName = SlotMeta.PlayerName;
	Meta->PlayerLevel = SlotMeta.PlayerLevel;
	Meta->SaveLocation = SlotMeta.SaveLocation;
	//
	return Meta;
}

UDataSlot* USavior::SlotDataToOBJ()
{
	auto Data = NewObject<UDataSlot>(GetWorld(), TEXT("SlotData"));
	//
	Data->Records = SlotData.Records;
	Data->Materials = SlotData.Materials;
	Data->DataLayers = SlotData.DataLayers;
	Data->ChaosObjects = SlotData.ChaosObjects;
	Data->GameFeatures = SlotData.GameFeatures;
	//
	return Data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Object Serialization Interface:

FSaviorRecord USavior::GenerateRecord_Object(const UObject* Object)
{
	FSaviorRecord Record;
	FString		  Buffer;
	//
	TSharedPtr<FJsonObject>											 JSON = MakeShareable(new FJsonObject);
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JBuffer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Buffer);
	//
	if (Object == nullptr)
	{
		return Record;
	}
	for (TFieldIterator<FProperty> PIT(Object->GetClass(), EFieldIteratorFlags::IncludeSuper,
			 EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::ExcludeInterfaces);
		PIT; ++PIT)
	{
		FProperty* Property = *PIT;
		//
		if (!IsSupportedProperty(Property))
		{
			continue;
		}
		if (Object->IsA(UHierarchicalInstancedStaticMeshComponent::StaticClass()))
		{
			if (Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
		}
		else if (Object->IsA(ULightComponent::StaticClass())
			|| Object->IsA(USkyAtmosphereComponent::StaticClass())
			|| Object->IsA(UExponentialHeightFogComponent::StaticClass())
			|| Object->IsA(UVolumetricCloudComponent::StaticClass()))
		{
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) || Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
		}
		else if ((Object->IsA(APostProcessVolume::StaticClass())))
		{
			if (Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
		}
		else if ((Object->IsA(UAbilitySystemComponent::StaticClass())))
		{
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame) && !Property->GetName().Equals(TEXT("SpawnedAttributes")))
			{
				continue;
			}
		}
		else
		{
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame))
			{
				continue;
			}
		}
		//
		const FString ID = Property->GetName();
		const bool	  IsSet = Property->IsA<FSetProperty>();
		const bool	  IsMap = Property->IsA<FMapProperty>();
		const bool	  IsInt = Property->IsA<FIntProperty>();
		const bool	  IsBool = Property->IsA<FBoolProperty>();
		const bool	  IsByte = Property->IsA<FByteProperty>();
		const bool	  IsEnum = Property->IsA<FEnumProperty>();
		const bool	  IsName = Property->IsA<FNameProperty>();
		const bool	  IsText = Property->IsA<FTextProperty>();
		const bool	  IsString = Property->IsA<FStrProperty>();
		const bool	  IsInt64 = Property->IsA<FInt64Property>();
		const bool	  IsArray = Property->IsA<FArrayProperty>();
		const bool	  IsFloat = Property->IsA<FFloatProperty>();
		const bool	  IsClass = Property->IsA<FClassProperty>();
		const bool	  IsDouble = Property->IsA<FDoubleProperty>();
		const bool	  IsStruct = Property->IsA<FStructProperty>();
		const bool	  IsObject = Property->IsA<FObjectProperty>();
		//
		if (IsBool && ID.Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase))
		{
			Record.Destroyed = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue_InContainer(Object);
			continue;
		}
		//
		if (IsInt)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FIntProperty>(Property), Object));
			continue;
		}
		if (IsBool)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FBoolProperty>(Property), Object));
			continue;
		}
		if (IsByte)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FByteProperty>(Property), Object));
			continue;
		}
		if (IsEnum)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FEnumProperty>(Property), Object));
			continue;
		}
		if (IsName)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FNameProperty>(Property), Object));
			continue;
		}
		if (IsText)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FTextProperty>(Property), Object));
			continue;
		}
		if (IsString)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FStrProperty>(Property), Object));
			continue;
		}
		if (IsFloat)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FFloatProperty>(Property), Object));
			continue;
		}
		if (IsInt64)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FInt64Property>(Property), Object));
			continue;
		}
		if (IsDouble)
		{
			JSON->SetField(ID, FPropertyToJSON(CastFieldChecked<FDoubleProperty>(Property), Object));
			continue;
		}
		if (IsClass)
		{
			JSON->SetObjectField(ID, FPropertyToJSON(CastFieldChecked<FClassProperty>(Property), Object));
			continue;
		}
		if (IsObject)
		{
			JSON->SetObjectField(ID, FPropertyToJSON(CastFieldChecked<FObjectProperty>(Property), Object));
			continue;
		}
		//
		if (IsSet)
		{
			JSON->SetArrayField(ID, FPropertyToJSON(CastFieldChecked<FSetProperty>(Property), Object));
			continue;
		}
		if (IsMap)
		{
			JSON->SetObjectField(ID, FPropertyToJSON(CastFieldChecked<FMapProperty>(Property), Object));
			continue;
		}
		if (IsArray)
		{
			JSON->SetArrayField(ID, FPropertyToJSON(CastFieldChecked<FArrayProperty>(Property), Object));
			continue;
		}
		//
		if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FGuid>::Get()) && (Property->GetName().Equals(TEXT("SGUID"), ESearchCase::IgnoreCase)))
		{
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FTransform>::Get()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::Transform));
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FLinearColor>::Get()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::LColor));
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FVector2D>::Get()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::Vector2D));
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == FRuntimeFloatCurve::StaticStruct()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::Curve));
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FRotator>::Get()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::Rotator));
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FVector>::Get()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::Vector3D));
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FColor>::Get()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::FColor));
			continue;
		}
		else if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FInstancedStruct>::Get()))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::Generic));
			continue;
		}
		else if ((IsStruct))
		{
			JSON->SetObjectField(
				ID, FPropertyToJSON(CastFieldChecked<FStructProperty>(Property), Object, EStructType::Struct));
			continue;
		}
	}
	FJsonSerializer::Serialize(JSON.ToSharedRef(), JBuffer);
	//
	//
	Record.Active = true;
	Record.GUID = FindGUID(Object);
	Record.ClassPath = Object->GetClass()->GetDefaultObject()->GetPathName();
	//
	const auto& Settings = GetDefault<USaviorSettings>();
	//
	if (Object->IsA(UActorComponent::StaticClass()))
	{
		auto		Component = CastChecked<UActorComponent>(Object);
		const auto& Scene = Cast<USceneComponent>(Component);
		//
		Record.FullName = Reflector::MakeComponentID(const_cast<UActorComponent*>(Component)).ToString();
		Record.Active = Component->IsActive();
		//
		if (const auto& MeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			Record.RootMesh = TSoftObjectPtr<UStaticMesh>(MeshComponent->GetStaticMesh()).ToString();
		}
		//
		if (!TAG_CNOTRAN(Component) && (Scene && Scene->Mobility == EComponentMobility::Movable))
		{
			Record.Scale = Scene->GetRelativeTransform().GetScale3D();
			Record.Location = Scene->GetRelativeTransform().GetLocation();
			Record.Rotation = Scene->GetRelativeTransform().GetRotation().Rotator();
		}
		//
		if (!TAG_CNODATA(Component))
		{
			Record.Buffer = Buffer;
		}
		if (TAG_CNOKILL(Component))
		{
			Record.Destroyed = false;
		}
		//
		LOG_SV(Debug, ESeverity::Info, FString("Serialized Component :: ") + Record.FullName);
	}
	else if (Object->IsA(AActor::StaticClass()))
	{
		auto Actor = CastChecked<AActor>(Object);
		//
		Record.FullName = Reflector::MakeActorID(const_cast<AActor*>(Actor), true).ToString();
		//
		if (const auto& Avatar = Cast<ACharacter>(Actor))
		{
			if (const auto& Skel = Avatar->GetMesh())
			{
				Record.RootMesh = TSoftObjectPtr<USkeletalMesh>(Skel->GetSkeletalMeshAsset()).ToString();
			}
		}
		else if (const auto& Mesh = Cast<UStaticMeshComponent>(Actor->GetRootComponent()))
		{
			Record.RootMesh = TSoftObjectPtr<UStaticMesh>(Mesh->GetStaticMesh()).ToString();
		}
		//
		if (!TAG_ANOTRAN(Actor))
		{
			AActor* Parent = Actor->GetParentActor();
			//
			if (Parent == nullptr)
			{
				Parent = Actor->GetAttachParentActor();
			}
			//
			if (Parent)
			{
				Record.Scale =
					Actor->GetActorTransform().GetRelativeTransform(Parent->GetActorTransform()).GetScale3D();
				Record.Location =
					Actor->GetActorTransform().GetRelativeTransform(Parent->GetActorTransform()).GetLocation();
				Record.Rotation = Actor->GetActorTransform()
									  .GetRelativeTransform(Parent->GetActorTransform())
									  .GetRotation()
									  .Rotator();
			}
			else
			{
				Record.Scale = Actor->GetActorTransform().GetScale3D();
				Record.Location = Actor->GetActorTransform().GetLocation();
				Record.Rotation = Actor->GetActorTransform().GetRotation().Rotator();
			}
			//
			if (auto* Root = Actor->GetRootComponent())
			{
				Record.Socket = Root->GetAttachSocketName();
			}
		}
		//
		if (!TAG_ANOPHYS(Actor) && Actor->GetRootComponent())
		{
			Record.LinearVelocity = Actor->GetVelocity();
			//
			if (const auto& Primitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
			{
				Record.AngularVelocity = Primitive->GetPhysicsAngularVelocityInDegrees();
				Record.Active = Primitive->IsActive();
			}
		}
		//
		if (!TAG_ANODATA(Actor))
		{
			Record.Buffer = Buffer;
		}
		if (TAG_ANOKILL(Actor))
		{
			Record.Destroyed = false;
		}
		if (!TAG_ANOHIDE(Actor))
		{
			Record.HiddenInGame = Actor->IsHidden();
		}
		//
		AActor* Parent = Actor->GetParentActor();
		{
			if (Parent == nullptr)
			{
				Parent = Actor->GetAttachParentActor();
			}
			//
			if (Parent)
			{
				Record.OuterName = Reflector::MakeActorID(Parent).ToString();
			}
		}
		//
		LOG_SV(Debug, ESeverity::Info, FString("Serialized Actor :: ") + Record.FullName);
	}
	else
	{
		Record.Buffer = Buffer;
	}
	//
	//
	LOG_SV(DeepLogs, ESeverity::Info,
		FString::Printf(TEXT("SAVED DATA for %s :: "),
			*Reflector::MakeObjectID(const_cast<UObject*>(Object)).ToString())
			+ Buffer);
	//
	return Record;
}

FMutableRecord USavior::GenerateRecord_Mutable(const UCustomizableObjectInstance* Mutable)
{
	FMutableRecord Record{};
	Record.GUID = FindGUID(Mutable);
	Record.State = Mutable->GetCurrentState();
	//
	UCustomizableObject* CDO = Mutable->GetCustomizableObject();
	int32 ModelParameterCount = CDO->GetParameterCount();
	//
	for (int32 ModelParameterIndex = 0; ModelParameterIndex < ModelParameterCount; ++ModelParameterIndex)
	{
		const FString& Name = CDO->GetParameterName(ModelParameterIndex);
		EMutableParameterType Type = CDO->GetParameterType(ModelParameterIndex);
		//
		switch (Type)
		{
			case EMutableParameterType::Bool:
			{
				bool Value = Mutable->GetBoolParameterSelectedOption(Name);
				FCustomizableObjectBoolParameterValue Param{};
				Param.ParameterValue = Value;
				Param.ParameterName = Name;
				Record.BoolParams.Add(Param);
			} break;
			//
			case EMutableParameterType::Float:
			{
				FCustomizableObjectFloatParameterValue Param{};
				const int32 ValueRange = Mutable->GetFloatValueRange(Name);
				//
				Param.ParameterName = Name;
				int32 ParamIndexInObject = CDO->FindParameter(Name);
				bool IsParamMultidimensional = CDO->IsParameterMultidimensional(ParamIndexInObject);
				//
				if (IsParamMultidimensional)
				{
					for (int32 i = 0; i < ValueRange; ++i)
					{
						Param.ParameterRangeValues.Add(Mutable->GetFloatParameterSelectedOption(Name, i));
					}
				}
				else
				{
					float Value = Mutable->GetFloatParameterSelectedOption(Name);
					Param.ParameterValue = Value;
				}
				//
				Record.FloatParams.Add(Param);
			} break;
			//
			case EMutableParameterType::Int:
			{
				FCustomizableObjectIntParameterValue Param{};
				const int32 ValueRange = Mutable->GetIntValueRange(Name);
				//
				Param.ParameterName = Name;
				int32 ParamIndexInObject = CDO->FindParameter(Name);
				bool IsParamMultidimensional = CDO->IsParameterMultidimensional(ParamIndexInObject);
				//
				if (IsParamMultidimensional)
				{
					for (int32 i = 0; i < ValueRange; ++i)
					{
						Param.ParameterRangeValueNames.Add(Mutable->GetIntParameterSelectedOption(Name, i));
					}
				}
				else
				{
					const FString ValueName = Mutable->GetIntParameterSelectedOption(Name);
					Param.ParameterValueName = ValueName;
				}
				//
				Record.IntParams.Add(Param);
			} break;
			//
			case EMutableParameterType::Color:
			{
				FLinearColor Value = Mutable->GetColorParameterSelectedOption(Name);
				FCustomizableObjectVectorParameterValue Param{};
				Param.ParameterValue = Value;
				Param.ParameterName = Name;
				Record.VectorParams.Add(Param);
			} break;
			//
			case EMutableParameterType::Transform:
			{
				FTransform Value = Mutable->GetTransformParameterSelectedOption(Name);
				FCustomizableObjectTransformParameterValue Param{};
				Param.ParameterValue = Value;
				Param.ParameterName = Name;
				Record.TransformParams.Add(Param);
			} break;
			//
			case EMutableParameterType::Texture:
			{
				FName ValueName = Mutable->GetTextureParameterSelectedOption(Name);
				FCustomizableObjectTextureParameterValue Param{};
				//
				TArray<FName>RangeValues;
				const int32 ValueRange = Mutable->GetTextureValueRange(Name);
				//
				Param.ParameterValue = ValueName;
				Param.ParameterName = Name;
				//
				for (int32 i = 0; i < ValueRange; ++i)
				{
					Param.ParameterRangeValues.Add(Mutable->GetTextureParameterSelectedOption(Name, i));
				}
				//
				Record.TextureParams.Add(Param);
			} break;
			//
			case EMutableParameterType::Projector:
			{
				FCustomizableObjectProjector Value = Mutable->GetProjector(Name, INDEX_NONE);
				FCustomizableObjectProjectorParameterValue Param{};
				//
				TArray<FCustomizableObjectProjector> RangeValues;
				const int32 ValueRange = Mutable->GetProjectorValueRange(Name);
				//
				for (int32 i = 0; i < ValueRange; ++i)
				{
					Param.RangeValues.Add(Mutable->GetProjector(Name, i));
				}
				//
				Record.ProjectorParams.Add(Param);
			} break;
		}
	}
	//
	LOG_SV(DeepLogs, ESeverity::Info,
		   FString::Printf(TEXT("SAVED DATA for %s :: "),
		   *Reflector::MakeObjectID(const_cast<UCustomizableObjectInstance*>(Mutable)).ToString()));
	//
	return Record;
}

FSaviorRecord USavior::GenerateRecord_Blackboard(const UBlackboardComponent* Component, const UBlackboardData* Blackboard)
{
	FSaviorRecord Record;
	FString		  Buffer;
	//
	TSharedPtr<FJsonObject>											 JSON = MakeShareable(new FJsonObject);
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JBuffer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Buffer);
	//
	if (Blackboard == nullptr || Component == nullptr)
	{
		return Record;
	}
	//
	for (const auto& Key : Blackboard->GetKeys())
	{
		const FString ID = Key.EntryName.ToString();
		const bool	  IsInt = Key.KeyType->IsA(UBlackboardKeyType_Int::StaticClass());
		const bool	  IsBool = Key.KeyType->IsA(UBlackboardKeyType_Bool::StaticClass());
		const bool	  IsEnum = Key.KeyType->IsA(UBlackboardKeyType_Enum::StaticClass());
		const bool	  IsName = Key.KeyType->IsA(UBlackboardKeyType_Name::StaticClass());
		const bool	  IsFloat = Key.KeyType->IsA(UBlackboardKeyType_Float::StaticClass());
		const bool	  IsClass = Key.KeyType->IsA(UBlackboardKeyType_Class::StaticClass());
		const bool	  IsObject = Key.KeyType->IsA(UBlackboardKeyType_Object::StaticClass());
		const bool	  IsString = Key.KeyType->IsA(UBlackboardKeyType_String::StaticClass());
		const bool	  IsVector = Key.KeyType->IsA(UBlackboardKeyType_Vector::StaticClass());
		const bool	  IsRotator = Key.KeyType->IsA(UBlackboardKeyType_Rotator::StaticClass());
		//
		if (IsBool)
		{
			bool Value = Component->GetValueAsBool(Key.EntryName);
			JSON->SetBoolField(ID, Value);
		}
		//
		if (IsInt)
		{
			int32 Value = Component->GetValueAsInt(Key.EntryName);
			JSON->SetNumberField(ID, Value);
		}
		//
		if (IsFloat)
		{
			float Value = Component->GetValueAsFloat(Key.EntryName);
			JSON->SetNumberField(ID, Value);
		}
		//
		if (IsEnum)
		{
			uint8 Value = Component->GetValueAsEnum(Key.EntryName);
			JSON->SetNumberField(ID, Value);
		}
		//
		if (IsName)
		{
			FName Value = Component->GetValueAsName(Key.EntryName);
			JSON->SetStringField(ID, Value.ToString());
		}
		//
		if (IsString)
		{
			FString Value = Component->GetValueAsString(Key.EntryName);
			JSON->SetStringField(ID, Value);
		}
		//
		if (IsVector)
		{
			TSharedPtr<FJsonObject> V3D = MakeShareable(new FJsonObject);
			FVector					Value = Component->GetValueAsVector(Key.EntryName);
			V3D->SetNumberField(TEXT("X"), Value.X);
			V3D->SetNumberField(TEXT("Y"), Value.Y);
			V3D->SetNumberField(TEXT("Z"), Value.Z);
			JSON->SetObjectField(ID, V3D);
		}
		//
		if (IsRotator)
		{
			TSharedPtr<FJsonObject> R3D = MakeShareable(new FJsonObject);
			FRotator				Value = Component->GetValueAsRotator(Key.EntryName);
			R3D->SetNumberField(TEXT("Pitch"), Value.Pitch);
			R3D->SetNumberField(TEXT("Yaw"), Value.Yaw);
			R3D->SetNumberField(TEXT("Roll"), Value.Roll);
			JSON->SetObjectField(ID, R3D);
		}
		//
		if (IsObject)
		{
			if (UObject* OBJ = Component->GetValueAsObject(Key.EntryName))
			{
				FString					FullName;
				TSharedPtr<FJsonObject> JSO = MakeShareable(new FJsonObject);
				const auto				ClassPath = OBJ->GetClass()->GetDefaultObject()->GetPathName();
				//
				if (OBJ->IsA(AActor::StaticClass()))
				{
					FullName = Reflector::MakeActorID(CastChecked<AActor>(OBJ), true).ToString();
				}
				else if (OBJ->IsA(UActorComponent::StaticClass()))
				{
					FullName = Reflector::MakeComponentID(CastChecked<UActorComponent>(OBJ), true).ToString();
				}
				else
				{
					FullName = Reflector::MakeObjectID(OBJ).ToString();
				}
				//
				JSO->SetStringField(TEXT("FullName"), FullName);
				JSO->SetStringField(TEXT("ClassPath"), ClassPath);
				JSON->SetObjectField(ID, JSO);
			}
		}
		//
		if (IsClass)
		{
			if (UClass* CLS = Component->GetValueAsClass(Key.EntryName))
			{
				TSharedPtr<FJsonObject> JSO = MakeShareable(new FJsonObject);
				const auto				ClassPath = CLS->GetDefaultObject()->GetPathName();
				JSO->SetStringField(TEXT("ClassPath"), ClassPath);
				JSON->SetObjectField(ID, JSO);
			}
		}
	}
	//
	if (!TAG_CNODATA(Component))
	{
		FJsonSerializer::Serialize(JSON.ToSharedRef(), JBuffer);
		Record.Buffer = Buffer;
	}
	//
	//
	LOG_SV(DeepLogs, ESeverity::Info,
		FString::Printf(TEXT("SAVED DATA for %s :: "),
			*Reflector::MakeObjectID(const_cast<UBlackboardData*>(Blackboard)).ToString())
			+ Buffer);
	//
	return Record;
}

FChaosGeometryRecord USavior::GenerateRecord_ChaosGeometry(const UGeometryCollectionComponent* Component)
{
	FChaosGeometryRecord Record;
	Record.CollectionID = Reflector::ParseGUIDfromHASH(Component->GetFullName());
	//
	if (const FGeometryDynamicCollection* DynamicCollection = Component->GetDynamicCollection())
	{
		const TManagedArray<uint8>& DynamicState = DynamicCollection->DynamicState;
		TArray<uint8>				DynamicStates{};
		//
		for (int32 I = 0; I < DynamicState.Num(); I++)
		{
			DynamicStates.Add(DynamicState[I]);
		}
		Record.DynamicStates.Append(DynamicStates);
		//
		TArray<FTransform3f> Transforms{};
		for (int32 Idx = 0; Idx < DynamicCollection->GetNumTransforms(); ++Idx)
		{
			const FTransform3f Transform = DynamicCollection->GetTransform(Idx);
			Transforms.Add(Transform);
		}
		Record.Transforms.Append(Transforms);
		//
		const TManagedArray<bool>& Particles = DynamicCollection->SimulatableParticles;
		TArray<bool>			   SimParticles{};
		//
		for (int32 I = 0; I < Particles.Num(); I++)
		{
			SimParticles.Add(Particles[I]);
		}
		Record.SimulatableParticles.Append(SimParticles);
		//
		const TManagedArray<bool>& Actives = DynamicCollection->Active;
		TArray<bool>			   SimActives{};
		//
		for (int32 I = 0; I < Actives.Num(); I++)
		{
			SimActives.Add(Actives[I]);
		}
		Record.ActiveStates.Append(SimActives);
	}
	//
	return Record;
}

FSaviorRecord USavior::GenerateRecord_Component(const UActorComponent* Component)
{
	return GenerateRecord_Object(Component);
}

FSaviorRecord USavior::GenerateRecord_Actor(const AActor* Actor)
{
	return GenerateRecord_Object(Actor);
}

void USavior::UnpackRecord_Object(const FSaviorRecord& Record, UObject* Object, ESaviorResult& Result)
{
	if (Object == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	Result = ESaviorResult::Success;
	//
	const auto&					   Settings = GetDefault<USaviorSettings>();
	TSharedPtr<FJsonObject>		   JSON = MakeShareable(new FJsonObject);
	TSharedRef<TJsonReader<TCHAR>> JReader = TJsonReaderFactory<TCHAR>::Create(Record.Buffer);
	//
	if (!FJsonSerializer::Deserialize(JReader, JSON))
	{
		LOG_SV(Debug, ESeverity::Warning,
			FString::Printf(TEXT("[Data<->Buffer]: Corrupted Data. Unable to unpack Data Record for Object: [%s]"),
				*Object->GetName()));
		Result = ESaviorResult::Failed;
		return;
	}
	//
	//
	for (TFieldIterator<FProperty> PIT(Object->GetClass(), EFieldIteratorFlags::IncludeSuper,
			 EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::ExcludeInterfaces);
		PIT; ++PIT)
	{
		FProperty* Property = *PIT;
		//
		if (!IsSupportedProperty(Property))
		{
			continue;
		}
		if (Object->IsA(UHierarchicalInstancedStaticMeshComponent::StaticClass()))
		{
			if (Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
		}
		else if (Object->IsA(ULightComponent::StaticClass())
			|| Object->IsA(USkyAtmosphereComponent::StaticClass())
			|| Object->IsA(UExponentialHeightFogComponent::StaticClass())
			|| Object->IsA(UVolumetricCloudComponent::StaticClass()))
		{
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) || Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
		}
		else if (Object->IsA(APostProcessVolume::StaticClass()))
		{
			if (Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
		}
		else if ((Object->IsA(UAbilitySystemComponent::StaticClass())))
		{
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame) && !Property->GetName().Equals(TEXT("SpawnedAttributes")))
			{
				continue;
			}
		}
		else
		{
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame))
			{
				continue;
			}
		}
		//
		const FString ID = Property->GetName();
		const bool	  IsSet = Property->IsA<FSetProperty>();
		const bool	  IsMap = Property->IsA<FMapProperty>();
		const bool	  IsInt = Property->IsA<FIntProperty>();
		const bool	  IsBool = Property->IsA<FBoolProperty>();
		const bool	  IsByte = Property->IsA<FByteProperty>();
		const bool	  IsEnum = Property->IsA<FEnumProperty>();
		const bool	  IsName = Property->IsA<FNameProperty>();
		const bool	  IsText = Property->IsA<FTextProperty>();
		const bool	  IsString = Property->IsA<FStrProperty>();
		const bool	  IsArray = Property->IsA<FArrayProperty>();
		const bool	  IsFloat = Property->IsA<FFloatProperty>();
		const bool	  IsInt64 = Property->IsA<FInt64Property>();
		const bool	  IsClass = Property->IsA<FClassProperty>();
		const bool	  IsDouble = Property->IsA<FDoubleProperty>();
		const bool	  IsStruct = Property->IsA<FStructProperty>();
		const bool	  IsObject = Property->IsA<FObjectProperty>();
		//
		if ((IsInt) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FIntProperty>(Property), Object);
			continue;
		}
		if ((IsBool) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FBoolProperty>(Property), Object);
			continue;
		}
		if ((IsByte) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FByteProperty>(Property), Object);
			continue;
		}
		if ((IsEnum) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FEnumProperty>(Property), Object);
			continue;
		}
		if ((IsName) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FNameProperty>(Property), Object);
			continue;
		}
		if ((IsText) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FTextProperty>(Property), Object);
			continue;
		}
		if ((IsString) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStrProperty>(Property), Object);
			continue;
		}
		if ((IsFloat) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FFloatProperty>(Property), Object);
			continue;
		}
		if ((IsInt64) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FInt64Property>(Property), Object);
			continue;
		}
		if ((IsDouble) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FDoubleProperty>(Property), Object);
			continue;
		}
		//
		if ((IsSet) && (JSON->HasField(ID)))
		{
			JSONToFProperty(this, JSON, ID, CastFieldChecked<FSetProperty>(Property), Object);
			continue;
		}
		if ((IsMap) && (JSON->HasField(ID)))
		{
			JSONToFProperty(this, JSON, ID, CastFieldChecked<FMapProperty>(Property), Object);
			continue;
		}
		if ((IsArray) && (JSON->HasField(ID)))
		{
			JSONToFProperty(this, JSON, ID, CastFieldChecked<FArrayProperty>(Property), Object);
			continue;
		}
		if ((IsClass) && (JSON->HasField(ID)))
		{
			JSONToFProperty(this, JSON, ID, CastFieldChecked<FClassProperty>(Property), Object);
			continue;
		}
		if ((IsObject) && (JSON->HasField(ID)))
		{
			JSONToFProperty(this, JSON, ID, CastFieldChecked<FObjectProperty>(Property), Object);
			continue;
		}
		//
		if ((IsStruct) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FGuid>::Get()) && (Property->GetName().Equals(TEXT("SGUID"), ESearchCase::IgnoreCase)))
		{
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FTransform>::Get()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Transform);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FLinearColor>::Get()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::LColor);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FVector2D>::Get()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Vector2D);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == FRuntimeFloatCurve::StaticStruct()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Curve);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FRotator>::Get()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Rotator);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FVector>::Get()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Vector3D);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FColor>::Get()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::FColor);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)) && (CastFieldChecked<FStructProperty>(Property)->Struct == TBaseStructure<FInstancedStruct>::Get()))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Generic);
			continue;
		}
		else if ((IsStruct) && (JSON->HasField(ID)))
		{
			JSONToFProperty(JSON, ID, CastFieldChecked<FStructProperty>(Property), Object, EStructType::Struct);
			continue;
		}
	}
	//
	//
	if (Object->IsA(UActorComponent::StaticClass()))
	{
		auto Component = CastChecked<UActorComponent>(Object);
		if (IsInGameThread())
		{
			DeserializeComponent(Record, Component);
		}
		else
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeComponent, Record, Component),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
		}
		//
		FString ID = Reflector::MakeComponentID(Component).ToString();
		LOG_SV(DeepLogs, ESeverity::Info, FString::Printf(TEXT("UNPACKED DATA for %s :: "), *ID) + Record.Buffer);
		LOG_SV(Debug, ESeverity::Info, FString("Deserialized :: ") + ID);
	}
	else if (Object->IsA(AActor::StaticClass()))
	{
		auto Actor = CastChecked<AActor>(Object);
		if (IsInGameThread())
		{
			DeserializeActor(Record, Actor);
		}
		else
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeActor, Record, Actor),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity), nullptr, ENamedThreads::GameThread);
		}
		//
		FString ID = Reflector::MakeActorID(Actor).ToString();
		LOG_SV(DeepLogs, ESeverity::Info, FString::Printf(TEXT("UNPACKED DATA for %s :: "), *ID) + Record.Buffer);
		LOG_SV(Debug, ESeverity::Info, FString("Deserialized :: ") + ID);
	}
	else
	{
		FString ID = Reflector::MakeObjectID(Object).ToString();
		LOG_SV(DeepLogs, ESeverity::Info, FString::Printf(TEXT("UNPACKED DATA for %s :: "), *ID) + Record.Buffer);
		LOG_SV(Debug, ESeverity::Info, FString("Deserialized :: ") + ID);
	}
}

void USavior::UnpackRecord_Mutable(const FMutableRecord& Record, UCustomizableObjectInstance* Mutable, ESaviorResult& Result)
{
	if (Mutable == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	Result = ESaviorResult::Success;
	//
	UCustomizableObject* CDO = Mutable->GetCustomizableObject();
	int32 ModelParameterCount = CDO->GetParameterCount();
	//
	Mutable->SetCurrentState(Record.State);
	for (int32 ModelParameterIndex = 0; ModelParameterIndex < ModelParameterCount; ++ModelParameterIndex)
	{
		const FString &Name = CDO->GetParameterName(ModelParameterIndex);
		EMutableParameterType Type = CDO->GetParameterType(ModelParameterIndex);
		//
		switch (Type)
		{
			case EMutableParameterType::Bool:
			{
				for (const FCustomizableObjectBoolParameterValue &P : Record.BoolParams)
				{
					if (P.ParameterName.Equals(Name))
					{
						bool Value = P.ParameterValue;
						Mutable->SetBoolParameterSelectedOption(Name, Value);
						break;
					}
				}
			} break;
			//
			case EMutableParameterType::Float:
			{
				for (const FCustomizableObjectFloatParameterValue &P : Record.FloatParams)
				{
					if (P.ParameterName == Name)
					{
						float Value = P.ParameterValue;
						TArray<float>ValueRange = P.ParameterRangeValues;
						Mutable->SetFloatParameterSelectedOption(Name, Value);
						//
						for (int32 i = 0; i < ValueRange.Num(); ++i)
						{
							Mutable->SetFloatParameterSelectedOption(Name, ValueRange[i], i);
						}
						break;
					}
				}
			} break;
			//
			case EMutableParameterType::Int:
			{
				int32 ParamIndexInObject = CDO->FindParameter(Name);
				const int32 InRange = Mutable->GetIntValueRange(Name);
				bool IsParamMultidimensional = CDO->IsParameterMultidimensional(ParamIndexInObject);
				//
				for (const FCustomizableObjectIntParameterValue &P : Record.IntParams)
				{
					if (P.ParameterName == Name)
					{
						FString Value = P.ParameterValueName;
						TArray<FString>ValueRange = P.ParameterRangeValueNames;
						Mutable->SetIntParameterSelectedOption(Name, Value);
						//
						if (IsParamMultidimensional)
						{
							for (int32 i = 0; i < InRange; ++i)
							{
								if (ValueRange.IsValidIndex(i))
									Mutable->SetIntParameterSelectedOption(Name, ValueRange[i], i);
							}
						}
						break;
					}
				}
			} break;
			//
			case EMutableParameterType::Color:
			{
				for (const FCustomizableObjectVectorParameterValue &P : Record.VectorParams)
				{
					if (P.ParameterName.Equals(Name))
					{
						FLinearColor Value = P.ParameterValue;
						Mutable->SetColorParameterSelectedOption(Name, Value);
						break;
					}
				}
			} break;
			//
			case EMutableParameterType::Transform:
			{
				for (const FCustomizableObjectTransformParameterValue &P : Record.TransformParams)
				{
					if (P.ParameterName.Equals(Name))
					{
						FTransform Value = P.ParameterValue;
						Mutable->SetTransformParameterSelectedOption(Name, Value);
						break;
					}
				}
			} break;
			//
			case EMutableParameterType::Texture:
			{
				for (const FCustomizableObjectTextureParameterValue &P : Record.TextureParams)
				{
					if (P.ParameterName.Equals(Name))
					{
						FString Value = P.ParameterValue.ToString();
						Mutable->SetTextureParameterSelectedOption(Name, Value);
						break;
					}
				}
			} break;
			//
			case EMutableParameterType::Projector:
			{
				for (const FCustomizableObjectProjectorParameterValue &P : Record.ProjectorParams)
				{
					if (P.ParameterName.Equals(Name))
					{
						FCustomizableObjectProjector Value = P.Value;
						TArray<FCustomizableObjectProjector>RangeValues = P.RangeValues;
						Mutable->SetProjectorValue(Name, static_cast<FVector>(Value.Position), static_cast<FVector>(Value.Direction),
												   static_cast<FVector>(Value.Up), static_cast<FVector>(Value.Scale), Value.Angle);
						//
						for (int32 i = 0; i < RangeValues.Num(); ++i)
						{
							Mutable->SetProjectorValue(Name, static_cast<FVector>(RangeValues[i].Position), static_cast<FVector>(RangeValues[i].Direction),
													   static_cast<FVector>(RangeValues[i].Up), static_cast<FVector>(RangeValues[i].Scale), RangeValues[i].Angle, i);
						}
						break;
					}
				}
			} break;
		}
	}
}

void USavior::UnpackRecord_Blackboard(const FSaviorRecord& Record, UBlackboardComponent* Component, UBlackboardData* Blackboard, ESaviorResult& Result)
{
	if (Blackboard == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	const auto&					   Settings = GetDefault<USaviorSettings>();
	TSharedPtr<FJsonObject>		   JSON = MakeShareable(new FJsonObject);
	TSharedRef<TJsonReader<TCHAR>> JReader = TJsonReaderFactory<TCHAR>::Create(Record.Buffer);
	Result = ESaviorResult::Success;
	//
	if (!FJsonSerializer::Deserialize(JReader, JSON))
	{
		LOG_SV(Debug, ESeverity::Warning,
			FString::Printf(TEXT("[Data<->Buffer]: Corrupted Data. Unable to unpack Data Record for Object: [%s]"),
				*Blackboard->GetName()));
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (Blackboard == nullptr || Component == nullptr)
	{
		return;
	}
	//
	for (const auto& Key : Blackboard->GetKeys())
	{
		const FString ID = Key.EntryName.ToString();
		const bool	  IsInt = Key.KeyType->IsA(UBlackboardKeyType_Int::StaticClass());
		const bool	  IsBool = Key.KeyType->IsA(UBlackboardKeyType_Bool::StaticClass());
		const bool	  IsEnum = Key.KeyType->IsA(UBlackboardKeyType_Enum::StaticClass());
		const bool	  IsName = Key.KeyType->IsA(UBlackboardKeyType_Name::StaticClass());
		const bool	  IsFloat = Key.KeyType->IsA(UBlackboardKeyType_Float::StaticClass());
		const bool	  IsClass = Key.KeyType->IsA(UBlackboardKeyType_Class::StaticClass());
		const bool	  IsObject = Key.KeyType->IsA(UBlackboardKeyType_Object::StaticClass());
		const bool	  IsString = Key.KeyType->IsA(UBlackboardKeyType_String::StaticClass());
		const bool	  IsVector = Key.KeyType->IsA(UBlackboardKeyType_Vector::StaticClass());
		const bool	  IsRotator = Key.KeyType->IsA(UBlackboardKeyType_Rotator::StaticClass());
		//
		if (IsBool)
		{
			bool Value = JSON->GetBoolField(ID);
			Component->SetValueAsBool(Key.EntryName, Value);
		}
		//
		if (IsInt)
		{
			int32 Value = JSON->GetIntegerField(ID);
			Component->SetValueAsInt(Key.EntryName, Value);
		}
		//
		if (IsFloat)
		{
			float Value = JSON->GetNumberField(ID);
			Component->SetValueAsFloat(Key.EntryName, Value);
		}
		//
		if (IsEnum)
		{
			uint8 Value = JSON->GetIntegerField(ID);
			Component->SetValueAsEnum(Key.EntryName, Value);
		}
		//
		if (IsName)
		{
			FString Value = JSON->GetStringField(ID);
			Component->SetValueAsString(Key.EntryName, Value);
		}
		//
		if (IsString)
		{
			FString Value = JSON->GetStringField(ID);
			Component->SetValueAsString(Key.EntryName, Value);
		}
		//
		if (IsVector)
		{
			const TSharedPtr<FJsonObject>* V3D{ nullptr };
			if (JSON->TryGetObjectField(ID, V3D))
			{
				FVector Value = FVector::ZeroVector;
				Value.X = (*V3D)->GetNumberField(TEXT("X"));
				Value.Y = (*V3D)->GetNumberField(TEXT("Y"));
				Value.Z = (*V3D)->GetNumberField(TEXT("Z"));
				Component->SetValueAsVector(Key.EntryName, Value);
			}
		}
		//
		if (IsRotator)
		{
			const TSharedPtr<FJsonObject>* R3D{ nullptr };
			if (JSON->TryGetObjectField(ID, R3D))
			{
				FRotator Value = FRotator::ZeroRotator;
				Value.Pitch = (*R3D)->GetNumberField(TEXT("Pitch"));
				Value.Yaw = (*R3D)->GetNumberField(TEXT("Yaw"));
				Value.Roll = (*R3D)->GetNumberField(TEXT("Roll"));
				Component->SetValueAsRotator(Key.EntryName, Value);
			}
		}
		//
		if (IsObject)
		{
			TSharedPtr<FJsonObject> JSO = JSON->GetObjectField(ID);
			const FString			Class = JSO->GetStringField(TEXT("ClassPath"));
			const FString			Instance = JSO->GetStringField(TEXT("FullName"));
			//
			const FSoftObjectPath	ClassPath(Class);
			TSoftObjectPtr<UObject> ClassPtr(ClassPath);
			//
			auto CDO = ClassPtr.Get();
			if ((CDO) && CDO->IsA(AActor::StaticClass()))
			{
				for (TObjectIterator<AActor> Actor; Actor; ++Actor)
				{
					if (!(*Actor)->GetOutermost()->ContainsMap())
					{
						continue;
					}
					if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed))
					{
						continue;
					}
					//
					const FString FullName = Reflector::MakeActorID(*Actor, true).ToString();
					if (FullName == Instance)
					{
						Component->SetValueAsObject(Key.EntryName, *Actor);
						break;
					}
				}
			}
			else if ((CDO) && CDO->IsA(UActorComponent::StaticClass()))
			{
				for (TObjectIterator<UActorComponent> CMP; CMP; ++CMP)
				{
					if (!(*CMP)->GetOutermost()->ContainsMap())
					{
						continue;
					}
					if ((*CMP)->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed))
					{
						continue;
					}
					//
					const FString FullName = Reflector::MakeComponentID(*CMP, true).ToString();
					if (FullName == Instance)
					{
						Component->SetValueAsObject(Key.EntryName, *CMP);
						break;
					}
				}
			}
			else if (CDO)
			{
				for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
				{
					if (!(*OBJ)->GetOutermost()->ContainsMap())
					{
						continue;
					}
					if ((*OBJ)->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed))
					{
						continue;
					}
					//
					const FString FullName = Reflector::MakeObjectID(*OBJ, true).ToString();
					if (FullName == Instance)
					{
						Component->SetValueAsObject(Key.EntryName, *OBJ);
						break;
					}
				}
			}
		}
		//
		if (IsClass)
		{
			TSharedPtr<FJsonObject> JSO = JSON->GetObjectField(ID);
			const FString			Class = JSO->GetStringField(TEXT("ClassPath"));
			const FSoftClassPath	ClassPath(Class);
			//
			if (ClassPath.IsValid())
			{
				Component->SetValueAsClass(Key.EntryName, ClassPath.ResolveClass());
			}
		}
	}
	//
	FString ID = Reflector::MakeObjectID(Blackboard).ToString();
	LOG_SV(DeepLogs, ESeverity::Info, FString::Printf(TEXT("UNPACKED DATA for %s :: "), *ID) + Record.Buffer);
	LOG_SV(Debug, ESeverity::Info, FString("Deserialized :: ") + ID);
}

void USavior::UnpackRecord_ChaosGeometry(const FChaosGeometryRecord& Record, UGeometryCollectionComponent* Component, ESaviorResult& Result)
{
	if (Component == nullptr)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	if (FGeometryDynamicCollection* DynamicCollection = Component->GetDynamicCollection())
	{
		int32			   Min = FMath::Min(DynamicCollection->GetNumTransforms(), Record.Transforms.Num());
		TArray<FTransform> RestTransforms{};
		for (int32 I = 0; I < Min; I++)
		{
			RestTransforms.Add(FTransform(Record.Transforms[I]));
			DynamicCollection->SetTransform(I, Record.Transforms[I]);
		}
		Component->SetRestState(MoveTemp(RestTransforms));
		//
		TManagedArray<uint8>& DynamicState = DynamicCollection->DynamicState;
		for (int32 I = 0; I < Record.DynamicStates.Num(); I++)
		{
			if (DynamicState.IsValidIndex(I))
			{
				DynamicState[I] = Record.DynamicStates[I];
			}
		}
		//
		TManagedArray<bool>& Actives = DynamicCollection->Active;
		for (int32 I = 0; I < Record.ActiveStates.Num(); I++)
		{
			if (Actives.IsValidIndex(I))
			{
				Actives[I] = Record.ActiveStates[I];
			}
		}
		//
		TManagedArray<bool>& Simulables = DynamicCollection->SimulatableParticles;
		for (int32 I = 0; I < Record.SimulatableParticles.Num(); I++)
		{
			if (Simulables.IsValidIndex(I))
			{
				Simulables[I] = Record.SimulatableParticles[I];
			}
		}
	}
}

void USavior::UnpackRecord_Component(const FSaviorRecord& Record, UActorComponent* Component, ESaviorResult& Result)
{
	UnpackRecord_Object(Record, Component, Result);
}

void USavior::UnpackRecord_Actor(const FSaviorRecord& Record, AActor* Actor, ESaviorResult& Result)
{
	UnpackRecord_Object(Record, Actor, Result);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Utilities:

USavior* USavior::NewSlotInstance(UObject* Context, USavior* Slot, ESaviorResult& Result)
{
	Result = ESaviorResult::Success;
	//
	if (!IsValid(Slot) || !IsValid(Context) || Context->GetWorld() == nullptr)
	{
		Result = ESaviorResult::Failed;
		return nullptr;
	}
	//
	USavior*		  Instance = nullptr;
	USaviorSubsystem* System = nullptr;
	if (UGameInstance* const& GI = Context->GetWorld()->GetGameInstance())
	{
		System = GI->GetSubsystem<USaviorSubsystem>();
		if (Slot == System->StaticSlot)
		{
			Slot->SetWorld(Context->GetWorld());
			return Slot;
		}
	}
	//
	if (System == nullptr)
	{
		return nullptr;
	}
	else
	{
		Instance = NewObject<USavior>(Context, Slot->GetClass(), *Slot->GetName(), RF_Standalone, Slot);
		Instance->SetWorld(Context->GetWorld());
	}
	//
	System->StaticSlot->AbortCurrentTask();
	System->StaticSlot->Reset();
	//
	ShadowCopySlot(Slot, System->StaticSlot, Result);
	return Instance;
}

USavior* USavior::ShadowCopySlot(USavior* From, USavior* To, ESaviorResult& Result)
{
	if (From == nullptr)
	{
		Result = ESaviorResult::Failed;
		return From;
	}
	if (To == nullptr)
	{
		Result = ESaviorResult::Failed;
		return From;
	}
	if (To == From)
	{
		Result = ESaviorResult::Success;
		return To;
	}
	//
	To->SlotFileName = FText::FromString(From->GetSlotName());
	To->SetSlotMeta(From->GetSlotMeta());
	To->SetSlotData(From->GetSlotData());
	Result = ESaviorResult::Success;
	//
	To->World = From->World;
	To->SlotThumbnail = From->SlotThumbnail;
	To->LevelThumbnails = From->LevelThumbnails;
	To->FeedbackFont = From->FeedbackFont;
	To->SplashStretch = From->SplashStretch;
	To->ProgressBarTint = From->ProgressBarTint;
	To->PauseGameOnLoad = From->PauseGameOnLoad;
	To->ProgressBarOnMovie = From->ProgressBarOnMovie;
	To->SplashMovie = From->SplashMovie;
	To->SplashImage = From->SplashImage;
	To->BackBlurPower = From->BackBlurPower;
	To->LoadScreenTimer = From->LoadScreenTimer;
	To->FeedbackLOAD = From->FeedbackLOAD;
	To->FeedbackSAVE = From->FeedbackSAVE;
	To->LoadScreenTrigger = From->LoadScreenTrigger;
	To->LoadScreenMode = From->LoadScreenMode;
	//
	To->ActorScope = From->ActorScope;
	To->ComponentScope = From->ComponentScope;
	To->ObjectScope = From->ObjectScope;
	To->RuntimeMode = From->RuntimeMode;
	To->DeepLogs = From->DeepLogs;
	To->Debug = From->Debug;
	//
	To->IgnorePawnTransformOnLoad = From->IgnorePawnTransformOnLoad;
	To->WriteMetaOnSave = From->WriteMetaOnSave;
	//
	To->EVENT_OnBeginDataLOAD = From->EVENT_OnBeginDataLOAD;
	To->EVENT_OnBeginDataSAVE = From->EVENT_OnBeginDataSAVE;
	To->EVENT_OnFinishDataLOAD = From->EVENT_OnFinishDataLOAD;
	To->EVENT_OnFinishDataSAVE = From->EVENT_OnFinishDataSAVE;
	//
	return To;
}

UObject* USavior::GetClassDefaultObject(UClass* Class)
{
	return Class->GetDefaultObject(true);
}

UObject* USavior::NewObjectInstance(UObject* Context, UClass* Class)
{
	if (!IsValid(Context))
	{
		return nullptr;
	}
	if (Class == nullptr)
	{
		return nullptr;
	}
	//
	auto World = GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		return NewObject<UObject>(Context, Class);
	}
	//
	return nullptr;
}

UObject* USavior::NewNamedObjectInstance(UObject* Context, UClass* Class, FName Name)
{
	if (!IsValid(Context))
	{
		return nullptr;
	}
	if (Class == nullptr)
	{
		return nullptr;
	}
	//
	auto World = GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		return NewObject<UObject>(Context, Class, Name);
	}
	//
	return nullptr;
}

FGuid USavior::NewObjectGUID()
{
	return FGuid::NewGuid();
}

FGuid USavior::CreateSGUID(UObject* Context)
{
	FGuid GUID = Reflector::FindGUID(Context);
	if (!GUID.IsValid())
	{
		GUID = FGuid::NewGuid();
	}
	//
	return GUID;
}

bool USavior::MatchesGUID(UObject* Context, UObject* ComparedTo)
{
	return Reflector::IsMatchingSGUID(Context, ComparedTo);
}

AActor* USavior::FindActorWithGUID(UObject* Context, const FGuid& SGUID)
{
	if (Context->GetWorld() == nullptr)
	{
		return nullptr;
	}
	//
	UWorld* World = Context->GetWorld();
	for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
	{
		if (Actor->HasAnyFlags(RF_BeginDestroyed))
		{
			continue;
		}
		//
		const FGuid GUID = Reflector::FindGUID(*Actor);
		if (GUID == SGUID)
		{
			return (*Actor);
		}
	}
	//
	return nullptr;
}

FGuid USavior::MakeInputSGUID(const FString Base)
{
	return FGuid(Base);
}

FGuid USavior::MakeLiteralSGUID(const int32 A, const int32 B, const int32 C, const int32 D)
{
	return FGuid((uint32)FMath::Abs(A), (uint32)FMath::Abs(B), (uint32)FMath::Abs(C), (uint32)FMath::Abs(D));
}

FGuid USavior::MakeObjectGUID(UObject* Object, const EGuidGeneratorMode Mode)
{
	if (Object == nullptr || (!Object->IsValidLowLevel()))
	{
		LOG_SV(true, ESeverity::Error, FString("Make Object SGUID:  target object is invalid."));
		return FGuid{};
	}
	UObject* Outer = Object->GetOuter();
	//
	FString Path = Object->GetFullName(Outer);
	Path = Reflector::SanitizeObjectPath(Path);
	//
	if (Path.Equals(TEXT("ERROR:ID")))
	{
		return FGuid(0, 0, 0, 0);
	}
	if (Object == nullptr)
	{
		return FGuid(0, 0, 0, 0);
	}
	//
	uint32		  iA = 0, iB = 0, iC = 0, iD = 0;
	const FString Hash = FMD5::HashAnsiString(*Path);
	{
		FString A{}, B{}, C{}, D{};
		for (int32 I = 0; I < Hash.Len(); ++I)
		{
			if (I <= 8)
			{
				A.AppendChar(Hash[I]);
				continue;
			}
			if ((I > 8) && (I <= 16))
			{
				B.AppendChar(Hash[I]);
				continue;
			}
			if ((I > 16) && (I <= 24))
			{
				C.AppendChar(Hash[I]);
				continue;
			}
			if (I > 24)
			{
				D.AppendChar(Hash[I]);
				continue;
			}
		}
		//
		for (const TCHAR& CH : A)
		{
			iA += CH;
		}
		for (const TCHAR& CH : B)
		{
			iB += CH;
		}
		for (const TCHAR& CH : C)
		{
			iC += CH;
		}
		for (const TCHAR& CH : D)
		{
			iD += CH;
		}
	}
	//
	switch (Mode)
	{
		case EGuidGeneratorMode::ResolvedNameToGUID:
		{
			FGuid GuidCheck = FGuid(iA, iB, iC, iD);
			for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
			{
				if (!OBJ->IsValidLowLevelFast())
				{
					continue;
				}
				if ((*OBJ)->GetClass() != Object->GetClass())
				{
					continue;
				}
				if ((*OBJ) == Object)
				{
					continue;
				}
				//
				const FGuid OGUID = Reflector::FindGUID(*OBJ);
				if (OGUID != GuidCheck)
				{
					continue;
				}
				//
				iA += 1;
				iB += 1;
				iC += 1;
				iD += 1;
			}
		}
		break;
		//
		case EGuidGeneratorMode::MemoryAddressToGUID:
		{
			const UPTRINT IntPtr = reinterpret_cast<UPTRINT>(Object);
			iA += IntPtr;
			iB += IntPtr;
			iC += IntPtr;
			iD += IntPtr;
		}
		break;
		default:
			break;
	}
	//
	return FGuid(iA, iB, iC, iD);
}

FGuid USavior::MakeComponentGUID(UActorComponent* Instance, const EGuidGeneratorMode Mode)
{
	if ((Instance == nullptr) || (!Instance->IsValidLowLevel()))
	{
		LOG_SV(true, ESeverity::Error, FString("Make Component SGUID:  target component is invalid."));
		return FGuid{};
	}
	AActor* Outer = Instance->GetTypedOuter<AActor>();
	//
	FString Path = Instance->GetFullName(Outer);
	Path = Reflector::SanitizeObjectPath(Path);
	//
	if (Path.Equals(TEXT("ERROR:ID")))
	{
		return FGuid(0, 0, 0, 0);
	}
	if (Instance == nullptr)
	{
		return FGuid(0, 0, 0, 0);
	}
	//
	uint32		  iA = 0, iB = 0, iC = 0, iD = 0;
	const FString Hash = FMD5::HashAnsiString(*Path);
	{
		FString A{}, B{}, C{}, D{};
		for (int32 I = 0; I < Hash.Len(); ++I)
		{
			if (I <= 8)
			{
				A.AppendChar(Hash[I]);
				continue;
			}
			if ((I > 8) && (I <= 16))
			{
				B.AppendChar(Hash[I]);
				continue;
			}
			if ((I > 16) && (I <= 24))
			{
				C.AppendChar(Hash[I]);
				continue;
			}
			if (I > 24)
			{
				D.AppendChar(Hash[I]);
				continue;
			}
		}
		//
		for (const TCHAR& CH : A)
		{
			iA += CH;
		}
		for (const TCHAR& CH : B)
		{
			iB += CH;
		}
		for (const TCHAR& CH : C)
		{
			iC += CH;
		}
		for (const TCHAR& CH : D)
		{
			iD += CH;
		}
	}
	//
	switch (Mode)
	{
		case EGuidGeneratorMode::ResolvedNameToGUID:
		{
			FGuid GuidCheck = FGuid(iA, iB, iC, iD);
			for (TObjectIterator<UActorComponent> CMP; CMP; ++CMP)
			{
				if ((*CMP)->GetClass() != Instance->GetClass())
				{
					continue;
				}
				if ((*CMP) == Instance)
				{
					continue;
				}
				//
				const FGuid OGUID = Reflector::FindGUID(*CMP);
				if (OGUID != GuidCheck)
				{
					continue;
				}
				//
				iA += 1;
				iB += 1;
				iC += 1;
				iD += 1;
			}
		}
		break;
		//
		case EGuidGeneratorMode::MemoryAddressToGUID:
		{
			const UPTRINT IntPtr = reinterpret_cast<UPTRINT>(Instance);
			iA += IntPtr;
			iB += IntPtr;
			iC += IntPtr;
			iD += IntPtr;
		}
		break;
		default:
			break;
	}
	//
	return FGuid(iA, iB, iC, iD);
}

FGuid USavior::MakeActorGUID(AActor* Instance, const EGuidGeneratorMode Mode)
{
	if ((Instance == nullptr) || (!Instance->IsValidLowLevel()))
	{
		LOG_SV(true, ESeverity::Error, FString("Make Actor SGUID:  target actor is invalid."));
		return FGuid{};
	}
	UWorld* Outer = Instance->GetTypedOuter<UWorld>();
	//
	FString Path = Instance->GetFullName(Outer);
	Path = Reflector::SanitizeObjectPath(Path);
	//
	if (Path.Equals(TEXT("ERROR:ID")))
	{
		return FGuid(0, 0, 0, 0);
	}
	if (Instance == nullptr)
	{
		return FGuid(0, 0, 0, 0);
	}
	//
	uint32		  iA = 0, iB = 0, iC = 0, iD = 0;
	const FString Hash = FMD5::HashAnsiString(*Path);
	{
		FString A{}, B{}, C{}, D{};
		for (int32 I = 0; I < Hash.Len(); ++I)
		{
			if (I <= 8)
			{
				A.AppendChar(Hash[I]);
				continue;
			}
			if ((I > 8) && (I <= 16))
			{
				B.AppendChar(Hash[I]);
				continue;
			}
			if ((I > 16) && (I <= 24))
			{
				C.AppendChar(Hash[I]);
				continue;
			}
			if (I > 24)
			{
				D.AppendChar(Hash[I]);
				continue;
			}
		}
		//
		for (const TCHAR& CH : A)
		{
			iA += CH;
		}
		for (const TCHAR& CH : B)
		{
			iB += CH;
		}
		for (const TCHAR& CH : C)
		{
			iC += CH;
		}
		for (const TCHAR& CH : D)
		{
			iD += CH;
		}
	}
	//
	switch (Mode)
	{
		case EGuidGeneratorMode::ResolvedNameToGUID:
		{
			if (Instance->GetWorld())
			{
				FGuid GuidCheck = FGuid(iA, iB, iC, iD);
				for (TActorIterator<AActor> ACT(Instance->GetWorld()); ACT; ++ACT)
				{
					if ((*ACT)->GetClass() != Instance->GetClass())
					{
						continue;
					}
					if ((*ACT) == Instance)
					{
						continue;
					}
					//
					const FGuid OGUID = Reflector::FindGUID(*ACT);
					if (OGUID != GuidCheck)
					{
						continue;
					}
					//
					iA += 1;
					iB += 1;
					iC += 1;
					iD += 1;
				}
			}
		}
		break;
		//
		case EGuidGeneratorMode::MemoryAddressToGUID:
		{
			const UPTRINT IntPtr = reinterpret_cast<UPTRINT>(Instance);
			iA += IntPtr;
			iB += IntPtr;
			iC += IntPtr;
			iD += IntPtr;
		}
		break;
		default:
			break;
	}
	//
	return FGuid(iA, iB, iC, iD);
}

FGuid USavior::GetObjectGUID(UObject* Instance)
{
	if (Instance == nullptr)
	{
		return FGuid();
	}
	//
	return Reflector::FindGUID(Instance);
}

FGuid USavior::GetComponentGUID(UActorComponent* Instance)
{
	if (Instance == nullptr)
	{
		return FGuid();
	}
	//
	return Reflector::FindGUID(Instance);
}

FGuid USavior::GetActorGUID(AActor* Instance)
{
	if (Instance == nullptr)
	{
		return FGuid();
	}
	//
	return Reflector::FindGUID(Instance);
}

float USavior::CalculateWorkload() const
{
	float Workload = 0.f;
	//
	for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
	{
		if (!IsValid(*OBJ))
		{
			continue;
		}
		if (!(*OBJ)->GetOutermost()->ContainsMap())
		{
			continue;
		}
		if ((*OBJ)->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		//
		if ((*OBJ)->IsA(AActor::StaticClass()))
		{
			for (auto ACT : ActorScope)
			{
				if (ACT.Get() == nullptr)
				{
					continue;
				}
				if ((*OBJ)->IsA(ACT))
				{
					Workload++;
					break;
				}
			}
		}
		else if ((*OBJ)->IsA(UActorComponent::StaticClass()))
		{
			for (auto COM : ComponentScope)
			{
				if (COM.Get() == nullptr)
				{
					continue;
				}
				if ((*OBJ)->IsA(COM))
				{
					Workload++;
					break;
				}
			}
		}
		else
		{
			for (auto OBS : ObjectScope)
			{
				if (OBS.Get() == nullptr)
				{
					continue;
				}
				if ((*OBJ)->IsA(OBS))
				{
					Workload++;
					break;
				}
			}
		}
	}
	//
	return Workload;
}

void USavior::ClearWorkload()
{
	USavior::SS_Workload = 0;
	USavior::SS_Complete = 0;
	USavior::SS_Progress = 100.f;
	USavior::SL_Workload = 0;
	USavior::SL_Complete = 0;
	USavior::SL_Progress = 100.f;
	//
	//
	for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
	{
		if (!IsValid(*Actor))
		{
			continue;
		}
		if (!(*Actor)->GetOutermost()->ContainsMap())
		{
			continue;
		}
		if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		//
		if (IsActorMarkedAutoDestroy(*Actor))
		{
			(*Actor)->SetActorTickEnabled(false);
			(*Actor)->SetActorHiddenInGame(true);
			(*Actor)->SetActorEnableCollision(false);
			(*Actor)->Destroy();
		}
	}
	//
	for (TObjectIterator<UObject> OBJ; OBJ; ++OBJ)
	{
		if (!IsValid(*OBJ))
		{
			continue;
		}
		if ((*OBJ)->IsA(AActor::StaticClass()))
		{
			continue;
		}
		if ((*OBJ)->IsA(UActorComponent::StaticClass()))
		{
			continue;
		}
		//
		if (!(*OBJ)->GetOutermost()->ContainsMap())
		{
			continue;
		}
		if ((*OBJ)->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed))
		{
			continue;
		}
		//
		if (IsObjectMarkedAutoDestroy(*OBJ))
		{
			(*OBJ)->BeginDestroy();
		}
	}
	//
	//
	USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;
	USavior::LastThreadState = EThreadSafety::IsCurrentlyThreadSafe;
	//
	if (World)
	{
		static FTimerDelegate TimerDelegate = FTimerDelegate::CreateStatic(&StaticRemoveLoadScreen);
		World->GetTimerManager().SetTimer(USavior::TH_LoadScreen, TimerDelegate, LoadScreenTimer, false);
	}
	else
	{
		RemoveLoadScreen();
	}
}

void USavior::Reset()
{
	SlotFileName = FText{};
	//
	SlotData.Records.Empty();
	SlotData.Materials.Empty();
	SlotData.DataLayers.Empty();
	SlotData.ChaosObjects.Empty();
	SlotData.GameFeatures.Empty();
	//
	SlotMeta.Progress = 0;
	SlotMeta.PlayTime = 0;
	SlotMeta.PlayerLevel = 0;
	SlotMeta.Chapter.Empty();
	SlotMeta.PlayerName.Empty();
	SlotMeta.SaveLocation.Empty();
	SlotMeta.SaveDate = FDateTime::Now();
	//
	SlotMeta = FSlotMeta();
	SlotData = FSlotData();
	//
	WriteMetaOnSave = true;
	IgnorePawnTransformOnLoad = false;
}

ESaviorResult USavior::MarkObjectAutoDestroyed(UObject* Object)
{
	if (IsValid(Object))
	{
		bool HasProperty = false;
		//
		for (TFieldIterator<FProperty> PIT(Object->GetClass(), EFieldIteratorFlags::IncludeSuper,
				 EFieldIteratorFlags::ExcludeDeprecated,
				 EFieldIteratorFlags::ExcludeInterfaces);
			PIT; ++PIT)
		{
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase))
			{
				auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
				CastFieldChecked<FBoolProperty>(Property)->SetPropertyValue(ValuePtr, true);
				HasProperty = true;
				break;
			}
		}
		//
		if (!HasProperty)
		{
			LOG_SV(true, ESeverity::Warning,
				FString::Printf(TEXT("'Auto-Destroy' function requires Target Object to have a Boolean Property "
									 "named 'Destroyed'. Property wasn't found in this Class:  %s"),
					*Object->GetClass()->GetName()));
			return ESaviorResult::Failed;
		}
		//
		const auto& Interface = Cast<ISAVIOR_Serializable>(Object);
		if (Interface)
		{
			Interface->Execute_OnMarkedAutoDestroy(Object);
		}
		else if (Object->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass()))
		{
			ISAVIOR_Serializable::Execute_OnMarkedAutoDestroy(Object);
		}
	}
	return ESaviorResult::Success;
}

bool USavior::IsObjectMarkedAutoDestroy(UObject* Object)
{
	if (IsValid(Object))
	{
		bool IsMarked = false;
		//
		for (TFieldIterator<FProperty> PIT(Object->GetClass(), EFieldIteratorFlags::IncludeSuper,
				 EFieldIteratorFlags::ExcludeDeprecated,
				 EFieldIteratorFlags::ExcludeInterfaces);
			PIT; ++PIT)
		{
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			//
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase))
			{
				const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
				IsMarked = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue(ValuePtr);
				break;
			}
		}
		//
		return IsMarked;
	}
	return false;
}

ESaviorResult USavior::MarkComponentAutoDestroyed(UActorComponent* Component)
{
	if (IsValid(Component))
	{
		bool HasProperty = false;
		//
		for (TFieldIterator<FProperty> PIT(Component->GetClass(), EFieldIteratorFlags::IncludeSuper,
				 EFieldIteratorFlags::ExcludeDeprecated,
				 EFieldIteratorFlags::ExcludeInterfaces);
			PIT; ++PIT)
		{
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase))
			{
				auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
				CastFieldChecked<FBoolProperty>(Property)->SetPropertyValue(ValuePtr, true);
				HasProperty = true;
				break;
			}
		}
		//
		if (!HasProperty)
		{
			LOG_SV(true, ESeverity::Warning,
				FString::Printf(TEXT("'Auto-Destroy' function requires Target Component to have a Boolean Property "
									 "named 'Destroyed'. Property wasn't found in this Class:  %s"),
					*Component->GetClass()->GetName()));
			return ESaviorResult::Failed;
		}
		//
		const auto& Interface = Cast<ISAVIOR_Serializable>(Component);
		if (Interface)
		{
			Interface->Execute_OnMarkedAutoDestroy(Component);
		}
		else if (Component->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass()))
		{
			ISAVIOR_Serializable::Execute_OnMarkedAutoDestroy(Component);
		}
	}
	return ESaviorResult::Success;
}

bool USavior::IsComponentMarkedAutoDestroy(UActorComponent* Component)
{
	if (IsValid(Component))
	{
		bool IsMarked = false;
		//
		for (TFieldIterator<FProperty> PIT(Component->GetClass(), EFieldIteratorFlags::IncludeSuper,
				 EFieldIteratorFlags::ExcludeDeprecated,
				 EFieldIteratorFlags::ExcludeInterfaces);
			PIT; ++PIT)
		{
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			//
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase))
			{
				const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
				IsMarked = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue(ValuePtr);
				break;
			}
		}
		//
		return IsMarked;
	}
	return false;
}

ESaviorResult USavior::MarkActorAutoDestroyed(AActor* Actor)
{
	if (IsValid(Actor))
	{
		bool HasProperty = false;
		//
		for (TFieldIterator<FProperty> PIT(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper,
				 EFieldIteratorFlags::ExcludeDeprecated,
				 EFieldIteratorFlags::ExcludeInterfaces);
			PIT; ++PIT)
		{
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase))
			{
				auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
				CastFieldChecked<FBoolProperty>(Property)->SetPropertyValue(ValuePtr, true);
				HasProperty = true;
				break;
			}
		}
		//
		if (!HasProperty)
		{
			LOG_SV(true, ESeverity::Warning,
				FString::Printf(TEXT("'Auto-Destroy' function requires Target Actor to have a Boolean Property "
									 "named 'Destroyed'. Property wasn't found in this Class:  %s"),
					*Actor->GetClass()->GetName()));
			return ESaviorResult::Failed;
		}
		//
		const auto& Interface = Cast<ISAVIOR_Serializable>(Actor);
		if (Interface)
		{
			Interface->Execute_OnMarkedAutoDestroy(Actor);
		}
		else if (Actor->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass()))
		{
			ISAVIOR_Serializable::Execute_OnMarkedAutoDestroy(Actor);
		}
	}
	return ESaviorResult::Success;
}

bool USavior::IsActorMarkedAutoDestroy(AActor* Actor)
{
	if (IsValid(Actor))
	{
		bool IsMarked = false;
		//
		for (TFieldIterator<FProperty> PIT(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper,
				 EFieldIteratorFlags::ExcludeDeprecated,
				 EFieldIteratorFlags::ExcludeInterfaces);
			PIT; ++PIT)
		{
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			//
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase))
			{
				const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
				IsMarked = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue(ValuePtr);
				break;
			}
		}
		//
		return IsMarked;
	}
	return false;
}

void USavior::SetDefaultPlayerID(const int32 NewID)
{
	const auto& Settings = GetMutableDefault<USaviorSettings>();
	//
	const uint32 ID = (NewID < 0) ? 0 : NewID;
	if (Settings)
	{
		Settings->DefaultPlayerID = ID;
	}
}

void USavior::SetDefaultPlayerLevel(const int32 NewLevel)
{
	const auto& Settings = GetMutableDefault<USaviorSettings>();
	//
	const uint32 Level = (NewLevel < 0) ? 0 : NewLevel;
	if (Settings)
	{
		Settings->DefaultPlayerID = Level;
	}
}

void USavior::SetDefaultPlayerName(const FString NewName)
{
	const auto& Settings = GetMutableDefault<USaviorSettings>();
	if (Settings)
	{
		Settings->DefaultPlayerName = NewName;
	}
}

void USavior::SetDefaultChapter(const FString NewChapter)
{
	const auto& Settings = GetMutableDefault<USaviorSettings>();
	if (Settings)
	{
		Settings->DefaultChapter = NewChapter;
	}
}

void USavior::SetDefaultLocation(const FString NewLocation)
{
	const auto& Settings = GetMutableDefault<USaviorSettings>();
	if (Settings)
	{
		Settings->DefaultLocation = NewLocation;
	}
}

UTexture2D* USavior::GetSlotThumbnail(FVector2D& TextureSize)
{
	FString Name;
	//
	for (auto Level : LevelThumbnails)
	{
		if (Level.Value.IsNull())
		{
			continue;
		}
		Level.Key.ToString().Split(TEXT("."), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		//
		if (Name == SlotMeta.SaveLocation)
		{
			SlotThumbnail = Level.Value.ToSoftObjectPath();
			break;
		}
	}
	//
	auto OBJ = SlotThumbnail.TryLoad();
	if (UTexture2D* Image = Cast<UTexture2D>(OBJ))
	{
		TextureSize = FVector2D(Image->GetSizeX(), Image->GetSizeY());
		return Image;
	}
	//
	return nullptr;
}

FString USavior::GetSaveTimeISO()
{
	FString S;
	//
	SlotMeta.SaveDate.ToIso8601().Split("T", nullptr, &S);
	S.Split(".", &S, nullptr);
	//
	return S;
}

FString USavior::GetSaveDateISO()
{
	FString S;
	//
	SlotMeta.SaveDate.ToIso8601().Split("T", &S, nullptr);
	//
	return S;
}

const FSlotMeta& USavior::GetSlotMetaData() const
{
	return SlotMeta;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FName USavior::MakeActorID(AActor* Actor, bool FullPathID)
{
	return Reflector::MakeActorID(Actor, FullPathID);
}

FName USavior::MakeComponentID(UActorComponent* Component, bool FullPathID)
{
	return Reflector::MakeComponentID(Component, FullPathID);
}

FName USavior::MakeObjectID(UObject* Object, bool FullPathID)
{
	return Reflector::MakeObjectID(Object, FullPathID);
}

bool USavior::IsRecordEmpty(const FSaviorRecord& Record)
{
	return (Record.Buffer.IsEmpty() && Record.FullName.IsEmpty() && Record.ClassPath.IsEmpty() && Record.RootMesh.IsEmpty() && Record.OuterName.IsEmpty() && (Record.Scale == FVector::ZeroVector) && (Record.Location == FVector::ZeroVector) && (Record.Rotation == FRotator::ZeroRotator) && (Record.LinearVelocity == FVector::ZeroVector) && (Record.AngularVelocity == FVector::ZeroVector) && (!Record.GUID.IsValid()));
}

bool USavior::IsRecordMarkedDestroy(const FSaviorRecord& Record)
{
	return Record.Destroyed;
}

TMap<FName, FSaviorRecord> USavior::GetSlotDataCopy() const
{
	return SlotData.Records;
}

FSaviorRecord USavior::FindRecordByName(const FName& ID) const
{
	return SlotData.Records.FindRef(ID);
}

FSaviorRecord USavior::FindRecordByGUID(const FGuid& ID) const
{
	for (auto IT = SlotData.Records.CreateConstIterator(); IT; ++IT)
	{
		const FSaviorRecord& Record = IT->Value;
		if (Record.GUID == ID)
		{
			return Record;
		}
	}
	//
	return FSaviorRecord{};
}

void USavior::AppendDataRecords(const TMap<FName, FSaviorRecord>& Records, ESaviorResult& Result)
{
	if (Records.Num() == 0)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	SlotData.Records.Append(Records);
	Result = ESaviorResult::Success;
}

void USavior::InsertDataRecord(const FName& ID, const FSaviorRecord& Record, ESaviorResult& Result)
{
	if (IsRecordEmpty(Record))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	if (ID == NAME_None)
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	SlotData.Records.Add(ID, Record);
	Result = ESaviorResult::Success;
}

void USavior::RemoveDataRecordByName(const FName& ID, ESaviorResult& Result)
{
	if (!SlotData.Records.Contains(ID))
	{
		Result = ESaviorResult::Failed;
		return;
	}
	//
	SlotData.Records.Remove(ID);
	Result = ESaviorResult::Success;
}

void USavior::RemoveDataRecordByGUID(const FGuid& ID, ESaviorResult& Result)
{
	Result = ESaviorResult::Failed;
	FName Item = NAME_None;

	for (auto IT = SlotData.Records.CreateConstIterator(); IT; ++IT)
	{
		const FSaviorRecord& Record = IT->Value;
		if (Record.GUID == ID)
		{
			Item = IT->Key;
			break;
		}
	}

	if (!Item.IsNone())
	{
		SlotData.Records.Remove(Item);
		Result = ESaviorResult::Success;
	}
}

void USavior::RemoveDataRecordByRef(const FSaviorRecord& Record, ESaviorResult& Result)
{
	FName Item = NAME_None;
	//
	for (auto IT = SlotData.Records.CreateConstIterator(); IT; ++IT)
	{
		const FSaviorRecord& ITR = IT->Value;
		if (ITR == Record)
		{
			Item = IT->Key;
			break;
		}
	}
	//
	if (!Item.IsNone())
	{
		SlotData.Records.Remove(Item);
		Result = ESaviorResult::Success;
	}
	//
	Result = ESaviorResult::Failed;
}

bool USavior::IsWorldPartitioned(UObject* Context)
{
	if (Context == nullptr || Context->GetWorld() == nullptr)
	{
		return false;
	}
	if (!Context->GetWorld()->PersistentLevel)
	{
		return false;
	}
	//
	return (Context->GetWorld()->PersistentLevel->bIsPartitioned);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ParseActorTAG(const AActor* Actor, const FName Tag)
{
	return Actor->ActorHasTag(Tag);
}

bool ParseComponentTAG(const UActorComponent* Component, const FName Tag)
{
	return Component->ComponentHasTag(Tag);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void USavior::SetProgress(float New)
{
	SlotMeta.Progress = New;
}

void USavior::SetPlayTime(int32 New)
{
	SlotMeta.PlayTime = New;
}

void USavior::SetChapter(FString New)
{
	SlotMeta.Chapter = New;
}

void USavior::SetPlayerLevel(int32 New)
{
	SlotMeta.PlayerLevel = New;
}

void USavior::SetSaveDate(FDateTime New)
{
	SlotMeta.SaveDate = New;
}

void USavior::SetPlayerName(FString New)
{
	SlotMeta.PlayerName = New;
}

void USavior::SetSaveLocation(FString New)
{
	SlotMeta.SaveLocation = New;
}

float USavior::GetProgress() const
{
	return SlotMeta.Progress;
}

int32 USavior::GetPlayTime() const
{
	return SlotMeta.PlayTime;
}

FString USavior::GetChapter() const
{
	return SlotMeta.Chapter;
}

int32 USavior::GetPlayerLevel() const
{
	return SlotMeta.PlayerLevel;
}

FDateTime USavior::GetSaveDate() const
{
	return SlotMeta.SaveDate;
}

FString USavior::GetPlayerName() const
{
	return SlotMeta.PlayerName;
}

FString USavior::GetSaveLocation() const
{
	return SlotMeta.SaveLocation;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Loading-Screen Interface:

void USavior::LaunchLoadScreen(const EThreadSafety Mode, const FText Info)
{
	if (GEngine == nullptr || World == nullptr)
	{
		return;
	}
	//
	const auto& PC = World->GetFirstPlayerController();
	//
	if (PC)
	{
		const auto& HUD = Cast<AHUD_SaviorUI>(PC->GetHUD());
		if (HUD)
		{
			switch (LoadScreenMode)
			{
				case ELoadScreenMode::BackgroundBlur:
				{
					HUD->DisplayBlurLoadScreenHUD(Mode, Info, FeedbackFont, ProgressBarTint, BackBlurPower);
				}
				break;
				//
				case ELoadScreenMode::SplashScreen:
				{
					HUD->DisplaySplashLoadScreenHUD(Mode, Info, FeedbackFont, ProgressBarTint, SplashImage, SplashStretch);
				}
				break;
				//
				case ELoadScreenMode::MoviePlayer:
				{
					HUD->DisplayMovieLoadScreenHUD(Mode, Info, FeedbackFont, ProgressBarTint, SplashMovie,
						ProgressBarOnMovie);
				}
				break;
				default:
					break;
			}
		}
	}
}

void USavior::RemoveLoadScreen()
{
	StaticRemoveLoadScreen();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void USaviorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	StaticSlot = NewObject<USavior>(this, TEXT("StaticSlot"));
	//
	StaticSlot->SetWorld(GetWorld());
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(StaticSlot, &USavior::StaticLoadGameWorld);
}

void USaviorSubsystem::Deinitialize()
{
	StaticSlot->ConditionalBeginDestroy();
}

void USaviorSubsystem::Reset()
{
	StaticSlot->Reset();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
