// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

// Job.CommanderShowcase: a commander's kit as the commander screen shows it. The director walks the faction menu to
// the commander screen on RTS_FactionSelection, selects the commander and opens every kit ability's preview in turn
// (the same AAbilityPreviewDirector the player's row click starts). A preview stays until its ability finished plus
// SecondsAfterFinished, then BACK; the next one opens after a short pause in the hangar. Every step is keyed on the
// rendered output frame and world time (never FPlatformTime: Movie Render Queue renders slower than real time).
// The edit cuts the hangar out, speeds up deliveries and builds the intro and the ability cards from a stills run.

#include "RTSShortsDirector.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/AircraftStrafeShotRecorder.h"
#include "EngineUtils.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "Misc/Paths.h"
#include "MoviePipeline.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/FactionSystem/FactionSelection/AbilityPreview/AbilityPreviewDirector.h"
#include "RTS_Survival/FactionSystem/FactionSelection/AbilityPreview/AbilityPreviewTargets.h"
#include "RTS_Survival/FactionSystem/FactionSelection/CommanderSelection/CommanderSelectionData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/CommanderSelection/W_CommanderAbilityRow.h"
#include "RTS_Survival/FactionSystem/FactionSelection/CommanderSelection/W_CommanderSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/W_UnitVariantSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsShowcase, Log, All);

namespace RTSShortsShowcasePrivate
{
	// Menu steps settle for this long (world seconds) before the next one, as the ability preview test does.
	constexpr double SettleSeconds = 1.0;

	template <typename WidgetType>
	WidgetType* FindLiveWidget(const UWorld* World)
	{
		for (TObjectIterator<WidgetType> WidgetIt; WidgetIt; ++WidgetIt)
		{
			if (WidgetIt->GetWorld() == World && WidgetIt->IsInViewport())
			{
				return *WidgetIt;
			}
		}
		return nullptr;
	}

	FString EnumValueName(const UEnum* Enum, const int64 Value)
	{
		return IsValid(Enum) ? Enum->GetNameStringByValue(Value) : FString::FromInt(Value);
	}
}

void ARTSShortsDirector::TickCommanderShowcase()
{
	using namespace RTSShortsShowcasePrivate;
	UWorld* const World = GetWorld();
	const double Now = World->GetTimeSeconds();
	if (not bM_HasSuppressedDialogs)
	{
		RTSFunctionLibrary::PushModalDialogSuppression();
		bM_HasSuppressedDialogs = true;
		M_RunStartWorldSeconds = Now;
	}
	if (M_Showcase.Step == ERTSShortsShowcaseStep::Done)
	{
		return;
	}
	const int32 Frame = GetShowcaseFrame(Now);
	if (M_Job.bRender && Frame >= 1)
	{
		UpdateSubmixCaptureWindow(Frame);
	}
	if (UW_FactionPopup* const Popup = FindLiveWidget<UW_FactionPopup>(World))
	{
		Popup->OnPopupAccepted.Broadcast();
		return;
	}
	if (Now < M_Showcase.NextActionWorldSeconds)
	{
		return;
	}
	const FRTSShortsCommanderShowcase& Settings = M_Job.CommanderShowcase;
	switch (M_Showcase.Step)
	{
	case ERTSShortsShowcaseStep::Boot:
		if (EnsureShowcaseCommanderScreen(Now))
		{
			if (not M_Showcase.bSoundMixPushed)
			{
				// Music, UI clicks and voice lines muted as in every short: the edit lays its own music under it.
				PushShortsSoundMix();
				M_Showcase.bSoundMixPushed = true;
			}
			M_Showcase.Menu->SelectCommander(Settings.Commander);
			FillShowcaseManifest();
			M_Showcase.Step = ERTSShortsShowcaseStep::WaitForStart;
			M_Showcase.NextActionWorldSeconds = Now + (Settings.bStills ? Settings.StillsSettleSeconds : SettleSeconds);
			UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_COMMANDER_SCREEN commander=%s slots=%d frame=%d"),
			       *EnumValueName(StaticEnum<ERTSCommander>(), static_cast<int64>(Settings.Commander)),
			       M_Showcase.Slots.Num(), Frame);
		}
		return;
	case ERTSShortsShowcaseStep::WaitForStart:
		if (Settings.bStills)
		{
			TakeShowcaseStill(TEXT("commander_kit_top"));
			M_Showcase.Step = ERTSShortsShowcaseStep::StillsScrollKit;
			M_Showcase.NextActionWorldSeconds = Now + 0.5;
			return;
		}
		// The rendered part begins after the warm-up; the post step drops the warm-up frames.
		if (Frame >= M_Manifest.WarmupFrames)
		{
			OpenShowcasePreview(Frame, Now);
		}
		return;
	case ERTSShortsShowcaseStep::StillsScrollKit:
		ScrollShowcaseKitToEnd();
		M_Showcase.Step = ERTSShortsShowcaseStep::StillsKitEnd;
		M_Showcase.NextActionWorldSeconds = Now + 0.8;
		return;
	case ERTSShortsShowcaseStep::StillsKitEnd:
		TakeShowcaseStill(TEXT("commander_kit_end"));
		M_Showcase.Step = ERTSShortsShowcaseStep::OpenPreview;
		M_Showcase.NextActionWorldSeconds = Now + 0.5;
		return;
	case ERTSShortsShowcaseStep::OpenPreview:
		OpenShowcasePreview(Frame, Now);
		return;
	case ERTSShortsShowcaseStep::Running:
		TickShowcasePreview(Frame, Now);
		return;
	case ERTSShortsShowcaseStep::BetweenPreviews:
		++M_Showcase.SlotListIndex;
		if (M_Showcase.SlotListIndex >= M_Showcase.Slots.Num())
		{
			FinishCommanderShowcase(Frame);
			return;
		}
		OpenShowcasePreview(Frame, Now);
		return;
	default:
		return;
	}
}

