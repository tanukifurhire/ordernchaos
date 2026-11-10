// fragment shader
#version 450 core
in vec4 ourColor;
out vec4 FragColor;

void main()
{
    FragColor = ourColor;  // keeps alpha
}
