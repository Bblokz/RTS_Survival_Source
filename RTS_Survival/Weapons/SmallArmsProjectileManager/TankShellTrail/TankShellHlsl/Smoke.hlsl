// Smoke_Trail: the smoke the shell leaves behind, lit puffs on the 2x2 puff atlas. A puff stays where the shell
// flew, appears a moment behind the tracer, then grows, rises, drifts with the wind and fades.

// @section Shape
LifeFull = SmokeLife * lerp(0.75, 1.25, R0) * ((SpawnKind > 0.5 && SpawnKind < 1.5) ? SmokeHeMlt : 1.0);
Particles.SubImageIndex = floor(R3 * 3.999);

// @section Place
float SmokeScale = S * lerp(1.0, SmokeHeMlt, IsHe);
float Grow = 1.0 - exp(-AgeSeconds * SmokeGrowRate);
float2 SmokeSide = (float2(Rx, Rz) - 0.5) * 2.0;
float3 SmokeSpreadOffset = (Right * SmokeSide.x + Up * SmokeSide.y) * (SmokeSpread * SmokeScale) * (0.25 + Grow);
// A slow corkscrew frozen in space: the phase comes from where on the path the puff was left.
float SmokePhase = dot(OriginValue, Fwd) * 0.017;
float3 SmokeWobbleOffset = (Right * sin(SmokePhase) + Up * cos(SmokePhase * 1.31)) * (SmokeWobble * SmokeScale * Grow);
float3 SmokeRiseOffset = float3(0.0, 0.0, SmokeRise * AgeSeconds);
float3 SmokeWindOffset = float3(10.0, 6.0, 0.0) * (AgeSeconds * SmokeWind);
Particles.Position = OriginValue + SmokeSpreadOffset + SmokeWobbleOffset + SmokeRiseOffset + SmokeWindOffset;
float PuffSize = SmokeScale * lerp(SmokeStartSize, SmokeEndSize, Grow) * lerp(0.8, 1.25, Ry);
Particles.SpriteSize = float2(PuffSize, PuffSize);
Particles.SpriteRotation = Rz * 360.0 + AgeSeconds * lerp(-25.0, 25.0, Rx);
// Smoothstep fade-in, so the smoke starts behind the hot streak instead of on the shell.
float SmokeFadeInT = saturate(AgeSeconds / SmokeFadeIn);
float SmokeAppear = SmokeFadeInT * SmokeFadeInT * (3.0 - 2.0 * SmokeFadeInT);
// Fresh smoke right behind the shell is darker and warmer, then it bleaches out.
float SmokeHeat = exp(-AgeSeconds * 7.0) * SmokeHotAmount;
float3 SmokeTint = lerp(SmokeColor, SmokeHotColor, SmokeHeat) * lerp(0.9, 1.08, Rx);
Particles.Color = float4(SmokeTint, SmokeAppear * pow(1.0 - T, SmokeFadePow) * SmokeAlpha);
