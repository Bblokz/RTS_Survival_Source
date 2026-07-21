#include "RTSSteamCaptureViewport.h"

#include "RenderingThread.h"
#include "SceneView.h"

FRTSSteamCaptureViewport::FRTSSteamCaptureViewport(FViewportClient* ViewportClient)
	: FDummyViewport(ViewportClient)
{
}

FRTSSteamCaptureViewport::~FRTSSteamCaptureViewport()
{
	Release();
}

bool FRTSSteamCaptureViewport::Initialize(
	const FIntPoint& Resolution,
	const FViewport& PlayerViewport)
{
	if (bM_IsInitialized || Resolution.X <= 0 || Resolution.Y <= 0)
	{
		return false;
	}

	SizeX = Resolution.X;
	SizeY = Resolution.Y;
	SetupHDR(
		PlayerViewport.GetDisplayColorGamut(),
		PlayerViewport.GetDisplayOutputFormat(),
		PlayerViewport.GetSceneHDREnabled());

	BeginInitResource(this);
	FlushRenderingCommands();
	bM_IsInitialized = GetRenderTargetTexture().IsValid();
	if (not bM_IsInitialized)
	{
		BeginReleaseResource(this);
		FlushRenderingCommands();
	}
	return bM_IsInitialized;
}

void FRTSSteamCaptureViewport::Release()
{
	if (not bM_IsInitialized)
	{
		return;
	}

	BeginReleaseResource(this);
	FlushRenderingCommands();
	bM_IsInitialized = false;
}

FRTSSteamCaptureViewExtension::FRTSSteamCaptureViewExtension(
	const FAutoRegister& AutoRegister,
	const FViewport* CaptureViewport,
	const ERHIFeatureLevel::Type FeatureLevel)
	: FSceneViewExtensionBase(AutoRegister)
	  , M_CaptureViewport(CaptureViewport)
{
	M_CaptureViewState.Allocate(FeatureLevel);
}

FRTSSteamCaptureViewExtension::~FRTSSteamCaptureViewExtension()
{
	M_CaptureViewState.Destroy();
}

void FRTSSteamCaptureViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.bIsMultipleViewFamily = true;
	InViewFamily.bIsFirstViewInMultipleViewFamily = false;
}

void FRTSSteamCaptureViewExtension::SetupView(
	FSceneViewFamily& InViewFamily,
	FSceneView& InView)
{
	FSceneViewStateInterface* CaptureViewState = M_CaptureViewState.GetReference();
	if (CaptureViewState == nullptr)
	{
		return;
	}

	// Construction linked eye adaptation to the player's state. Replacing only State keeps exposure shared and temporal history isolated.
	InView.State = CaptureViewState;
}

void FRTSSteamCaptureViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	// UGameViewportClient marks all of its families as main; this offscreen family must not drive main-view scene captures.
	InViewFamily.bIsMainViewFamily = false;
}

void FRTSSteamCaptureViewExtension::PreRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& InViewFamily)
{
}

void FRTSSteamCaptureViewExtension::PreRenderView_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneView& InView)
{
}

bool FRTSSteamCaptureViewExtension::IsActiveThisFrame_Internal(
	const FSceneViewExtensionContext& Context) const
{
	return M_CaptureViewport != nullptr && Context.Viewport == M_CaptureViewport;
}
