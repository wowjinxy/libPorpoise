R""(
#version 330 core
#define MAX_TEV_STAGES 16

smooth in vec4 color0;
smooth in vec4 color1;
smooth in vec3 texture_coordinate0;
smooth in vec3 texture_coordinate1;
smooth in vec3 texture_coordinate2;
smooth in vec3 texture_coordinate3;
smooth in vec3 texture_coordinate4;
smooth in vec3 texture_coordinate5;
smooth in vec3 texture_coordinate6;
smooth in vec3 texture_coordinate7;
out vec4 color;

uniform int u_num_tev_stages;
uniform int u_use_texture[MAX_TEV_STAGES];
uniform sampler2D u_stage_texture[MAX_TEV_STAGES];
uniform int u_stage_texcoord[MAX_TEV_STAGES];
uniform int u_stage_raster_channel[MAX_TEV_STAGES];
uniform ivec4 u_tev_color_inputs[MAX_TEV_STAGES];
uniform ivec4 u_tev_alpha_inputs[MAX_TEV_STAGES];
uniform ivec4 u_tev_color_operation[MAX_TEV_STAGES];
uniform ivec4 u_tev_alpha_operation[MAX_TEV_STAGES];
uniform ivec2 u_tev_output_registers[MAX_TEV_STAGES];
uniform ivec2 u_tev_swap_selectors[MAX_TEV_STAGES];
uniform ivec4 u_tev_swap_tables[4];
uniform vec4 u_tev_registers[4];
uniform vec4 u_tev_konst_color[MAX_TEV_STAGES];
uniform float u_tev_konst_alpha[MAX_TEV_STAGES];

uniform int u_alpha_comparison0;
uniform int u_alpha_reference0;
uniform int u_alpha_operation;
uniform int u_alpha_comparison1;
uniform int u_alpha_reference1;
uniform int u_fog_type;
uniform bool u_fog_orthographic;
uniform float u_fog_a;
uniform float u_fog_b;
uniform float u_fog_c;
uniform vec3 u_fog_color;
uniform bool u_fog_range_adjustment_enabled;
uniform float u_fog_range_adjustment_center;
uniform float u_fog_range_adjustment[10];
uniform float u_fog_x_scale;
uniform int u_ztexture_operation;
uniform int u_ztexture_format;
uniform uint u_ztexture_bias;

vec3 selectTextureCoordinate(int index)
{
    if (index == 1) return texture_coordinate1;
    if (index == 2) return texture_coordinate2;
    if (index == 3) return texture_coordinate3;
    if (index == 4) return texture_coordinate4;
    if (index == 5) return texture_coordinate5;
    if (index == 6) return texture_coordinate6;
    if (index == 7) return texture_coordinate7;
    return texture_coordinate0;
}

vec4 sampleStageTexture(int stage, vec3 coordinate)
{
    if (u_use_texture[stage] == 0) return vec4(1.0);
    vec2 projected = coordinate.xy / coordinate.z;
    if (stage == 0) return texture(u_stage_texture[0], projected);
    if (stage == 1) return texture(u_stage_texture[1], projected);
    if (stage == 2) return texture(u_stage_texture[2], projected);
    if (stage == 3) return texture(u_stage_texture[3], projected);
    if (stage == 4) return texture(u_stage_texture[4], projected);
    if (stage == 5) return texture(u_stage_texture[5], projected);
    if (stage == 6) return texture(u_stage_texture[6], projected);
    if (stage == 7) return texture(u_stage_texture[7], projected);
    if (stage == 8) return texture(u_stage_texture[8], projected);
    if (stage == 9) return texture(u_stage_texture[9], projected);
    if (stage == 10) return texture(u_stage_texture[10], projected);
    if (stage == 11) return texture(u_stage_texture[11], projected);
    if (stage == 12) return texture(u_stage_texture[12], projected);
    if (stage == 13) return texture(u_stage_texture[13], projected);
    if (stage == 14) return texture(u_stage_texture[14], projected);
    return texture(u_stage_texture[15], projected);
}

vec4 rasterColor(int channel)
{
    if (channel == 1) return color1;
    if (channel == 7) return vec4(1.0);
    return color0;
}

vec4 swapColor(vec4 inputColor, int tableIndex)
{
    ivec4 selectors = u_tev_swap_tables[clamp(tableIndex, 0, 3)];
    return vec4(
        inputColor[selectors.x],
        inputColor[selectors.y],
        inputColor[selectors.z],
        inputColor[selectors.w]);
}

vec3 colorInput(
    int input,
    vec4 previous,
    vec4 reg0,
    vec4 reg1,
    vec4 reg2,
    vec4 textureColor,
    vec4 raster,
    vec3 konst)
{
    if (input == 0) return previous.rgb;
    if (input == 1) return vec3(previous.a);
    if (input == 2) return reg0.rgb;
    if (input == 3) return reg1.rgb;
    if (input == 4) return reg2.rgb;
    if (input == 5) return vec3(reg0.a);
    if (input == 6) return vec3(reg1.a);
    if (input == 7) return vec3(reg2.a);
    if (input == 8) return textureColor.rgb;
    if (input == 9) return vec3(textureColor.a);
    if (input == 10) return raster.rgb;
    if (input == 11) return vec3(raster.a);
    if (input == 12) return vec3(1.0);
    if (input == 13) return vec3(0.5);
    if (input == 14) return konst;
    return vec3(0.0);
}

float alphaInput(
    int input,
    vec4 previous,
    vec4 reg0,
    vec4 reg1,
    vec4 reg2,
    vec4 textureColor,
    vec4 raster,
    float konst)
{
    if (input == 0) return previous.a;
    if (input == 1) return reg0.a;
    if (input == 2) return reg1.a;
    if (input == 3) return reg2.a;
    if (input == 4) return textureColor.a;
    if (input == 5) return raster.a;
    if (input == 6) return konst;
    return 0.0;
}

float operationScale(int scale)
{
    if (scale == 1) return 2.0;
    if (scale == 2) return 4.0;
    if (scale == 3) return 0.5;
    return 1.0;
}

ivec3 quantizeTevColor(vec3 value)
{
    return ivec3(floor(clamp(value, 0.0, 1.0) * 255.0 + 0.5));
}

int quantizeTevAlpha(float value)
{
    return int(floor(clamp(value, 0.0, 1.0) * 255.0 + 0.5));
}

vec3 applyColorOperation(
    int operation,
    int bias,
    int scale,
    int clampEnabled,
    vec3 a,
    vec3 b,
    vec3 c,
    vec3 d)
{
    vec3 result;
    if (operation < 8) {
        vec3 blend = mix(a, b, c);
        result = operation == 1 ? d - blend : d + blend;
        if (bias == 1) result += 0.5;
        if (bias == 2) result -= 0.5;
        result *= operationScale(scale);
    } else if (operation == 8 || operation == 9) {
        int qa = quantizeTevColor(a).r;
        int qb = quantizeTevColor(b).r;
        bool passes = operation == 8 ? qa > qb : qa == qb;
        result = d + (passes ? c : vec3(0.0));
    } else if (operation == 10 || operation == 11) {
        ivec3 qa = quantizeTevColor(a);
        ivec3 qb = quantizeTevColor(b);
        int packedA = qa.r * 256 + qa.g;
        int packedB = qb.r * 256 + qb.g;
        bool passes = operation == 10
            ? packedA > packedB
            : packedA == packedB;
        result = d + (passes ? c : vec3(0.0));
    } else if (operation == 12 || operation == 13) {
        ivec3 qa = quantizeTevColor(a);
        ivec3 qb = quantizeTevColor(b);
        int packedA = qa.b * 65536 + qa.g * 256 + qa.r;
        int packedB = qb.b * 65536 + qb.g * 256 + qb.r;
        bool passes = operation == 12
            ? packedA > packedB
            : packedA == packedB;
        result = d + (passes ? c : vec3(0.0));
    } else {
        ivec3 qa = quantizeTevColor(a);
        ivec3 qb = quantizeTevColor(b);
        bvec3 passes = operation == 14
            ? greaterThan(qa, qb)
            : equal(qa, qb);
        result = d + mix(vec3(0.0), c, passes);
    }
    return clampEnabled != 0
        ? clamp(result, 0.0, 1.0)
        : clamp(result, -1024.0 / 255.0, 1023.0 / 255.0);
}

float applyAlphaOperation(
    int operation,
    int bias,
    int scale,
    int clampEnabled,
    float a,
    float b,
    float c,
    float d)
{
    float result;
    if (operation < 8) {
        float blend = mix(a, b, c);
        result = operation == 1 ? d - blend : d + blend;
        if (bias == 1) result += 0.5;
        if (bias == 2) result -= 0.5;
        result *= operationScale(scale);
    } else {
        int qa = quantizeTevAlpha(a);
        int qb = quantizeTevAlpha(b);
        bool passes = (operation & 1) == 0 ? qa > qb : qa == qb;
        result = d + (passes ? c : 0.0);
    }
    return clampEnabled != 0
        ? clamp(result, 0.0, 1.0)
        : clamp(result, -1024.0 / 255.0, 1023.0 / 255.0);
}

bool alphaComparisonPasses(int comparison, int value, int reference)
{
    if (comparison == 0) return false;
    if (comparison == 1) return value < reference;
    if (comparison == 2) return value == reference;
    if (comparison == 3) return value <= reference;
    if (comparison == 4) return value > reference;
    if (comparison == 5) return value != reference;
    if (comparison == 6) return value >= reference;
    return true;
}

float fogRangeAdjustment()
{
    if (!u_fog_range_adjustment_enabled) return 1.0;

    float distanceFromCenter = abs(
        gl_FragCoord.x * u_fog_x_scale -
        u_fog_range_adjustment_center);
    float tablePosition = distanceFromCenter / 32.0;
    if (tablePosition <= 1.0) {
        return mix(
            1.0,
            u_fog_range_adjustment[0],
            tablePosition);
    }

    int lowerIndex = min(int(floor(tablePosition)) - 1, 9);
    int upperIndex = min(lowerIndex + 1, 9);
    return mix(
        u_fog_range_adjustment[lowerIndex],
        u_fog_range_adjustment[upperIndex],
        fract(tablePosition));
}

float fogDensity()
{
    if (u_fog_type == 0) return 0.0;

    float gxDepth = gl_FragCoord.z * 2.0;
    float eyeDistanceOverRange;
    if (u_fog_orthographic) {
        eyeDistanceOverRange = u_fog_a * gxDepth;
    } else {
        eyeDistanceOverRange =
            u_fog_a / max(u_fog_b - gxDepth, 0.0000001);
    }
    float linearFog = clamp(
        eyeDistanceOverRange * fogRangeAdjustment() - u_fog_c,
        0.0,
        1.0);

    if (u_fog_type == 4) return 1.0 - exp2(-8.0 * linearFog);
    if (u_fog_type == 5) {
        return 1.0 - exp2(-8.0 * linearFog * linearFog);
    }
    if (u_fog_type == 6) {
        return exp2(-8.0 * (1.0 - linearFog));
    }
    if (u_fog_type == 7) {
        float reverseFog = 1.0 - linearFog;
        return exp2(-8.0 * reverseFog * reverseFog);
    }
    return linearFog;
}

void main()
{
    vec4 registers[4];
    registers[0] = u_tev_registers[0];
    registers[1] = u_tev_registers[1];
    registers[2] = u_tev_registers[2];
    registers[3] = u_tev_registers[3];
    vec4 lastTextureColor = vec4(0.0);
    int lastStage = clamp(
        u_num_tev_stages - 1,
        0,
        MAX_TEV_STAGES - 1);

    for (int stage = 0; stage < MAX_TEV_STAGES; ++stage) {
        if (stage >= u_num_tev_stages) break;

        ivec2 swapSelectors = u_tev_swap_selectors[stage];
        vec4 rawTextureColor = sampleStageTexture(
            stage,
            selectTextureCoordinate(u_stage_texcoord[stage]));
        if (stage == lastStage) {
            lastTextureColor = rawTextureColor;
        }
        vec4 textureColor = swapColor(
            rawTextureColor,
            swapSelectors.y);
        vec4 raster = swapColor(
            rasterColor(u_stage_raster_channel[stage]),
            swapSelectors.x);
        ivec4 colorInputs = u_tev_color_inputs[stage];
        ivec4 alphaInputs = u_tev_alpha_inputs[stage];
        ivec4 colorOperation = u_tev_color_operation[stage];
        ivec4 alphaOperation = u_tev_alpha_operation[stage];

        vec3 resultColor = applyColorOperation(
            colorOperation.x,
            colorOperation.y,
            colorOperation.z,
            colorOperation.w,
            colorInput(
                colorInputs.x,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_color[stage].rgb),
            colorInput(
                colorInputs.y,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_color[stage].rgb),
            colorInput(
                colorInputs.z,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_color[stage].rgb),
            colorInput(
                colorInputs.w,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_color[stage].rgb));
        float resultAlpha = applyAlphaOperation(
            alphaOperation.x,
            alphaOperation.y,
            alphaOperation.z,
            alphaOperation.w,
            alphaInput(
                alphaInputs.x,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_alpha[stage]),
            alphaInput(
                alphaInputs.y,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_alpha[stage]),
            alphaInput(
                alphaInputs.z,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_alpha[stage]),
            alphaInput(
                alphaInputs.w,
                registers[0], registers[1], registers[2], registers[3],
                textureColor, raster, u_tev_konst_alpha[stage]));

        ivec2 outputs = u_tev_output_registers[stage];
        registers[outputs.x].rgb = resultColor;
        registers[outputs.y].a = resultAlpha;
    }

    color = registers[0];
    color.rgb = mix(color.rgb, u_fog_color, fogDensity());

    int alpha = int(floor(clamp(color.a, 0.0, 1.0) * 255.0 + 0.5));
    bool pass0 = alphaComparisonPasses(
        u_alpha_comparison0, alpha, u_alpha_reference0);
    bool pass1 = alphaComparisonPasses(
        u_alpha_comparison1, alpha, u_alpha_reference1);
    bool passes;
    if (u_alpha_operation == 0) {
        passes = pass0 && pass1;
    } else if (u_alpha_operation == 1) {
        passes = pass0 || pass1;
    } else if (u_alpha_operation == 2) {
        passes = pass0 != pass1;
    } else {
        passes = pass0 == pass1;
    }
    if (!passes) discard;

    if (u_ztexture_operation != 0 &&
        u_use_texture[lastStage] != 0) {
        uint textureDepth;
        if (u_ztexture_format == 0) {
            textureDepth = uint(
                floor(lastTextureColor.r * 255.0 + 0.5));
        } else if (u_ztexture_format == 1) {
            uint highByte = uint(
                floor(lastTextureColor.a * 255.0 + 0.5));
            uint lowByte = uint(
                floor(lastTextureColor.r * 255.0 + 0.5));
            textureDepth = (highByte << 8u) | lowByte;
        } else {
            uvec3 bytes = uvec3(
                floor(lastTextureColor.rgb * 255.0 + 0.5));
            textureDepth =
                (bytes.r << 16u) |
                (bytes.g << 8u) |
                bytes.b;
        }

        uint depth = textureDepth;
        if (u_ztexture_operation == 1) {
            uint referenceDepth = uint(
                floor(
                    clamp(gl_FragCoord.z, 0.0, 1.0) *
                        16777215.0 +
                    0.5));
            depth += referenceDepth;
        }
        depth = (depth + u_ztexture_bias) & 0x00ffffffu;
        gl_FragDepth = float(depth) / 16777215.0;
    } else {
        gl_FragDepth = gl_FragCoord.z;
    }
}
)""
