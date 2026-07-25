R""(
#version 330 core
smooth in vec4 color0;
smooth in vec3 texture_coordinate0;
out vec4 color;

uniform bool u_use_texture0;
uniform int u_tev_color_mode;
uniform sampler2D u_texture0;

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
}
)""
