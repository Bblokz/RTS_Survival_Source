#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimatedIconLifetimeTest, "RTS.UI.AnimatedIcons.Lifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimatedIconLifetimeTest::RunTest(const FString& Parameters)
{
	FRTSVerticalAnimIconSettings Settings;
	Settings.VisibleDuration = 2.0f;
	Settings.FadeOutDuration = 2.0f;
	Settings.DeltaZ = 100.0f;
	float WorldOffsetZ = 0.0f;
	float Opacity = 0.0f;

	TestTrue(TEXT("Starts active"), Settings.Evaluate(0.0, WorldOffsetZ, Opacity));
	TestEqual(TEXT("Starts at anchor"), WorldOffsetZ, 0.0f);
	TestEqual(TEXT("Starts opaque"), Opacity, 1.0f);
	TestTrue(TEXT("Visible phase moves too"), Settings.Evaluate(1.0, WorldOffsetZ, Opacity));
	TestEqual(TEXT("Quarter lifetime has quarter travel"), WorldOffsetZ, 25.0f);
	TestEqual(TEXT("Visible phase stays opaque"), Opacity, 1.0f);
	TestTrue(TEXT("Fade boundary remains active"), Settings.Evaluate(2.0, WorldOffsetZ, Opacity));
	TestEqual(TEXT("Fade starts fully opaque"), Opacity, 1.0f);
	TestTrue(TEXT("Fade midpoint remains active"), Settings.Evaluate(3.0, WorldOffsetZ, Opacity));
	TestEqual(TEXT("Motion continues through fade"), WorldOffsetZ, 75.0f);
	TestEqual(TEXT("Halfway through fade"), Opacity, 0.5f);
	TestFalse(TEXT("Exact endpoint releases slot"), Settings.Evaluate(4.0, WorldOffsetZ, Opacity));
	TestEqual(TEXT("Endpoint is transparent"), Opacity, 0.0f);
	TestEqual(TEXT("Endpoint reaches full travel"), WorldOffsetZ, 100.0f);
	TestFalse(TEXT("Long frame past endpoint releases slot"), Settings.Evaluate(20.0, WorldOffsetZ, Opacity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimatedIconDurationEdgeCasesTest, "RTS.UI.AnimatedIcons.DurationEdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimatedIconDurationEdgeCasesTest::RunTest(const FString& Parameters)
{
	FRTSVerticalAnimIconSettings Settings;
	Settings.VisibleDuration = 1.0f;
	Settings.FadeOutDuration = 0.0f;
	Settings.DeltaZ = -20.0f;
	float WorldOffsetZ = 0.0f;
	float Opacity = 0.0f;
	TestTrue(TEXT("Zero fade permits visible period"), Settings.Evaluate(0.5, WorldOffsetZ, Opacity));
	TestEqual(TEXT("Negative travel goes downward"), WorldOffsetZ, -10.0f);
	TestEqual(TEXT("Zero fade remains opaque until expiry"), Opacity, 1.0f);
	TestFalse(TEXT("Zero fade expires without dividing by zero"), Settings.Evaluate(1.0, WorldOffsetZ, Opacity));
	Settings.VisibleDuration = 0.0f;
	Settings.FadeOutDuration = 1.0f;
	TestTrue(TEXT("Zero visible duration fades immediately"), Settings.Evaluate(0.5, WorldOffsetZ, Opacity));
	TestEqual(TEXT("Immediate fade reaches half opacity"), Opacity, 0.5f);
	Settings.FadeOutDuration = 0.0f;
	TestFalse(TEXT("Zero total duration cannot consume a slot"), Settings.IsUsable());
	TestFalse(TEXT("Zero total duration evaluates safely"), Settings.Evaluate(0.0, WorldOffsetZ, Opacity));
	Settings.VisibleDuration = -1.0f;
	TestFalse(TEXT("Negative duration is rejected"), Settings.IsUsable());
	return true;
}

#endif
