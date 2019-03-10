#ifndef __SEMAPHORE_SHADER__
#define __SEMAPHORE_SHADER__

const char * const semaphore_shader =
"struct VOut{\n"
"    float4 position : SV_POSITION;\n"
"    float2 NDCpos : POSITION;\n"
"};\n"
"cbuffer cbViewport : register(b0){\n"
"    float width;\n"
"    float height;\n"
"}\n"
"cbuffer cbLights : register(b1){\n"
"    float color;\n"
"    float count;\n"
"}\n"
"Texture2D Texture : register(t0);\n"
"sampler Sampler : register(s0);\n"
"VOut VShader(float4 position : POSITION){\n"
"    VOut output;\n"
"    output.position = position;\n"
"    output.NDCpos = position.xy;\n"
"    return output;\n"
"}\n"
"float4 PShader(float2 NDCpos : POSITION) : SV_TARGET{\n"
"	  float depth = Texture.Sample( Sampler, float2((NDCpos.x*0.5)+0.5, (NDCpos.y*-0.5)+0.5)).r;\n"
"     return float4(depth, depth, depth, 1.0);\n"
"}";
#endif //__SEMAPHORE_SHADER__