bool ARTSShortsDirector::EnsureShowcaseCommanderScreen(const double Now)
{
	using namespace RTSShortsShowcasePrivate;
	UWorld* const World = GetWorld();
	if (not M_Showcase.Controller.IsValid())
	{
		M_Showcase.Controller = Cast<AFactionPlayerController>(World->GetFirstPlayerController());
		if (not M_Showcase.Controller.IsValid())
		{
			return false;
		}
	}
	M_Showcase.Menu = FindLiveWidget<UW_CommanderSelectionMenu>(World);
	if (M_Showcase.Menu.IsValid())
	{
		return true;
	}
	if (FindLiveWidget<UW_UnitVariantSelectionMenu>(World) != nullptr)
	{
		if (not M_Showcase.bVariantsConfirmed)
		{
			M_Showcase.bVariantsConfirmed = true;
			M_Showcase.Controller->HandleUnitVariantSelectionConfirmed({});
			M_Showcase.NextActionWorldSeconds = Now + SettleSeconds;
		}
		return false;
	}
	if (not M_Showcase.bLaunchRequested)
	{
		M_Showcase.bLaunchRequested = true;
		M_Showcase.Controller->HandleLaunchCampaignRequested(ERTSFaction::GerBreakthroughDoctrine);
		M_Showcase.NextActionWorldSeconds = Now + 2.0 * SettleSeconds;
	}
	return false;
}

