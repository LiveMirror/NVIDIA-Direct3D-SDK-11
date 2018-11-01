#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 9.29.952.3111
//
//
//   fxc /T cs_5_0 /E UpsampleAndBlurX_CS11_8 Source/BlurX_CS.hlsl /Fh
//    Bin/UpsampleAndBlurX_CS_8.h /DKERNEL_RADIUS=8 /DHALF_RES_AO=1
//
//
// Buffer Definitions: 
//
// cbuffer GlobalConstantBuffer
// {
//
//   float2 g_FullResolution;           // Offset:    0 Size:     8
//   float2 g_InvFullResolution;        // Offset:    8 Size:     8
//   float2 g_AOResolution;             // Offset:   16 Size:     8 [unused]
//   float2 g_InvAOResolution;          // Offset:   24 Size:     8 [unused]
//   float2 g_FocalLen;                 // Offset:   32 Size:     8 [unused]
//   float2 g_InvFocalLen;              // Offset:   40 Size:     8 [unused]
//   float2 g_UVToViewA;                // Offset:   48 Size:     8 [unused]
//   float2 g_UVToViewB;                // Offset:   56 Size:     8 [unused]
//   float g_R;                         // Offset:   64 Size:     4 [unused]
//   float g_R2;                        // Offset:   68 Size:     4 [unused]
//   float g_NegInvR2;                  // Offset:   72 Size:     4 [unused]
//   float g_MaxRadiusPixels;           // Offset:   76 Size:     4 [unused]
//   float g_AngleBias;                 // Offset:   80 Size:     4 [unused]
//   float g_TanAngleBias;              // Offset:   84 Size:     4 [unused]
//   float g_PowExponent;               // Offset:   88 Size:     4 [unused]
//   float g_Strength;                  // Offset:   92 Size:     4 [unused]
//   float g_BlurDepthThreshold;        // Offset:   96 Size:     4
//   float g_BlurFalloff;               // Offset:  100 Size:     4
//   float g_LinA;                      // Offset:  104 Size:     4 [unused]
//   float g_LinB;                      // Offset:  108 Size:     4 [unused]
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim Slot Elements
// ------------------------------ ---------- ------- ----------- ---- --------
// PointClampSampler                 sampler      NA          NA    0        1
// LinearSamplerClamp                sampler      NA          NA    1        1
// tAO                               texture   float          2d    0        1
// tLinDepth                         texture   float          2d    1        1
// uOutputBuffer                         UAV  float2          2d    0        1
// GlobalConstantBuffer              cbuffer      NA          NA    0        1
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue Format   Used
// -------------------- ----- ------ -------- -------- ------ ------
// no Input
//
// Output signature:
//
// Name                 Index   Mask Register SysValue Format   Used
// -------------------- ----- ------ -------- -------- ------ ------
// no Output
cs_5_0
dcl_globalFlags refactoringAllowed
dcl_constantbuffer cb0[7], immediateIndexed
dcl_sampler s0, mode_default
dcl_sampler s1, mode_default
dcl_resource_texture2d (float,float,float,float) t0
dcl_resource_texture2d (float,float,float,float) t1
dcl_uav_typed_texture2d (float,float,float,float) u0
dcl_input vThreadGroupID.xy
dcl_input vThreadIDInGroup.x
dcl_temps 5
dcl_tgsm_structured g0, 8, 336
dcl_thread_group 336, 1, 1
utof r0.zw, vThreadGroupID.yyyx
mad r1.xy, r0.wwww, l(320.000000, 320.000000, 0.000000, 0.000000), l(320.000000, -8.000000, 0.000000, 0.000000)
utof r1.z, vThreadIDInGroup.x
add r0.y, r1.z, r1.y
add r1.yw, r0.yyyz, l(0.000000, 1.000000, 0.000000, 0.500000)
mul r1.yw, r1.yyyw, cb0[0].zzzw
sample_l_indexable(texture2d)(float,float,float,float) r2.x, r1.ywyy, t0.xyzw, s1, l(0.000000)
sample_l_indexable(texture2d)(float,float,float,float) r0.y, r1.ywyy, t1.yxzw, s1, l(0.000000)
mov r2.y, r0.y
store_structured g0.xy, vThreadIDInGroup.x, l(0), r2.xyxx
sync_g_t
mad r0.x, r0.w, l(320.000000), r1.z
min r0.w, r1.x, cb0[0].x
lt r0.w, r0.x, r0.w
if_nz r0.w
  add r0.zw, r0.xxxz, l(0.000000, 0.000000, 0.500000, 0.500000)
  mul r0.zw, r0.zzzw, cb0[0].zzzw
  sample_l_indexable(texture2d)(float,float,float,float) r1.x, r0.zwzz, t0.xyzw, s0, l(0.000000)
  sample_l_indexable(texture2d)(float,float,float,float) r3.y, r0.zwzz, t1.yxzw, s0, l(0.000000)
  mul r4.xyzw, cb0[6].yyyy, l(-56.250000, -30.250000, -12.250000, -2.250000)
  exp r4.xyzw, r4.xyzw
  add r0.y, r0.y, -r3.y
  lt r0.y, |r0.y|, cb0[6].x
  and r0.y, r0.y, l(0x3f800000)
  mul r0.z, r0.y, r4.x
  mad r0.z, r0.z, r2.x, r1.x
  mad r0.y, r4.x, r0.y, l(1.000000)
  iadd r1.xyz, l(2, 4, 6, 0), vThreadIDInGroup.xxxx
  ld_structured r1.xw, r1.x, l(0), g0.xxxy
  add r0.w, -r3.y, r1.w
  lt r0.w, |r0.w|, cb0[6].x
  and r0.w, r0.w, l(0x3f800000)
  mul r1.w, r0.w, r4.y
  mad r0.z, r1.w, r1.x, r0.z
  mad r0.y, r4.y, r0.w, r0.y
  ld_structured r1.xy, r1.y, l(0), g0.xyxx
  add r0.w, -r3.y, r1.y
  lt r0.w, |r0.w|, cb0[6].x
  and r0.w, r0.w, l(0x3f800000)
  mul r1.y, r0.w, r4.z
  mad r0.z, r1.y, r1.x, r0.z
  mad r0.y, r4.z, r0.w, r0.y
  ld_structured r1.xy, r1.z, l(0), g0.xyxx
  add r0.w, -r3.y, r1.y
  lt r0.w, |r0.w|, cb0[6].x
  and r0.w, r0.w, l(0x3f800000)
  mul r1.y, r0.w, r4.w
  mad r0.z, r1.y, r1.x, r0.z
  mad r0.y, r4.w, r0.w, r0.y
  iadd r1.xyzw, vThreadIDInGroup.xxxx, l(9, 11, 13, 15)
  ld_structured r2.xy, r1.x, l(0), g0.xyxx
  add r0.w, -r3.y, r2.y
  lt r0.w, |r0.w|, cb0[6].x
  and r0.w, r0.w, l(0x3f800000)
  mul r1.x, r0.w, r4.w
  mad r0.z, r1.x, r2.x, r0.z
  mad r0.y, r4.w, r0.w, r0.y
  ld_structured r1.xy, r1.y, l(0), g0.xyxx
  add r0.w, -r3.y, r1.y
  lt r0.w, |r0.w|, cb0[6].x
  and r0.w, r0.w, l(0x3f800000)
  mul r1.y, r0.w, r4.z
  mad r0.z, r1.y, r1.x, r0.z
  mad r0.y, r4.z, r0.w, r0.y
  ld_structured r1.xy, r1.z, l(0), g0.xyxx
  add r0.w, -r3.y, r1.y
  lt r0.w, |r0.w|, cb0[6].x
  and r0.w, r0.w, l(0x3f800000)
  mul r1.y, r0.w, r4.y
  mad r0.z, r1.y, r1.x, r0.z
  mad r0.y, r4.y, r0.w, r0.y
  ld_structured r1.xy, r1.w, l(0), g0.xyxx
  add r0.w, -r3.y, r1.y
  lt r0.w, |r0.w|, cb0[6].x
  and r0.w, r0.w, l(0x3f800000)
  mul r1.y, r0.w, r4.x
  mad r0.z, r1.y, r1.x, r0.z
  mad r0.y, r4.x, r0.w, r0.y
  div r3.xzw, r0.zzzz, r0.yyyy
  ftou r0.x, r0.x
  mov r0.yzw, vThreadGroupID.yyyy
  store_uav_typed u0.xyzw, r0.xyzw, r3.xyzw
