// Glow_Head: a soft halo on the shell itself, spawned again every frame and gone two frames later.

// @section Shape
// At least one and a half frames, or it would die on a slow frame before it is drawn.
LifeFull = max(0.035, Engine.DeltaTime * 1.5);

// @section Place
Particles.Position = OriginValue;
float HaloSize = GlowSize * S * lerp(0.9, 1.1, Rx);
Particles.SpriteSize = float2(HaloSize, HaloSize);
Particles.SpriteRotation = 0.0;
Particles.Color = float4(GlowColor * GlowBright, 1.0 - T);