void ARTSShortsDirector::FillShowcaseManifest()
{
	using namespace RTSShortsShowcasePrivate;
	const FRTSShortsCommanderShowcase& Settings = M_Job.CommanderShowcase;
	const URTSGlobalAbilitySettings* const AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* const DataAsset = IsValid(AbilitySettings)
		                                                    ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                                    : nullptr;
	TArray<FCommanderSelectionAbilitySlot> Kit;
	for (const FCommanderSelectionEntryData& Entry : CommanderSelectionData::BuildCommanderEntries(DataAsset))
	{
		if (Entry.Commander == Settings.Commander)
		{
			Kit = Entry.AbilitySlots;
		}
	}
	M_Showcase.Slots = Settings.Slots;
	if (M_Showcase.Slots.IsEmpty())
	{
		for (int32 SlotIndex = 0; SlotIndex < Kit.Num(); ++SlotIndex)
		{
			M_Showcase.Slots.Add(SlotIndex);
		}
	}
	// One manifest segment per previewed slot; the job's own segments only budgeted the render length.
	M_Manifest.Segments.SetNum(M_Showcase.Slots.Num());
	for (int32 Index = 0; Index < M_Showcase.Slots.Num(); ++Index)
	{
		FRTSShortsManifestSegment& Entry = M_Manifest.Segments[Index];
		Entry = FRTSShortsManifestSegment();
		Entry.Index = Index;
		Entry.ShowcaseSlot = M_Showcase.Slots[Index];
		if (not Kit.IsValidIndex(Entry.ShowcaseSlot))
		{
			Entry.bSkipped = true;
			Entry.SkipReason = TEXT("the commander has no such kit slot");
			continue;
		}
		const FCommanderSelectionAbilitySlot& KitSlot = Kit[Entry.ShowcaseSlot];
		Entry.ShowcaseAbility = EnumValueName(StaticEnum<EGlobalAbility>(), static_cast<int64>(KitSlot.Ability));
		Entry.ShowcaseTitle = KitSlot.Title.ToString();
		Entry.ShowcaseTier = EnumValueName(StaticEnum<EGlobalAbilityTier>(), static_cast<int64>(KitSlot.Tier));
		Entry.ShowcaseDescription = KitSlot.Description.ToString();
		const UGlobalAbility* const Template = IsValid(DataAsset) ? DataAsset->FindAbilityTemplate(KitSlot.Ability) : nullptr;
		Entry.ShowcaseAbilityClass = IsValid(Template) ? Template->GetClass()->GetName() : TEXT("missing");
		Entry.Caption1 = Entry.ShowcaseTitle;
		Entry.Caption2 = Entry.ShowcaseTier;
	}
	WriteManifest();
}

void ARTSShortsDirector::OpenShowcasePreview(const int32 Frame, const double Now)
{
	using namespace RTSShortsShowcasePrivate;
	while (M_Showcase.Slots.IsValidIndex(M_Showcase.SlotListIndex)
		&& M_Manifest.Segments[M_Showcase.SlotListIndex].bSkipped)
	{
		++M_Showcase.SlotListIndex;
	}
	if (not M_Showcase.Slots.IsValidIndex(M_Showcase.SlotListIndex))
	{
		FinishCommanderShowcase(Frame);
		return;
	}
	FRTSShortsManifestSegment& Entry = M_Manifest.Segments[M_Showcase.SlotListIndex];
	const bool bOpened = M_Showcase.Menu.IsValid() && M_Showcase.Menu->PreviewAbilitySlot(Entry.ShowcaseSlot)
		&& M_Showcase.Controller.IsValid() && M_Showcase.Controller->GetIsAbilityPreviewOpen();
	if (not bOpened)
	{
		Entry.bSkipped = true;
		Entry.SkipReason = TEXT("the preview did not open");
		UE_LOG(LogRTSShortsShowcase, Warning, TEXT("RTS_SHORTS_SHOWCASE_PREVIEW_FAILED slot=%d ability=%s"),
		       Entry.ShowcaseSlot, *Entry.ShowcaseAbility);
		M_Showcase.Step = ERTSShortsShowcaseStep::BetweenPreviews;
		M_Showcase.NextActionWorldSeconds = Now + SettleSeconds;
		WriteManifest();
		return;
	}
#if RTS_WITH_MAP_TESTS
	if (M_Job.CommanderShowcase.bRecordStrafeShots)
	{
		FAircraftStrafeShotRecorder::Enable(FString::Printf(TEXT("%s_slot%d"), *M_Job.JobName, Entry.ShowcaseSlot));
	}
#endif
	Entry.ShowcaseOpenFrame = Frame;
	Entry.StartFrame = Frame;
	M_Showcase.Step = ERTSShortsShowcaseStep::Running;
	M_Showcase.StepStartWorldSeconds = Now;
	M_Showcase.FinishedWorldSeconds = -1.0;
	M_Showcase.bStillCardTaken = false;
	M_Showcase.bCameraFocusDone = false;
	UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_OPEN slot=%d ability=%s class=%s frame=%d"),
	       Entry.ShowcaseSlot, *Entry.ShowcaseAbility, *Entry.ShowcaseAbilityClass, Frame);
	WriteManifest();
}