endif 
ret 
// Approximately 84 instruction slots used
#endif

const BYTE g_UpsampleAndBlurX_CS11_8[] =
{
     68,  88,  66,  67,  90, 233, 
     74, 139,  43,  67,  21, 113, 
    235, 112, 161,  36,  34, 162, 
     63, 242,   1,   0,   0,   0, 
     56,  18,   0,   0,   5,   0, 
      0,   0,  52,   0,   0,   0, 
     80,   6,   0,   0,  96,   6, 
      0,   0, 112,   6,   0,   0, 
    156,  17,   0,   0,  82,  68, 
     69,  70,  20,   6,   0,   0, 
      1,   0,   0,   0,  84,   1, 
      0,   0,   6,   0,   0,   0, 
     60,   0,   0,   0,   0,   5, 
     83,  67,   0,   1,   0,   0, 
    227,   5,   0,   0,  82,  68, 
     49,  49,  60,   0,   0,   0, 
     24,   0,   0,   0,  32,   0, 
      0,   0,  40,   0,   0,   0, 
     36,   0,   0,   0,  12,   0, 
      0,   0,   0,   0,   0,   0, 
    252,   0,   0,   0,   3,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      1,   0,   0,   0,   1,   0, 
      0,   0,  14,   1,   0,   0, 
      3,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   1,   0,   0,   0, 
      1,   0,   0,   0,  33,   1, 
      0,   0,   2,   0,   0,   0, 
      5,   0,   0,   0,   4,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   1,   0,   0,   0, 
     37,   1,   0,   0,   2,   0, 
      0,   0,   5,   0,   0,   0, 
      4,   0,   0,   0, 255, 255, 
    255, 255,   1,   0,   0,   0, 
      1,   0,   0,   0,   1,   0, 
      0,   0,  47,   1,   0,   0, 
      4,   0,   0,   0,   5,   0, 
      0,   0,   4,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0,   1,   0,   0,   0, 
      5,   0,   0,   0,  61,   1, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   1,   0,   0,   0, 
     80, 111, 105, 110, 116,  67, 
    108,  97, 109, 112,  83,  97, 
    109, 112, 108, 101, 114,   0, 
     76, 105, 110, 101,  97, 114, 
     83,  97, 109, 112, 108, 101, 
    114,  67, 108,  97, 109, 112, 
      0, 116,  65,  79,   0, 116, 
     76, 105, 110,  68, 101, 112, 
    116, 104,   0, 117,  79, 117, 
    116, 112, 117, 116,  66, 117, 
    102, 102, 101, 114,   0,  71, 
    108, 111,  98,  97, 108,  67, 
    111, 110, 115, 116,  97, 110, 
    116,  66, 117, 102, 102, 101, 
    114,   0, 171, 171,  61,   1, 
      0,   0,  20,   0,   0,   0, 
    108,   1,   0,   0, 112,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 140,   4, 
      0,   0,   0,   0,   0,   0, 
      8,   0,   0,   0,   2,   0, 
      0,   0, 164,   4,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 200,   4,   0,   0, 
      8,   0,   0,   0,   8,   0, 
      0,   0,   2,   0,   0,   0, 
    164,   4,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    220,   4,   0,   0,  16,   0, 
      0,   0,   8,   0,   0,   0, 
      0,   0,   0,   0, 164,   4, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 235,   4, 
      0,   0,  24,   0,   0,   0, 
      8,   0,   0,   0,   0,   0, 
      0,   0, 164,   4,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 253,   4,   0,   0, 
     32,   0,   0,   0,   8,   0, 
      0,   0,   0,   0,   0,   0, 
    164,   4,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
      8,   5,   0,   0,  40,   0, 
      0,   0,   8,   0,   0,   0, 
      0,   0,   0,   0, 164,   4, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0,  22,   5, 
      0,   0,  48,   0,   0,   0, 
      8,   0,   0,   0,   0,   0, 
      0,   0, 164,   4,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0,  34,   5,   0,   0, 
     56,   0,   0,   0,   8,   0, 
      0,   0,   0,   0,   0,   0, 
    164,   4,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
     46,   5,   0,   0,  64,   0, 
      0,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,  56,   5, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0,  92,   5, 
      0,   0,  68,   0,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,  56,   5,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0,  97,   5,   0,   0, 
     72,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
     56,   5,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    108,   5,   0,   0,  76,   0, 
      0,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,  56,   5, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 126,   5, 
      0,   0,  80,   0,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,  56,   5,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 138,   5,   0,   0, 
     84,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
     56,   5,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    153,   5,   0,   0,  88,   0, 
      0,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,  56,   5, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 167,   5, 
      0,   0,  92,   0,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,  56,   5,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 178,   5,   0,   0, 
     96,   0,   0,   0,   4,   0, 
      0,   0,   2,   0,   0,   0, 
     56,   5,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    199,   5,   0,   0, 100,   0, 
      0,   0,   4,   0,   0,   0, 
      2,   0,   0,   0,  56,   5, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 213,   5, 
      0,   0, 104,   0,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,  56,   5,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 220,   5,   0,   0, 
    108,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
     56,   5,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    103,  95,  70, 117, 108, 108, 
     82, 101, 115, 111, 108, 117, 
    116, 105, 111, 110,   0, 102, 
    108, 111,  97, 116,  50,   0, 
      1,   0,   3,   0,   1,   0, 
      2,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 157,   4,   0,   0, 
    103,  95,  73, 110, 118,  70, 
    117, 108, 108,  82, 101, 115, 
    111, 108, 117, 116, 105, 111, 
    110,   0, 103,  95,  65,  79, 
     82, 101, 115, 111, 108, 117, 
    116, 105, 111, 110,   0, 103, 
     95,  73, 110, 118,  65,  79, 
     82, 101, 115, 111, 108, 117, 
    116, 105, 111, 110,   0, 103, 
     95,  70, 111,  99,  97, 108, 
     76, 101, 110,   0, 103,  95, 
     73, 110, 118,  70, 111,  99, 
     97, 108,  76, 101, 110,   0, 
    103,  95,  85,  86,  84, 111, 
     86, 105, 101, 119,  65,   0, 
    103,  95,  85,  86,  84, 111, 
     86, 105, 101, 119,  66,   0, 
    103,  95,  82,   0, 102, 108, 
    111,  97, 116,   0,   0,   0, 
      3,   0,   1,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     50,   5,   0,   0, 103,  95, 
     82,  50,   0, 103,  95,  78, 
    101, 103,  73, 110, 118,  82, 
     50,   0, 103,  95,  77,  97, 
    120,  82,  97, 100, 105, 117, 
    115,  80, 105, 120, 101, 108, 
    115,   0, 103,  95,  65, 110, 
    103, 108, 101,  66, 105,  97, 
    115,   0, 103,  95,  84,  97, 
    110,  65, 110, 103, 108, 101, 
     66, 105,  97, 115,   0, 103, 
     95,  80, 111, 119,  69, 120, 
    112, 111, 110, 101, 110, 116, 
      0, 103,  95,  83, 116, 114, 
    101, 110, 103, 116, 104,   0, 
    103,  95,  66, 108, 117, 114, 
     68, 101, 112, 116, 104,  84, 
    104, 114, 101, 115, 104, 111, 
    108, 100,   0, 103,  95,  66, 
    108, 117, 114,  70,  97, 108, 
    108, 111, 102, 102,   0, 103, 
     95,  76, 105, 110,  65,   0, 
    103,  95,  76, 105, 110,  66, 
      0,  77, 105,  99, 114, 111, 
    115, 111, 102, 116,  32,  40, 
     82,  41,  32,  72,  76,  83, 
     76,  32,  83, 104,  97, 100, 
    101, 114,  32,  67, 111, 109, 
    112, 105, 108, 101, 114,  32, 
     57,  46,  50,  57,  46,  57, 
     53,  50,  46,  51,  49,  49, 
     49,   0,  73,  83,  71,  78, 
      8,   0,   0,   0,   0,   0, 
      0,   0,   8,   0,   0,   0, 
     79,  83,  71,  78,   8,   0, 
      0,   0,   0,   0,   0,   0, 
      8,   0,   0,   0,  83,  72, 
     69,  88,  36,  11,   0,   0, 
     80,   0,   5,   0, 201,   2, 
      0,   0, 106,   8,   0,   1, 
     89,   0,   0,   4,  70, 142, 
     32,   0,   0,   0,   0,   0, 
      7,   0,   0,   0,  90,   0, 
      0,   3,   0,  96,  16,   0, 
      0,   0,   0,   0,  90,   0, 
      0,   3,   0,  96,  16,   0, 
      1,   0,   0,   0,  88,  24, 
      0,   4,   0, 112,  16,   0, 
      0,   0,   0,   0,  85,  85, 
      0,   0,  88,  24,   0,   4, 
      0, 112,  16,   0,   1,   0, 
      0,   0,  85,  85,   0,   0, 
    156,  24,   0,   4,   0, 224, 
     17,   0,   0,   0,   0,   0, 
     85,  85,   0,   0,  95,   0, 
      0,   2,  50,  16,   2,   0, 
     95,   0,   0,   2,  18,  32, 
      2,   0, 104,   0,   0,   2, 
      5,   0,   0,   0, 160,   0, 
      0,   5,   0, 240,  17,   0, 
      0,   0,   0,   0,   8,   0, 
      0,   0,  80,   1,   0,   0, 
    155,   0,   0,   4,  80,   1, 
      0,   0,   1,   0,   0,   0, 
      1,   0,   0,   0,  86,   0, 
      0,   4, 194,   0,  16,   0, 
      0,   0,   0,   0,  86,  17, 
      2,   0,  50,   0,   0,  15, 
     50,   0,  16,   0,   1,   0, 
      0,   0, 246,  15,  16,   0, 
      0,   0,   0,   0,   2,  64, 
      0,   0,   0,   0, 160,  67, 
      0,   0, 160,  67,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      2,  64,   0,   0,   0,   0, 
    160,  67,   0,   0,   0, 193, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  86,   0,   0,   4, 
     66,   0,  16,   0,   1,   0, 
      0,   0,  10,  32,   2,   0, 
      0,   0,   0,   7,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     42,   0,  16,   0,   1,   0, 
      0,   0,  26,   0,  16,   0, 
      1,   0,   0,   0,   0,   0, 
      0,  10, 162,   0,  16,   0, 
      1,   0,   0,   0,  86,   9, 
     16,   0,   0,   0,   0,   0, 
      2,  64,   0,   0,   0,   0, 
      0,   0,   0,   0, 128,  63, 
      0,   0,   0,   0,   0,   0, 
      0,  63,  56,   0,   0,   8, 
    162,   0,  16,   0,   1,   0, 
      0,   0,  86,  13,  16,   0, 
      1,   0,   0,   0, 166, 142, 
     32,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  72,   0, 
      0, 141, 194,   0,   0, 128, 
     67,  85,  21,   0,  18,   0, 
     16,   0,   2,   0,   0,   0, 
    214,   5,  16,   0,   1,   0, 
      0,   0,  70, 126,  16,   0, 
      0,   0,   0,   0,   0,  96, 
     16,   0,   1,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  72,   0,   0, 141, 
    194,   0,   0, 128,  67,  85, 
     21,   0,  34,   0,  16,   0, 
      0,   0,   0,   0, 214,   5, 
     16,   0,   1,   0,   0,   0, 
     22, 126,  16,   0,   1,   0, 
      0,   0,   0,  96,  16,   0, 
      1,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
     54,   0,   0,   5,  34,   0, 
     16,   0,   2,   0,   0,   0, 
     26,   0,  16,   0,   0,   0, 
      0,   0, 168,   0,   0,   8, 
     50, 240,  17,   0,   0,   0, 
      0,   0,  10,  32,   2,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  70,   0,  16,   0, 
      2,   0,   0,   0, 190,  24, 
      0,   1,  50,   0,   0,   9, 
     18,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0, 160,  67, 
     42,   0,  16,   0,   1,   0, 
      0,   0,  51,   0,   0,   8, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      1,   0,   0,   0,  10, 128, 
     32,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  49,   0, 
      0,   7, 130,   0,  16,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  31,   0,   4,   3, 
     58,   0,  16,   0,   0,   0, 
      0,   0,   0,   0,   0,  10, 
    194,   0,  16,   0,   0,   0, 
      0,   0,   6,   8,  16,   0, 
      0,   0,   0,   0,   2,  64, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,  63,   0,   0,   0,  63, 
     56,   0,   0,   8, 194,   0, 
     16,   0,   0,   0,   0,   0, 
    166,  14,  16,   0,   0,   0, 
      0,   0, 166, 142,  32,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  72,   0,   0, 141, 
    194,   0,   0, 128,  67,  85, 
     21,   0,  18,   0,  16,   0, 
      1,   0,   0,   0, 230,  10, 
     16,   0,   0,   0,   0,   0, 
     70, 126,  16,   0,   0,   0, 
      0,   0,   0,  96,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
     72,   0,   0, 141, 194,   0, 
      0, 128,  67,  85,  21,   0, 
     34,   0,  16,   0,   3,   0, 
      0,   0, 230,  10,  16,   0, 
      0,   0,   0,   0,  22, 126, 
     16,   0,   1,   0,   0,   0, 
      0,  96,  16,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0,   0,   0,  56,   0, 
      0,  11, 242,   0,  16,   0, 
      4,   0,   0,   0,  86, 133, 
     32,   0,   0,   0,   0,   0, 
      6,   0,   0,   0,   2,  64, 
      0,   0,   0,   0,  97, 194, 
      0,   0, 242, 193,   0,   0, 
     68, 193,   0,   0,  16, 192, 
     25,   0,   0,   5, 242,   0, 
     16,   0,   4,   0,   0,   0, 
     70,  14,  16,   0,   4,   0, 
      0,   0,   0,   0,   0,   8, 
     34,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16, 128,  65,   0,   0,   0, 
      3,   0,   0,   0,  49,   0, 
      0,   9,  34,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16, 128, 129,   0,   0,   0, 
      0,   0,   0,   0,  10, 128, 
     32,   0,   0,   0,   0,   0, 
      6,   0,   0,   0,   1,   0, 
      0,   7,  34,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
    128,  63,  56,   0,   0,   7, 
     66,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   4,   0,   0,   0, 
     50,   0,   0,   9,  66,   0, 
     16,   0,   0,   0,   0,   0, 
     42,   0,  16,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      2,   0,   0,   0,  10,   0, 
     16,   0,   1,   0,   0,   0, 
     50,   0,   0,   9,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   4,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0, 128,  63, 
     30,   0,   0,   9, 114,   0, 
     16,   0,   1,   0,   0,   0, 
      2,  64,   0,   0,   2,   0, 
      0,   0,   4,   0,   0,   0, 
      6,   0,   0,   0,   0,   0, 
      0,   0,   6,  32,   2,   0, 
    167,   0,   0,   9, 146,   0, 
     16,   0,   1,   0,   0,   0, 
     10,   0,  16,   0,   1,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0,   0,   0,   6, 244, 
     17,   0,   0,   0,   0,   0, 
      0,   0,   0,   8, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16, 128,  65,   0, 
      0,   0,   3,   0,   0,   0, 
     58,   0,  16,   0,   1,   0, 
      0,   0,  49,   0,   0,   9, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16, 128, 
    129,   0,   0,   0,   0,   0, 
      0,   0,  10, 128,  32,   0, 
      0,   0,   0,   0,   6,   0, 
      0,   0,   1,   0,   0,   7, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0, 128,  63, 
     56,   0,   0,   7, 130,   0, 
     16,   0,   1,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      4,   0,   0,   0,  50,   0, 
      0,   9,  66,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   1,   0,   0,   0, 
     10,   0,  16,   0,   1,   0, 
      0,   0,  42,   0,  16,   0, 
      0,   0,   0,   0,  50,   0, 
      0,   9,  34,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   4,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0, 167,   0, 
      0,   9,  50,   0,  16,   0, 
      1,   0,   0,   0,  26,   0, 
     16,   0,   1,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  70, 240,  17,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   8, 130,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16, 128,  65,   0,   0,   0, 
      3,   0,   0,   0,  26,   0, 
     16,   0,   1,   0,   0,   0, 
     49,   0,   0,   9, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16, 128, 129,   0, 
      0,   0,   0,   0,   0,   0, 
     10, 128,  32,   0,   0,   0, 
      0,   0,   6,   0,   0,   0, 
      1,   0,   0,   7, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0, 128,  63,  56,   0, 
      0,   7,  34,   0,  16,   0, 
      1,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
     42,   0,  16,   0,   4,   0, 
      0,   0,  50,   0,   0,   9, 
     66,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      1,   0,   0,   0,  10,   0, 
     16,   0,   1,   0,   0,   0, 
     42,   0,  16,   0,   0,   0, 
      0,   0,  50,   0,   0,   9, 
     34,   0,  16,   0,   0,   0, 
      0,   0,  42,   0,  16,   0, 
      4,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   0,   0, 
      0,   0, 167,   0,   0,   9, 
     50,   0,  16,   0,   1,   0, 
      0,   0,  42,   0,  16,   0, 
      1,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
     70, 240,  17,   0,   0,   0, 
      0,   0,   0,   0,   0,   8, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16, 128, 
     65,   0,   0,   0,   3,   0, 
      0,   0,  26,   0,  16,   0, 
      1,   0,   0,   0,  49,   0, 
      0,   9, 130,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16, 128, 129,   0,   0,   0, 
      0,   0,   0,   0,  10, 128, 
     32,   0,   0,   0,   0,   0, 
      6,   0,   0,   0,   1,   0, 
      0,   7, 130,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
    128,  63,  56,   0,   0,   7, 
     34,   0,  16,   0,   1,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   4,   0,   0,   0, 
     50,   0,   0,   9,  66,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   1,   0, 
      0,   0,  10,   0,  16,   0, 
      1,   0,   0,   0,  42,   0, 
     16,   0,   0,   0,   0,   0, 
     50,   0,   0,   9,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16,   0,   4,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
     30,   0,   0,   9, 242,   0, 
     16,   0,   1,   0,   0,   0, 
      6,  32,   2,   0,   2,  64, 
      0,   0,   9,   0,   0,   0, 
     11,   0,   0,   0,  13,   0, 
      0,   0,  15,   0,   0,   0, 
    167,   0,   0,   9,  50,   0, 
     16,   0,   2,   0,   0,   0, 
     10,   0,  16,   0,   1,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0,   0,   0,  70, 240, 
     17,   0,   0,   0,   0,   0, 
      0,   0,   0,   8, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16, 128,  65,   0, 
      0,   0,   3,   0,   0,   0, 
     26,   0,  16,   0,   2,   0, 
      0,   0,  49,   0,   0,   9, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16, 128, 
    129,   0,   0,   0,   0,   0, 
      0,   0,  10, 128,  32,   0, 
      0,   0,   0,   0,   6,   0, 
      0,   0,   1,   0,   0,   7, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0, 128,  63, 
     56,   0,   0,   7,  18,   0, 
     16,   0,   1,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      4,   0,   0,   0,  50,   0, 
      0,   9,  66,   0,  16,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   1,   0,   0,   0, 
     10,   0,  16,   0,   2,   0, 
      0,   0,  42,   0,  16,   0, 
      0,   0,   0,   0,  50,   0, 
      0,   9,  34,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   4,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0, 167,   0, 
      0,   9,  50,   0,  16,   0, 
      1,   0,   0,   0,  26,   0, 
     16,   0,   1,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  70, 240,  17,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   8, 130,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16, 128,  65,   0,   0,   0, 
      3,   0,   0,   0,  26,   0, 
     16,   0,   1,   0,   0,   0, 
     49,   0,   0,   9, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16, 128, 129,   0, 
      0,   0,   0,   0,   0,   0, 
     10, 128,  32,   0,   0,   0, 
      0,   0,   6,   0,   0,   0, 
      1,   0,   0,   7, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0, 128,  63,  56,   0, 
      0,   7,  34,   0,  16,   0, 
      1,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
     42,   0,  16,   0,   4,   0, 
      0,   0,  50,   0,   0,   9, 
     66,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      1,   0,   0,   0,  10,   0, 
     16,   0,   1,   0,   0,   0, 
     42,   0,  16,   0,   0,   0, 
      0,   0,  50,   0,   0,   9, 
     34,   0,  16,   0,   0,   0, 
      0,   0,  42,   0,  16,   0, 
      4,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   0,   0, 
      0,   0, 167,   0,   0,   9, 
     50,   0,  16,   0,   1,   0, 
      0,   0,  42,   0,  16,   0, 
      1,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
     70, 240,  17,   0,   0,   0, 
      0,   0,   0,   0,   0,   8, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16, 128, 
     65,   0,   0,   0,   3,   0, 
      0,   0,  26,   0,  16,   0, 
      1,   0,   0,   0,  49,   0, 
      0,   9, 130,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16, 128, 129,   0,   0,   0, 
      0,   0,   0,   0,  10, 128, 
     32,   0,   0,   0,   0,   0, 
      6,   0,   0,   0,   1,   0, 
      0,   7, 130,   0,  16,   0, 
      0,   0,   0,   0,  58,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
    128,  63,  56,   0,   0,   7, 
     34,   0,  16,   0,   1,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   4,   0,   0,   0, 
     50,   0,   0,   9,  66,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   1,   0, 
      0,   0,  10,   0,  16,   0, 
      1,   0,   0,   0,  42,   0, 
     16,   0,   0,   0,   0,   0, 
     50,   0,   0,   9,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   4,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
    167,   0,   0,   9,  50,   0, 
     16,   0,   1,   0,   0,   0, 
     58,   0,  16,   0,   1,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0,   0,   0,  70, 240, 
     17,   0,   0,   0,   0,   0, 
      0,   0,   0,   8, 130,   0, 
     16,   0,   0,   0,   0,   0, 
     26,   0,  16, 128,  65,   0, 
      0,   0,   3,   0,   0,   0, 
     26,   0,  16,   0,   1,   0, 
      0,   0,  49,   0,   0,   9, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16, 128, 
    129,   0,   0,   0,   0,   0, 
      0,   0,  10, 128,  32,   0, 
      0,   0,   0,   0,   6,   0, 
      0,   0,   1,   0,   0,   7, 
    130,   0,  16,   0,   0,   0, 
      0,   0,  58,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0, 128,  63, 
     56,   0,   0,   7,  34,   0, 
     16,   0,   1,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      4,   0,   0,   0,  50,   0, 
      0,   9,  66,   0,  16,   0, 
      0,   0,   0,   0,  26,   0, 
     16,   0,   1,   0,   0,   0, 
     10,   0,  16,   0,   1,   0, 
      0,   0,  42,   0,  16,   0, 
      0,   0,   0,   0,  50,   0, 
      0,   9,  34,   0,  16,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   4,   0,   0,   0, 
     58,   0,  16,   0,   0,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0,  14,   0, 
      0,   7, 210,   0,  16,   0, 
      3,   0,   0,   0, 166,  10, 
     16,   0,   0,   0,   0,   0, 
     86,   5,  16,   0,   0,   0, 
      0,   0,  28,   0,   0,   5, 
     18,   0,  16,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      0,   0,   0,   0,  54,   0, 
      0,   4, 226,   0,  16,   0, 
      0,   0,   0,   0,  86,  21, 
      2,   0, 164,   0,   0,   7, 
    242, 224,  17,   0,   0,   0, 
      0,   0,  70,  14,  16,   0, 
      0,   0,   0,   0,  70,  14, 
     16,   0,   3,   0,   0,   0, 
     21,   0,   0,   1,  62,   0, 
      0,   1,  83,  84,  65,  84, 
    148,   0,   0,   0,  84,   0, 
      0,   0,   5,   0,   0,   0, 
      0,   0,   0,   0,   2,   0, 
      0,   0,  34,   0,   0,   0, 
      2,   0,   0,   0,   8,   0, 
      0,   0,   1,   0,   0,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   6,   0,   0,   0, 
      0,   0,   0,   0,   3,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0
};
