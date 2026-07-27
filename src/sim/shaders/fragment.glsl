R""(
#version 330 core
smooth in vec4 color0;
smooth in vec3 texture_coordinate0;
out vec4 color;

uniform bool u_use_texture0;
uniform int u_tev_color_mode;
uniform sampler2D u_texture0;
uniform int u_alpha_comparison0;
uniform int u_alpha_reference0;
uniform int u_alpha_operation;
uniform int u_alpha_comparison1;
uniform int u_alpha_reference1;

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

void main()
{
    if (!u_use_texture0) {
        color = color0;
    } else {
        vec4 texture_color = textureProj(u_texture0, texture_coordinate0);
        if (u_tev_color_mode == 2) {
            color = texture_color * color0;
        } else {
            color = texture_color;
        }
    }

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