void ARTSShortsDirector::TickShowcasePreview(const int32 Frame, const double Now)
{
	FRTSShortsManifestSegment& Entry = M_Manifest.Segments[M_Showcase.SlotListIndex];
	AFactionPlayerController* const Controller = M_Showcase.Controller.Get();
	if (not IsValid(Controller) || not Controller->GetIsAbilityPreviewOpen())
	{
		// The preview closed by itself (its 90 s safety net, or the 6 s linger after a very short ability).
		const AAbilityPreviewDirector* const PreviewDirector = IsValid(Controller) ? Controller->GetAbilityPreviewDirector() : nullptr;
		CloseShowcasePreview(Frame, Now, IsValid(PreviewDirector) ? PreviewDirector->GetCloseReason() : TEXT("closed"));
		return;
	}
	if (M_Job.CommanderShowcase.bStills)
	{
		// Stills: the card is all that is needed; screenshot once the cut has landed, then BACK.
		if (not M_Showcase.bStillCardTaken && Now - M_Showcase.StepStartWorldSeconds >= M_Job.CommanderShowcase.StillCardDelaySeconds)
		{
			TakeShowcaseStill(FString::Printf(TEXT("card_%d_%s"), Entry.ShowcaseSlot, *Entry.ShowcaseAbility));
			M_Showcase.bStillCardTaken = true;
			M_Showcase.NextActionWorldSeconds = Now + 0.5;
			return;
		}
		if (M_Showcase.bStillCardTaken)
		{
			Controller->HandleAbilityPreviewBack();
			CloseShowcasePreview(Frame, Now, TEXT("stills"));
		}
		return;
	}
	const AAbilityPreviewDirector* const PreviewDirector = Controller->GetAbilityPreviewDirector();
	if (not IsValid(PreviewDirector))
	{
		return;
	}
	ApplyShowcaseCameraFieldOfView();
	FocusShowcaseCameraOnTargets(Now);
	TrackShowcaseAttackRuns(Frame);
	if (Entry.ShowcaseExecuteFrame < 0 && PreviewDirector->GetHasAbilityExecuted())
	{
		Entry.ShowcaseExecuteFrame = Frame;
		Entry.FireOrderFrame = Frame;
		UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_EXECUTED slot=%d frame=%d"), Entry.ShowcaseSlot, Frame);
	}
	if (Entry.ShowcaseFinishedFrame < 0 && PreviewDirector->GetHasAbilityFinished())
	{
		Entry.ShowcaseFinishedFrame = Frame;
		Entry.StopOrderFrame = Frame;
		M_Showcase.FinishedWorldSeconds = Now;
		UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_FINISHED slot=%d frame=%d"), Entry.ShowcaseSlot, Frame);
		WriteManifest();
	}
	if (M_Showcase.FinishedWorldSeconds >= 0.0
		&& Now - M_Showcase.FinishedWorldSeconds >= M_Job.CommanderShowcase.SecondsAfterFinished)
	{
		Controller->HandleAbilityPreviewBack();
		CloseShowcasePreview(Frame, Now, TEXT("after_finished"));
	}
}

void ARTSShortsDirector::CloseShowcasePreview(const int32 Frame, const double Now, const FString& Reason)
{
	RestoreShowcaseCamera();
	if (M_Showcase.bWasAttacking)
	{
		M_Manifest.Segments[M_Showcase.SlotListIndex].ShowcaseAttackEndFrames.Add(Frame);
		M_Showcase.bWasAttacking = false;
	}
	FRTSShortsManifestSegment& Entry = M_Manifest.Segments[M_Showcase.SlotListIndex];
#if RTS_WITH_MAP_TESTS
	if (FAircraftStrafeShotRecorder::GetIsEnabled())
	{
		UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_STRAFE_SHOTS slot=%d %s"), Entry.ShowcaseSlot,
		       *FAircraftStrafeShotRecorder::DescribeMetrics());
		FAircraftStrafeShotRecorder::WriteCsv();
		FAircraftStrafeShotRecorder::Disable();
	}
