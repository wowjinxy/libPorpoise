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
uniform vec2 u_stage_texcoord_scale[MAX_TEV_STAGES];
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
    // A disabled TEV texture input is white.  If texturing is enabled while
    // no texture coordinates exist, Flipper produces black instead (the
    // renderer communicates that case with a negative value).
    if (u_use_texture[stage] == 0) return vec4(1.0);
    if (u_use_texture[stage] < 0) return vec4(0.0);
    vec2 projected = coordinate.z == 0.0
        ? coordinate.xy
        : coordinate.xy / coordinate.z;
    // The setup unit first converts generated coordinates to texel space
    // using SU_SSIZE/SU_TSIZE.  OpenGL samples normalized coordinates, so
    // each stage carries (SU size / bound texture size).
    projected *= u_stage_texcoord_scale[stage];
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
    if (channel == 0) return color0;
    if (channel == 1) return color1;
    // Channels 2-4 are invalid, 5-6 are indirect-texture alpha bump
    // channels, and 7 is GX_COLOR_ZERO/GX_COLOR_NULL.  Alpha bump requires
    // the indirect TEV path; never alias any of these channels to COLOR0.
    return vec4(0.0);
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
    if (input == 3) return vec3(reg0.a);
    if (input == 4) return reg1.rgb;
    if (input == 5) return vec3(reg1.a);
    if (input == 6) return reg2.rgb;
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

ivec3 tevByte(vec3 value)
{
    return ivec3(floor(value * 255.0 + vec3(0.5))) & ivec3(255);
}

int tevByte(float value)
{
    return int(floor(value * 255.0 + 0.5)) & 255;
}

vec4 tevOverflow(vec4 value)
{
    ivec4 bytes =
        ivec4(floor(value * 255.0 + vec4(0.5))) & ivec4(255);
    return vec4(bytes) / 255.0;
}

ivec3 tevSignedValue(vec3 value)
{
    return ivec3(floor(value * 255.0 + vec3(0.5)));
}

int tevSignedValue(float value)
{
    return int(floor(value * 255.0 + 0.5));
}

ivec3 tevLerp(
    ivec3 a,
    ivec3 b,
    ivec3 c,
    ivec3 d,
    int bias,
    int operation,
    int scale)
{
    // Flipper interpolates in byte space.  C is expanded from 0..255 to
    // 0..256 and the scale is moved inside the lerp before rounding.
    c += c >> 7;
    if (bias == 1) d += ivec3(128);
    if (bias == 2) d -= ivec3(128);

    ivec3 lerp = (a << 8) + (b - a) * c;
    if (scale != 3) {
        int multiplier = 1 << scale;
        lerp *= multiplier;
        d *= multiplier;
        lerp += operation == 1 ? ivec3(127) : ivec3(128);
    }

    ivec3 result = lerp >> 8;
    result = operation == 1 ? d - result : d + result;
    if (scale == 3) result >>= 1;
    return result;
}

int tevLerp(
    int a,
    int b,
    int c,
    int d,
    int bias,
    int operation,
    int scale)
{
    c += c >> 7;
    if (bias == 1) d += 128;
    if (bias == 2) d -= 128;

    int lerp = (a << 8) + (b - a) * c;
    if (scale != 3) {
        int multiplier = 1 << scale;
        lerp *= multiplier;
        d *= multiplier;
        lerp += operation == 1 ? 127 : 128;
    }

    int result = lerp >> 8;
    result = operation == 1 ? d - result : d + result;
    if (scale == 3) result >>= 1;
    return result;
}

bool tevColorCompare(int operation, ivec3 a, ivec3 b)
{
    if (operation == 8) return a.r > b.r;
    if (operation == 9) return a.r == b.r;

    int packedA;
    int packedB;
    if (operation == 10 || operation == 11) {
        // R is the low byte and G is the high byte.
        packedA = a.r | (a.g << 8);
        packedB = b.r | (b.g << 8);
    } else {
        // BGR24 likewise compares the packed little-endian RGB value.
        packedA = a.r | (a.g << 8) | (a.b << 16);
        packedB = b.r | (b.g << 8) | (b.b << 16);
    }
    return (operation & 1) == 0
        ? packedA > packedB
        : packedA == packedB;
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
    ivec3 inputA = tevByte(a);
    ivec3 inputB = tevByte(b);
    ivec3 inputC = tevByte(c);
    ivec3 inputD = tevSignedValue(d);
    ivec3 result;
    if (operation < 8) {
        result = tevLerp(
            inputA, inputB, inputC, inputD,
            bias, operation, scale);
    } else if (operation < 14) {
        result = inputD +
            (tevColorCompare(operation, inputA, inputB)
                ? inputC
                : ivec3(0));
    } else {
        bvec3 passes = operation == 14
            ? greaterThan(inputA, inputB)
            : equal(inputA, inputB);
        result = inputD + ivec3(
            passes.r ? inputC.r : 0,
            passes.g ? inputC.g : 0,
            passes.b ? inputC.b : 0);
    }
    result = clampEnabled != 0
        ? clamp(result, ivec3(0), ivec3(255))
        : clamp(result, ivec3(-1024), ivec3(1023));
    return vec3(result) / 255.0;
}

