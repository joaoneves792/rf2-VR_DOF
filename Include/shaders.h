#ifndef __SEMAPHORE_SHADER__
#define __SEMAPHORE_SHADER__

const char * const semaphore_shader =
"struct VOut{\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD0;\n"
"};\n"
"Texture2D Depth : register(t0);\n"
"Texture2D Color : register(t1);\n"
"sampler Sampler : register(s0);\n"
"VOut VShader(float4 position : POSITION){\n"
"    VOut output;\n"
"    output.pos = position;\n"
"    output.uv = (position.xy*0.5)+0.5;\n"
"    return output;\n"
"}\n"
"float4 PShader(float2 uv : TEXCOORD0) : SV_TARGET{\n"
"	  float depth = Depth.Sample( Sampler, uv ).r;\n"
"     float3 color = Color.Sample(Sampler, uv).rgb;\n"
"     return float4(depth, color.g, color.b, 1.0);\n"
"}";
#endif //__SEMAPHORE_SHADER__