#endif
	Entry.ShowcaseCloseFrame = Frame;
	Entry.EndFrame = Frame;
	Entry.ShowcaseCloseReason = Reason;
	UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_CLOSE slot=%d frame=%d reason=%s open=%d executed=%d finished=%d"),
	       Entry.ShowcaseSlot, Frame, *Reason, Entry.ShowcaseOpenFrame, Entry.ShowcaseExecuteFrame, Entry.ShowcaseFinishedFrame);
	M_Showcase.Step = ERTSShortsShowcaseStep::BetweenPreviews;
	M_Showcase.NextActionWorldSeconds = Now + M_Job.CommanderShowcase.SecondsBetweenPreviews;
	WriteManifest();
}

void ARTSShortsDirector::FinishCommanderShowcase(const int32 Frame)
{
	M_Showcase.Step = ERTSShortsShowcaseStep::Done;
	M_Phase = ERTSShortsDirectorPhase::Finished;
	// Past the last segment: NotifyRenderFinished then ends no segment and seals the manifest as complete.
	M_CurrentSegment = M_Manifest.Segments.Num();
	M_Manifest.bComplete = true;
	UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_DONE frame=%d stills=%d"), Frame,
	       M_Job.CommanderShowcase.bStills ? 1 : 0);
	if (not M_Job.bRender)
	{
		WriteManifest();
		// Stills: give the last screenshot a moment to be written, then end the process as a finished job.
		FTimerHandle ExitHandle;
		const FString JobName = M_Job.JobName;
		GetWorld()->GetTimerManager().SetTimer(ExitHandle, FTimerDelegate::CreateWeakLambda(this, [JobName]()
		{
			UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_JOB_DONE job=%s success=true"), *JobName);
			GLog->Flush();
			FPlatformMisc::RequestExitWithStatus(false, 0);
		}), 2.f, false);
		return;
	}
	// Nothing left to film: the render stops here instead of at the budgeted end, and the audio window ends with it.
	M_Manifest.TotalFrames = FMath::Max(Frame, 1);
	UpdateSubmixCaptureWindow(M_Manifest.TotalFrames);
	WriteManifest();
	if (UMoviePipeline* const Pipeline = GetActivePipeline())
	{
		Pipeline->RequestShutdown();
		UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_RENDER_STOPPED_EARLY frame=%d"), Frame);
	}
}

void ARTSShortsDirector::ApplyShowcaseCameraFieldOfView() const
{
	const float FieldOfView = M_Job.CommanderShowcase.CameraFieldOfView;
	const AFactionPlayerController* const Controller = M_Showcase.Controller.Get();
	if (FieldOfView <= 0.f || not M_Job.bRender || not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return;
	}
	// The preview's view target is its director actor; its camera component frames the view.
	const AActor* const ViewTarget = Controller->PlayerCameraManager->GetViewTarget();
	UCameraComponent* const Camera = IsValid(ViewTarget) ? ViewTarget->FindComponentByClass<UCameraComponent>() : nullptr;
	if (IsValid(Camera) && not FMath::IsNearlyEqual(Camera->FieldOfView, FieldOfView))
	{
		Camera->SetFieldOfView(FieldOfView);
		UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_CAMERA_FOV %.1f on %s"), FieldOfView, *ViewTarget->GetName());
	}
}

