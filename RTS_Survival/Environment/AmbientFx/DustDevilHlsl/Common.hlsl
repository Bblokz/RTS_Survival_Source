// Custom HLSL of the dust devil emitters (NS_AmbientDustDevils), split into sections that
// RTS.AmbientFx.PatchNiagaraSystem joins per emitter; the code is written into the emitters' HLSL nodes.
// Spawn node:  SpawnBegin + <Layer> Shape + SpawnEnd + ReadDevil + <Layer> Place
// Update node: UpdateBegin + ReadDevil + <Layer> Place
// Inputs: DevilPositionAndScale / DevilMotion (float4 arrays, one entry per devil slot), AliveSlots (int array of
// the slots that still grow), AliveCount, DustColor, and DevilSlotIn / DevilShapeIn (the particle's
// Particles.DevilSlot / DevilShape as read before the node; the update must read them this way, see the C++).

// @section SpawnBegin
// PCG hashes of the execution index and the time: random numbers R0..R4 (0 to 1) per particle.
uint Hash = uint(ExecIndex()) * 747796405u + asuint(Engine.Time) * 2891336453u + 2654435769u;
Hash = Hash * 747796405u + 2891336453u;
uint W0 = ((Hash >> ((Hash >> 28u) + 4u)) ^ Hash) * 277803737u;
float R0 = float((W0 >> 22u) ^ W0) / 4294967295.0;
Hash = Hash * 747796405u + 2891336453u;
uint W1 = ((Hash >> ((Hash >> 28u) + 4u)) ^ Hash) * 277803737u;
float R1 = float((W1 >> 22u) ^ W1) / 4294967295.0;
Hash = Hash * 747796405u + 2891336453u;
uint W2 = ((Hash >> ((Hash >> 28u) + 4u)) ^ Hash) * 277803737u;
float R2 = float((W2 >> 22u) ^ W2) / 4294967295.0;
Hash = Hash * 747796405u + 2891336453u;
uint W3 = ((Hash >> ((Hash >> 28u) + 4u)) ^ Hash) * 277803737u;
float R3 = float((W3 >> 22u) ^ W3) / 4294967295.0;
Hash = Hash * 747796405u + 2891336453u;
uint W4 = ((Hash >> ((Hash >> 28u) + 4u)) ^ Hash) * 277803737u;
float R4 = float((W4 >> 22u) ^ W4) / 4294967295.0;
// Spreads the particles spawned this frame over the devils that are still growing, at random.
int Spawning = max(AliveCount, 1);
int SlotIndex;
AliveSlots.Get(min(int(R4 * Spawning), Spawning - 1), SlotIndex);
Particles.DevilSlot = SlotIndex;

// @section SpawnEnd
// No devil is growing: the particle dies at once.
Particles.Lifetime = AliveCount > 0 ? Particles.Lifetime : 0.0;
// One of the 2x2 puffs of T_AmbientDust_Puffs.
Particles.SubImageIndex = floor(Particles.DevilShape.z * 3.999);
int DevilSlotValue = Particles.DevilSlot;
float4 DevilShapeValue = Particles.DevilShape;
float AgeSeconds = 0.0;
float AgeNormalized = 0.0;

// @section UpdateBegin
// Follows the devil every frame, so the particles move, lean and fade with it.
int DevilSlotValue = DevilSlotIn;
float4 DevilShapeValue = DevilShapeIn;
float AgeSeconds = Particles.Age;
float AgeNormalized = saturate(Particles.NormalizedAge);

// @section ReadDevil
// PS: xyz ground point under the devil, w scale. MO: x strength (fade), y seed, z spin (+1 / -1), w drift heading.
float4 PS;
float4 MO;
DevilPositionAndScale.Get(DevilSlotValue, PS);
DevilMotion.Get(DevilSlotValue, MO);
float4 Shape = DevilShapeValue;
float S = max(PS.w, 0.05);
float T = AgeNormalized;
float2 Heading = float2(cos(MO.w), sin(MO.w));
float2 Side = float2(-Heading.y, Heading.x);
