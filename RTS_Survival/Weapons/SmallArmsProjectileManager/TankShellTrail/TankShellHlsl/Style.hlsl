// The values of the tank shell look ("Frontlinie", picked 2026-11-25 out of five): white-hot core with an orange
// tail, the same for every shell type, and a smoke trail that grows, rises and drifts for almost two seconds.
// Sizes are larger than life on purpose: a true-to-scale 75 mm tracer is one pixel wide from the RTS camera.
// The looks that were not picked are kept in Docs/VFX/TankShellTrail.md.

// @section Style
float TracerLife = 0.07;
float TracerWidth = 24.0;
float TracerTailWidth = 0.3;
float TracerBright = 3.0;
float TracerFadePow = 1.6;
float3 TracerHot = float3(1.0, 0.96, 0.85);
float3 TracerCool = float3(1.0, 0.4, 0.08);
float GlowSize = 61.6;
float GlowBright = 1.2;
float3 GlowColor = float3(1.0, 0.72, 0.35);
float SmokeLife = 1.8;
float SmokeHeMlt = 1.35;
float SmokeGrowRate = 1.9;
float SmokeStartSize = 28.6;
float SmokeEndSize = 115.6;
float SmokeSpread = 8.0;
float SmokeWobble = 6.0;
float SmokeRise = 14.0;
float SmokeWind = 1.0;
float SmokeFadeIn = 0.15;
float SmokeFadePow = 1.3;
float SmokeAlpha = 0.5;
float SmokeHotAmount = 0.45;
float3 SmokeColor = float3(0.58, 0.57, 0.54);
float3 SmokeHotColor = float3(0.26, 0.24, 0.22);
