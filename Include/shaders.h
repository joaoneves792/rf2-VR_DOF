#ifndef __SEMAPHORE_SHADER__
#define __SEMAPHORE_SHADER__

const char * const fxaa_shader =
"Texture2D frame : register(t0);\n"
"sampler Sampler : register(s0);\n"
"\n"
"static float2 uv;\n"
"static float4 color;\n"
"\n"
"struct VOut{\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD0;\n"
"};\n"
"\n"
"uint2 SPIRV_Cross_textureSize(Texture2D<float4> Tex, uint Level, out uint Param)\n"
"{\n"
"    uint2 ret;\n"
"    Tex.GetDimensions(Level, ret.x, ret.y, Param);\n"
"    return ret;\n"
"}\n"
"\n"
"float rgb2luma(float3 rgb)\n"
"{\n"
"    return sqrt(dot(rgb, float3(0.2989999949932098388671875f, 0.58700001239776611328125f, 0.114000000059604644775390625f)));\n"
"}\n"
"\n"
"void frag_main()\n"
"{\n"
"    uint _34_dummy_parameter;\n"
"    float2 screenSize = float2(int2(SPIRV_Cross_textureSize(frame, uint(0), _34_dummy_parameter)));\n"
"    float2 texelSize = 1.0f.xx / screenSize;\n"
"    float3 colorCenter = frame.Sample( Sampler, uv ).xyz;\n"
"    float3 param = colorCenter;\n"
"    float lumaCenter = rgb2luma(param);\n"
"    float3 param_1 = frame.Sample( Sampler, uv + float2(0.0f, texelSize.y)).xyz;\n"
"    float lumaDown = rgb2luma(param_1);\n"
"    float3 param_2 = frame.Sample( Sampler, uv + float2(0.0f, -texelSize.y)).xyz;\n"
"    float lumaUp = rgb2luma(param_2);\n"
"    float3 param_3 = frame.Sample( Sampler, uv + float2(-texelSize.x, 0.0f)).xyz;\n"
"    float lumaLeft = rgb2luma(param_3);\n"
"    float3 param_4 = frame.Sample( Sampler, uv + float2(texelSize.x, 0.0f)).xyz;\n"
"    float lumaRight = rgb2luma(param_4);\n"
"    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));\n"
"    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));\n"
"    float lumaRange = lumaMax - lumaMin;\n"
"    if (lumaRange < max(0.031199999153614044189453125f, lumaMax * 0.125f))\n"
"    {\n"
"        color = float4(colorCenter, 1.0f);\n"
"        return;\n"
"    }\n"
"    float3 param_5 = frame.Sample( Sampler, uv + float2(-texelSize.x, texelSize.y)).xyz;\n"
"    float lumaDownLeft = rgb2luma(param_5);\n"
"    float3 param_6 = frame.Sample( Sampler, uv + float2(texelSize.x, -texelSize.y)).xyz;\n"
"    float lumaUpRight = rgb2luma(param_6);\n"
"    float3 param_7 = frame.Sample( Sampler, uv + float2(-texelSize.x, -texelSize.y)).xyz;\n"
"    float lumaUpLeft = rgb2luma(param_7);\n"
"    float3 param_8 = frame.Sample( Sampler, uv + float2(texelSize.x, texelSize.y)).xyz;\n"
"    float lumaDownRight = rgb2luma(param_8);\n"
"    float lumaDownUp = lumaDown + lumaUp;\n"
"    float lumaLeftRight = lumaLeft + lumaRight;\n"
"    float lumaLeftCorners = lumaDownLeft + lumaUpLeft;\n"
"    float lumaDownCorners = lumaDownLeft + lumaDownRight;\n"
"    float lumaRightCorners = lumaDownRight + lumaUpRight;\n"
"    float lumaUpCorners = lumaUpRight + lumaUpLeft;\n"
"    float edgeHorizontal = (abs(((-2.0f) * lumaLeft) + lumaLeftCorners) + (abs(((-2.0f) * lumaCenter) + lumaDownUp) * 2.0f)) + abs(((-2.0f) * lumaRight) + lumaRightCorners);\n"
"    float edgeVertical = (abs(((-2.0f) * lumaUp) + lumaUpCorners) + (abs(((-2.0f) * lumaCenter) + lumaLeftRight) * 2.0f)) + abs(((-2.0f) * lumaDown) + lumaDownCorners);\n"
"    bool isHorizontal = edgeHorizontal >= edgeVertical;\n"
"    float luma1 = isHorizontal ? lumaDown : lumaLeft;\n"
"    float luma2 = isHorizontal ? lumaUp : lumaRight;\n"
"    float gradient1 = luma1 - lumaCenter;\n"
"    float gradient2 = luma2 - lumaCenter;\n"
"    bool is1Steepest = abs(gradient1) >= abs(gradient2);\n"
"    float gradientScaled = 0.25f * max(abs(gradient1), abs(gradient2));\n"
"    float _305;\n"
"    if (isHorizontal)\n"
"    {\n"
"        _305 = texelSize.y;\n"
"    }\n"
"    else\n"
"    {\n"
"        _305 = texelSize.x;\n"
"    }\n"
"    float stepLength = _305;\n"
"    float lumaLocalAverage = 0.0f;\n"
"    if (is1Steepest)\n"
"    {\n"
"        stepLength = -stepLength;\n"
"        lumaLocalAverage = 0.5f * (luma1 + lumaCenter);\n"
"    }\n"
"    else\n"
"    {\n"
"        lumaLocalAverage = 0.5f * (luma2 + lumaCenter);\n"
"    }\n"
"    float2 currentUv = uv;\n"
"    if (isHorizontal)\n"
"    {\n"
"        currentUv.y += (stepLength * 0.5f);\n"
"    }\n"
"    else\n"
"    {\n"
"        currentUv.x += (stepLength * 0.5f);\n"
"    }\n"
"    float2 _350;\n"
"    if (isHorizontal)\n"
"    {\n"
"        _350 = float2(texelSize.x, 0.0f);\n"
"    }\n"
"    else\n"
"    {\n"
"        _350 = float2(0.0f, texelSize.y);\n"
"    }\n"
"    float2 offset = _350;\n"
"    float2 uv1 = currentUv - offset;\n"
"    float2 uv2 = currentUv + offset;\n"
"    float3 param_9 = frame.Sample( Sampler, uv1).xyz;\n"
"    float lumaEnd1 = rgb2luma(param_9);\n"
"    float3 param_10 = frame.Sample( Sampler, uv2).xyz;\n"
"    float lumaEnd2 = rgb2luma(param_10);\n"
"    lumaEnd1 -= lumaLocalAverage;\n"
"    lumaEnd2 -= lumaLocalAverage;\n"
"    bool reached1 = abs(lumaEnd1) >= gradientScaled;\n"
"    bool reached2 = abs(lumaEnd2) >= gradientScaled;\n"
"    bool reachedBoth = reached1 && reached2;\n"
"    if (!reached1)\n"
"    {\n"
"        uv1 -= offset;\n"
"    }\n"
"    if (!reached2)\n"
"    {\n"
"        uv2 += offset;\n"
"    }\n"
"    if (!reachedBoth)\n"
"    {\n"
"        [unroll(10) ]for (int i = 2; i < 12; i++)\n"
"        {\n"
"            if (!reached1)\n"
"            {\n"
"                float3 param_11 = frame.Sample( Sampler, uv1).xyz;\n"
"                lumaEnd1 = rgb2luma(param_11);\n"
"                lumaEnd1 -= lumaLocalAverage;\n"
"            }\n"
"            if (!reached2)\n"
"            {\n"
"                float3 param_12 = frame.Sample( Sampler, uv2).xyz;\n"
"                lumaEnd2 = rgb2luma(param_12);\n"
"                lumaEnd2 -= lumaLocalAverage;\n"
"            }\n"
"            reached1 = abs(lumaEnd1) >= gradientScaled;\n"
"            reached2 = abs(lumaEnd2) >= gradientScaled;\n"
"            reachedBoth = reached1 && reached2;\n"
"            if (!reached1)\n"
"            {\n"
"                uv1 -= (offset * 2.0f);\n"
"            }\n"
"            if (!reached2)\n"
"            {\n"
"                uv2 += (offset * 2.0f);\n"
"            }\n"
"            if (reachedBoth)\n"
"            {\n"
"                break;\n"
"            }\n"
"        }\n"
"    }\n"
"    float _494;\n"
"    if (isHorizontal)\n"
"    {\n"
"        _494 = uv.x - uv1.x;\n"
"    }\n"
"    else\n"
"    {\n"
"        _494 = uv.y - uv1.y;\n"
"    }\n"
"    float distance1 = _494;\n"
"    float _512;\n"
"    if (isHorizontal)\n"
"    {\n"
"        _512 = uv2.x - uv.x;\n"
"    }\n"
"    else\n"
"    {\n"
"        _512 = uv2.y - uv.y;\n"
"    }\n"
"    float distance2 = _512;\n"
"    bool isDirection1 = distance1 < distance2;\n"
"    float distanceFinal = min(distance1, distance2);\n"
"    float edgeThickness = distance1 + distance2;\n"
"    float pixelOffset = ((-distanceFinal) / edgeThickness) + 0.5f;\n"
"    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;\n"
"    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0f) != isLumaCenterSmaller;\n"
"    float finalOffset = correctVariation ? pixelOffset : 0.0f;\n"
"    float lumaAverage = 0.083333335816860198974609375f * (((2.0f * (lumaDownUp + lumaLeftRight)) + lumaLeftCorners) + lumaRightCorners);\n"
"    float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0f, 1.0f);\n"
"    float subPixelOffset2 = ((((-2.0f) * subPixelOffset1) + 3.0f) * subPixelOffset1) * subPixelOffset1;\n"
"    float subPixelOffsetFinal = (subPixelOffset2 * subPixelOffset2) * 0.75f;\n"
"    finalOffset = max(finalOffset, subPixelOffsetFinal);\n"
"    float2 finalUv = uv;\n"
"    if (isHorizontal)\n"
"    {\n"
"        finalUv.y += (finalOffset * stepLength);\n"
"    }\n"
"    else\n"
"    {\n"
"        finalUv.x += (finalOffset * stepLength);\n"
"    }\n"
"    color = float4(frame.Sample( Sampler, finalUv).xyz, 1.0f);\n"
"}\n"
"\n"
"float4 main(VOut input): SV_TARGET\n"
"{\n"
"    uv = input.uv;\n"
"    frag_main();\n"
"    return float4(color);\n"
"}\n";

