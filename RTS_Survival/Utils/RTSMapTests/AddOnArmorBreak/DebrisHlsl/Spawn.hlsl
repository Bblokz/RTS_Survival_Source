// Spawn node of NS_TestSkirtDebris (written into the emitter by RTS.AddOnArmorTest.BuildNiagara).
// One particle per released skirt piece: it starts exactly where the piece was, with the piece's push and spin.
// Inputs: PieceStart (xyz position, w shard mesh index), PieceVelocity (xyz), PieceSpin (xyz axis, w rad/s),
// PieceOrientation (quaternion xyzw), PieceCount, DebrisLifetime.
// Branch free: the CPU Vector VM rejects variables that only some branches write.
int PieceIndex = ExecIndex();
float4 StartValue;
PieceStart.Get(PieceIndex, StartValue);
float4 VelocityValue;
PieceVelocity.Get(PieceIndex, VelocityValue);
float4 SpinValue;
PieceSpin.Get(PieceIndex, SpinValue);
float4 OrientationValue;
PieceOrientation.Get(PieceIndex, OrientationValue);
// 1 for a burst slot that carries a piece of this hit, 0 for a spare slot, which dies at once.
float CarriesPiece = step((float)PieceIndex + 0.5, (float)PieceCount);
Particles.Position = StartValue.xyz;
Particles.MeshIndex = (int)(StartValue.w + 0.5);
Particles.Velocity = VelocityValue.xyz;
Particles.SpinAxis = SpinValue.xyz;
Particles.SpinSpeed = SpinValue.w;
Particles.MeshOrientation = OrientationValue;
Particles.Scale = float3(1.0, 1.0, 1.0);
Particles.Resting = 0.0;
Particles.Lifetime = DebrisLifetime * CarriesPiece;
