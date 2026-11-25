// Tracer_Core: the hot streak. Every particle stays where the shell flew and fades within a few frames, so the
// line of them is a streak whose length is flight speed x TracerLife. Camera-facing sprite, additive glow.

// @section Shape
LifeFull = TracerLife * lerp(0.9, 1.1, R0);

// @section Place
Particles.Position = OriginValue;
float TracerSize = TracerWidth * S * lerp(1.0, TracerTailWidth, T);
Particles.SpriteSize = float2(TracerSize, TracerSize);
Particles.SpriteRotation = 0.0;
float3 TracerColor = lerp(TracerHot, TracerCool, saturate(T * 1.5));
Particles.Color = float4(TracerColor * TracerBright, pow(1.0 - T, TracerFadePow));
