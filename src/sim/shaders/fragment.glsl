R""(
#version 330 core
smooth in vec4 color0;
smooth in vec3 texture_coordinate0;
out vec4 color;

uniform bool u_use_texture0;
uniform int u_tev_color_mode;
uniform vec4 u_tev_color0;
uniform sampler2D u_texture0;
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

bool alphaComparisonPasses(int comparison, int value, int reference)
{
    if (comparison == 0) {
        return false;
    } else if (comparison == 1) {
        return value < reference;
    } else if (comparison == 2) {
        return value == reference;
    } else if (comparison == 3) {
        return value <= reference;
    } else if (comparison == 4) {
        return value > reference;
    } else if (comparison == 5) {
        return value != reference;
    } else if (comparison == 6) {
        return value >= reference;
    }
    return true;
}

float fogRangeAdjustment()
{
    if (!u_fog_range_adjustment_enabled) {
        return 1.0;
    }

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
    if (u_fog_type == 0) {
        return 0.0;
    }

    // GX perspective matrices produce [-1, 0] NDC depth, and the GX
    // viewport transform rebiases that to [0, 1] EFB depth. OpenGL's
    // default depth mapping places it in [0, 0.5], so apply that rebase.
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

    if (u_fog_type == 4) {
        return 1.0 - exp2(-8.0 * linearFog);
    } else if (u_fog_type == 5) {
        return 1.0 - exp2(-8.0 * linearFog * linearFog);
    } else if (u_fog_type == 6) {
        return exp2(-8.0 * (1.0 - linearFog));
    } else if (u_fog_type == 7) {
        float reverseFog = 1.0 - linearFog;
        return exp2(-8.0 * reverseFog * reverseFog);
    }
    return linearFog;
}

void main()
{
    if (!u_use_texture0) {
        color = color0;
    } else {
        vec4 texture_color = textureProj(u_texture0, texture_coordinate0);
        if (u_tev_color_mode == 2) {
            color = texture_color * color0;
        } else if (u_tev_color_mode == 3) {
            ivec3 textureRgb = ivec3(
                floor(clamp(texture_color.rgb, 0.0, 1.0) * 255.0 + 0.5));
            bool textureIsZero = all(equal(textureRgb, ivec3(0)));
            color = vec4(
                textureIsZero
                    ? vec3(1.0)
                    : u_tev_color0.rgb,
                1.0);
        } else {
            color = texture_color;
        }
    }

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
    if (!passes) {
        discard;
    }
}
)""