void ARTSShortsDirector::FocusShowcaseCameraOnTargets(const double Now)
{
	// The preview frames its targets for 16:9; at 9:16 a bombing strafe's tanks (placed where the bombs land, well
	// along the run) stood at the frame's edge. Slide the camera once so the targets' centre is mid-frame.
	if (M_Showcase.bCameraFocusDone || not M_Job.bRender)
	{
		return;
	}
	const AFactionPlayerController* const Controller = M_Showcase.Controller.Get();
	const AAbilityPreviewDirector* const PreviewDirector = IsValid(Controller) ? Controller->GetAbilityPreviewDirector() : nullptr;
	const UAbilityPreviewTargetsComponent* const Targets = IsValid(PreviewDirector) ? PreviewDirector->GetPreviewTargets() : nullptr;
	FVector Centroid = FVector::ZeroVector;
	int32 Count = 0;
	if (IsValid(Targets))
	{
		for (const TWeakObjectPtr<AActor>& Target : Targets->GetTargets())
		{
			if (Target.IsValid())
			{
				Centroid += Target->GetActorLocation();
				++Count;
			}
		}
	}
	if (Count == 0)
	{
		// Targets spawn when the preview starts; none after a second means this ability has none.
		M_Showcase.bCameraFocusDone = Now - M_Showcase.StepStartWorldSeconds > 1.0;
		return;
	}
	M_Showcase.bCameraFocusDone = true;
	for (const TWeakObjectPtr<AActor>& Target : Targets->GetTargets())
	{
		if (Target.IsValid())
		{
			UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_TARGET %s at=%s"), *Target->GetName(),
			       *Target->GetActorLocation().ToCompactString());
		}
	}
	Centroid /= Count;
	const AActor* const ViewTarget = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetViewTarget() : nullptr;
	UCameraComponent* const Camera = IsValid(ViewTarget) ? ViewTarget->FindComponentByClass<UCameraComponent>() : nullptr;
	if (not IsValid(Camera))
	{
		return;
	}
	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector Forward = Camera->GetForwardVector();
	if (Forward.Z > -0.05f)
	{
		return;
	}
	// Where the view's centre meets the targets' height, and the sideways slide that puts the centroid there.
	const FVector LookPoint = CameraLocation + Forward * ((Centroid.Z - CameraLocation.Z) / Forward.Z);
	FVector Shift = Centroid - LookPoint;
	Shift.Z = 0.f;
	M_Showcase.FocusedCamera = Camera;
	M_Showcase.CameraRelativeBeforeFocus = Camera->GetRelativeLocation();
	Camera->SetWorldLocation(CameraLocation + Shift);
	UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_CAMERA_FOCUS targets=%d shift=%s"), Count,
	       *Shift.ToCompactString());
}

void ARTSShortsDirector::TrackShowcaseAttackRuns(const int32 Frame)
{
	bool bAttacking = false;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		if (It->GetIsFlyingAttackDive())
		{
			bAttacking = true;
			break;
		}
	}
	if (bAttacking == M_Showcase.bWasAttacking)
	{
		return;
	}
	M_Showcase.bWasAttacking = bAttacking;
	FRTSShortsManifestSegment& Entry = M_Manifest.Segments[M_Showcase.SlotListIndex];
	(bAttacking ? Entry.ShowcaseAttackStartFrames : Entry.ShowcaseAttackEndFrames).Add(Frame);
	UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_ATTACK_%s slot=%d frame=%d"),
	       bAttacking ? TEXT("START") : TEXT("END"), Entry.ShowcaseSlot, Frame);
}

void ARTSShortsDirector::RestoreShowcaseCamera()
{
	if (UCameraComponent* const Camera = M_Showcase.FocusedCamera.Get())
	{
		Camera->SetRelativeLocation(M_Showcase.CameraRelativeBeforeFocus);
	}
	M_Showcase.FocusedCamera.Reset();
}

int32 ARTSShortsDirector::GetShowcaseFrame(const double Now) const
{
	if (M_Job.bRender)
	{
		return GetTimelineFrame();
	}
	return FMath::FloorToInt((Now - M_RunStartWorldSeconds) * M_Job.Fps);
}

void ARTSShortsDirector::TakeShowcaseStill(const FString& Name) const
{
	const FString Path = FPaths::Combine(M_Job.OutputDir, Name + TEXT(".png"));
	// With the UI: the commander screen and the ability cards are what the stills are for.
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogRTSShortsShowcase, Display, TEXT("RTS_SHORTS_SHOWCASE_STILL %s"), *Path);
}

void ARTSShortsDirector::ScrollShowcaseKitToEnd() const
{
	const UW_CommanderAbilityRow* const Row = M_Showcase.Menu.IsValid() ? M_Showcase.Menu->GetAbilityRow(0) : nullptr;
	for (UWidget* Parent = IsValid(Row) ? Row->GetParent() : nullptr; IsValid(Parent); Parent = Parent->GetParent())
	{
		if (UScrollBox* const ScrollBox = Cast<UScrollBox>(Parent))
		{
			ScrollBox->ScrollToEnd();
			return;
		}
	}
	UE_LOG(LogRTSShortsShowcase, Warning, TEXT("RTS_SHORTS_SHOWCASE_NO_KIT_SCROLLBOX"));
}
