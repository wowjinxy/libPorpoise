R""(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec4 vertex_color;
layout (location = 2) in vec2 texCoords;

uniform mat4 u_projection;
uniform mat4 u_modelview;

smooth out vec4 color0;
smooth out vec2 fragTexCoords;

void main()
{
    gl_Position = u_projection * u_modelview * vec4(position, 1.0);
    color0 = vertex_color;
    fragTexCoords = texCoords;
}
)""
