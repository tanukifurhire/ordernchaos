// vertex shader
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;  // <- now vec4

out vec4 ourColor;

uniform mat4 projection;
uniform mat4 camera;

void main()
{
    gl_Position = projection * (camera * vec4(aPos, 0.0, 1.0));
    ourColor = aColor;  // full RGBA
   
}
