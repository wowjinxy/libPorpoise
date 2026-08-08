R""(
#version 330 core
uniform sampler2D myTexture;
smooth in vec4 color0;
smooth in vec2 fragTexCoords;
out vec4 color;

void main()
{
    //color = color0;
    color = texture(myTexture, fragTexCoords);
}
)""
