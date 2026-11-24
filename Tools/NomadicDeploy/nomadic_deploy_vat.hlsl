// Nomadic building VAT deploy offset (Custom node body in MF_NomadicDeploy_VAT). Returns a LOCAL-space offset.
// LP: pre-skinned local position. U4 = (piece column u, pivot.x), U5 = (pivot.y, pivot.z) baked by vat_bake.py;
// u < 0 marks pieces that never move (the truck). P: deploy progress from custom primitive data 3
// (UNomadicDeployAnimationComponent). Frames: rows of the textures. OffsetTex: RGB pivot offset cm, A uniform scale.
// RotTex: rotation quaternion xyzw (Unreal space).
float u = U4.x;
if (u < 0.0 || P >= 0.9999)
{
    return float3(0, 0, 0);
}
float3 pivot = float3(U4.y, U5.x, U5.y);
float v = (saturate(P) * (Frames - 1.0) + 0.5) / Frames;
float4 A = OffsetTex.SampleLevel(OffsetTexSampler, float2(u, v), 0);
float4 Q = RotTex.SampleLevel(RotTexSampler, float2(u, v), 0);
float qLen = length(Q);
Q = qLen > 1e-4 ? Q / qLen : float4(0, 0, 0, 1);
float3 rel = (LP - pivot) * A.w;
float3 rotated = rel + 2.0 * cross(Q.xyz, cross(Q.xyz, rel) + Q.w * rel);
return pivot + A.xyz + rotated - LP;