float applyAlphaOperation(
    int operation,
    int bias,
    int scale,
    int clampEnabled,
    float a,
    float b,
    float c,
    float d,
    vec3 colorA,
    vec3 colorB)
{
    int inputA = tevByte(a);
    int inputB = tevByte(b);
    int inputC = tevByte(c);
    int inputD = tevSignedValue(d);
    int result;
    if (operation < 8) {
        result = tevLerp(
            inputA, inputB, inputC, inputD,
            bias, operation, scale);
    } else {
        bool passes;
        if (operation < 14) {
            // Alpha compare modes R8/GR16/BGR24 compare the color combiner's
            // A and B inputs; only A8 (ops 14/15) uses alpha A and B.
            passes = tevColorCompare(
                operation,
                tevByte(colorA),
                tevByte(colorB));
        } else {
            passes = (operation & 1) == 0
                ? inputA > inputB
                : inputA == inputB;
        }
        result = inputD + (passes ? inputC : 0);
    }
    result = clampEnabled != 0
        ? clamp(result, 0, 255)
        : clamp(result, -1024, 1023);
    return float(result) / 255.0;
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
    vec3 finalStageColor = registers[0].rgb;
    float finalStageAlpha = registers[0].a;
    vec4 lastTextureColor = vec4(0.0);

    for (int stage = 0; stage < MAX_TEV_STAGES; ++stage) {
        if (stage >= u_num_tev_stages) break;

        ivec2 swapSelectors = u_tev_swap_selectors[stage];
        vec4 rawTextureColor = sampleStageTexture(
            stage,
            selectTextureCoordinate(u_stage_texcoord[stage]));
        if (u_use_texture[stage] > 0) {
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

        vec3 colorA = colorInput(
            colorInputs.x,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_color[stage].rgb);
        vec3 colorB = colorInput(
            colorInputs.y,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_color[stage].rgb);
        vec3 colorC = colorInput(
            colorInputs.z,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_color[stage].rgb);
        vec3 colorD = colorInput(
            colorInputs.w,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_color[stage].rgb);
        float alphaA = alphaInput(
            alphaInputs.x,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_alpha[stage]);
        float alphaB = alphaInput(
            alphaInputs.y,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_alpha[stage]);
        float alphaC = alphaInput(
            alphaInputs.z,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_alpha[stage]);
        float alphaD = alphaInput(
            alphaInputs.w,
            registers[0], registers[1], registers[2], registers[3],
            textureColor, raster, u_tev_konst_alpha[stage]);

        vec3 resultColor = applyColorOperation(
            colorOperation.x,
            colorOperation.y,
            colorOperation.z,
            colorOperation.w,
            colorA, colorB, colorC, colorD);
        float resultAlpha = applyAlphaOperation(
            alphaOperation.x,
            alphaOperation.y,
            alphaOperation.z,
            alphaOperation.w,
            alphaA, alphaB, alphaC, alphaD,
            colorA, colorB);

        ivec2 outputs = u_tev_output_registers[stage];
        registers[outputs.x].rgb = resultColor;
        registers[outputs.y].a = resultAlpha;
        finalStageColor = resultColor;
        finalStageAlpha = resultAlpha;
    }

    // The last TEV stage is displayed regardless of which register it wrote.
    color = tevOverflow(vec4(finalStageColor, finalStageAlpha));
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

    if (u_ztexture_operation != 0) {
        uint textureDepth;
        if (u_ztexture_format == 0) {
            textureDepth = uint(
                floor(lastTextureColor.a * 255.0 + 0.5));
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
            // GX projection occupies OpenGL window depth 0..0.5.  Recover
            // the 24-bit GX depth before applying GX_ZT_ADD.
            uint referenceDepth = min(
                uint(clamp(gl_FragCoord.z * 2.0, 0.0, 1.0) *
                    16777216.0),
                0x00ffffffu);
            depth += referenceDepth;
        }
        depth = (depth + u_ztexture_bias) & 0x00ffffffu;
        // Convert the resulting GX depth back to this renderer's OpenGL
        // window-depth range.
        gl_FragDepth = float(depth) / 33554432.0;
    } else {
        gl_FragDepth = gl_FragCoord.z;
    }
}
)""
