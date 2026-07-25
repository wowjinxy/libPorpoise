R""(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec4 vertex_color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 binormal;
layout (location = 4) in vec3 tangent;
layout (location = 6) in vec2 texcoord0;

uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat4 u_texmatrix0;
uniform int u_texgen0_source;

smooth out vec4 color0;
smooth out vec3 texture_coordinate0;

void main()
{
    gl_Position = u_projection * u_modelview * vec4(position, 1.0);
    color0 = vertex_color;
    vec4 texture_source;
    if (u_texgen0_source == 0) {
        texture_source = vec4(position, 1.0);
    } else if (u_texgen0_source == 1) {
        texture_source = vec4(normal, 1.0);
    } else if (u_texgen0_source == 2) {
        texture_source = vec4(binormal, 1.0);
    } else if (u_texgen0_source == 3) {
        texture_source = vec4(tangent, 1.0);
    } else {
        texture_source = vec4(texcoord0, 1.0, 1.0);
    }
    texture_coordinate0 =
        (u_texmatrix0 * texture_source).xyz;
}
)""