const char * const quad_shader =
"struct VOut{\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD0;\n"
"};\n"
"VOut VShader(float4 position : POSITION){\n"
"    VOut output;\n"
"    output.pos = position;\n"
"    output.uv.x = (position.x*0.5)+0.5;\n"
"    output.uv.y = (position.y*-0.5)+0.5;\n"
"    return output;\n"
"}\n";


const char * const dof_shader =
"struct VOut{\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD0;\n"
"};\n"
"Texture2D Depth : register(t0);\n"
"Texture2D Color : register(t1);\n"
"sampler Sampler : register(s0);\n"
"static const float3x3 kernel =\n"
"{\n"
"    0.0625f,  0.125f, 0.0625f,\n"
"    0.125f,   0.25f,  0.125f,\n"
"    0.0625f,  0.125f, 0.0625f\n"
"};\n"
"float3 convolute(float2 pos, float2 radius){\n"
"    float3 t[9];\n"
"    t[0] = Color.Sample( Sampler, float2(pos.x-radius.x, pos.y-radius.y)).rgb;\n" 
"    t[1] = Color.Sample( Sampler, float2(pos.x, pos.y-radius.y)).rgb;\n" 
"    t[2] = Color.Sample( Sampler, float2(pos.x+radius.x, pos.y-radius.y)).rgb;\n"
"    t[3] = Color.Sample( Sampler, float2(pos.x-radius.x, pos.y)).rgb;\n"
"    t[4] = Color.Sample( Sampler, float2(pos.x, pos.y)).rgb;\n"
"    t[5] = Color.Sample( Sampler, float2(pos.x+radius.x, pos.y)).rgb;\n"
"    t[6] = Color.Sample( Sampler, float2(pos.x-radius.x, pos.y+radius.y)).rgb;\n" 
"    t[7] = Color.Sample( Sampler, float2(pos.x, pos.y+radius.y)).rgb;\n" 
"    t[8] = Color.Sample( Sampler, float2(pos.x+radius.x, pos.y+radius.y)).rgb;\n"
"    return kernel[0][0]*t[0] + kernel[0][1]*t[1] + kernel[0][2]*t[2] +\n"
"           kernel[1][0]*t[3] + kernel[1][1]*t[4] + kernel[1][2]*t[5] +\n"
"           kernel[2][0]*t[6] + kernel[2][1]*t[7] + kernel[2][2]*t[8];\n"
"}\n"
"#define FAR 4.0f\n"//Yes I am aware this are not the real values, but it doesnt matter
"#define NEAR 0.001f\n"
"float4 PShader(VOut input) : SV_TARGET{\n"
"	 float depth = Depth.Sample( Sampler, input.uv ).r;\n"
"	 //float P32 = -((2*FAR*NEAR)/(FAR-NEAR));\n"
"    //float P22 = -((FAR+NEAR)/(FAR-NEAR));\n"
"    //float z = -(P32/(depth+P22));\n" //Pseudo-Linearized Depth (not really all that great, far away is too blured, near is very pixelized)
"    depth = depth + 0.9f;\n"
"	 //float2 radius = float2(1.0/1920.0f, 1.0f/1080.0f);\n"
"	 float radius = depth*0.0005f;\n" // z = [0-4]
"    float3 color = convolute(input.uv, radius);\n"
"    return float4(color.r, color.g, color.b, 1.0);\n"
"}";

