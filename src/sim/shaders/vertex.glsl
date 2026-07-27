R""(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec4 vertex_color0;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 binormal;
layout (location = 4) in vec3 tangent;
layout (location = 5) in vec4 vertex_color1;
layout (location = 6) in vec3 texcoord0;
layout (location = 7) in vec3 texcoord1;
layout (location = 8) in vec3 texcoord2;
layout (location = 9) in vec3 texcoord3;
layout (location = 10) in vec3 texcoord4;
layout (location = 11) in vec3 texcoord5;
layout (location = 12) in vec3 texcoord6;
layout (location = 13) in vec3 texcoord7;

uniform mat4 u_projection;
uniform mat4 u_modelview;

smooth out vec4 color0;
smooth out vec4 color1;
smooth out vec3 texture_coordinate0;
smooth out vec3 texture_coordinate1;
smooth out vec3 texture_coordinate2;
smooth out vec3 texture_coordinate3;
smooth out vec3 texture_coordinate4;
smooth out vec3 texture_coordinate5;
smooth out vec3 texture_coordinate6;
smooth out vec3 texture_coordinate7;

void main()
{
    gl_Position = u_projection * u_modelview * vec4(position, 1.0);
    color0 = vertex_color0;
    color1 = vertex_color1;
    texture_coordinate0 = texcoord0;
    texture_coordinate1 = texcoord1;
    texture_coordinate2 = texcoord2;
    texture_coordinate3 = texcoord3;
    texture_coordinate4 = texcoord4;
    texture_coordinate5 = texcoord5;
    texture_coordinate6 = texcoord6;
    texture_coordinate7 = texcoord7;
}
)""
