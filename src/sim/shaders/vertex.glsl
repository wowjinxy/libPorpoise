R""(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec4 vertex_color;
layout (location = 6) in vec2 texcoord0;

uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat4 u_texmatrix0;

smooth out vec4 color0;
smooth out vec2 texture_coordinate0;

void main()
{
    gl_Position = u_projection * u_modelview * vec4(position, 1.0);
    color0 = vertex_color;
    texture_coordinate0 =
        (u_texmatrix0 * vec4(texcoord0, 1.0, 1.0)).xy;
}
)""