const char * const mfaa_shader = //This isnt really mfaa, but close enough (just testing an idea)
"struct VOut{\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD0;\n"
"};\n"
"Texture2D current : register(t0);\n"
"Texture2D last : register(t1);\n"
"sampler Sampler : register(s0);\n"
"float rgb2luma(float3 rgb)\n"
"{\n"
"    return sqrt(dot(rgb, float3(0.2989999949932098388671875f, 0.58700001239776611328125f, 0.114000000059604644775390625f)));\n"
"}\n"
"float4 main(VOut input) : SV_TARGET{\n"
"	 float3 currentColor = current.Sample( Sampler, input.uv ).rgb;\n"
"    float3 lastColor = last.Sample( Sampler, input.uv ).rgb;\n"
"    //Compute lumas\n"
"	 float lumaCurrent = rgb2luma(currentColor);\n"
"	 float lumaLast = rgb2luma(lastColor);\n"
"    float lumaMax = max(lumaLast, lumaCurrent);\n"
"    float lumaMin = min(lumaLast, lumaCurrent);\n"
"    float lumaRange = lumaMax - lumaMin;\n"
"    if (lumaRange < max(0.031199999153614044189453125f, lumaMax * 0.125f)){\n"
"		return float4((currentColor*0.4f)+(lastColor*0.6f), 1.0f);\n" //EWMA
"    }\n"
"    return float4(currentColor, 1.0);\n"
"}";

#endif //__SEMAPHORE_SHADER__