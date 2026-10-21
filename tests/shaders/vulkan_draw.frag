#version 450
layout(push_constant) uniform ClearColor { vec4 value; } color;
layout(location = 0) out vec4 outputColor;
void main() { outputColor = color.value; }
