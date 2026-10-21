#version 450
layout(push_constant) uniform ClearColor { vec4 value; ivec4 rectangle; } color;
layout(location = 0) out vec4 outputColor;
void main() {
    // Enforce the clear's integer pixel domain as well as the Vulkan scissor.
    // This keeps all samples of an outside pixel unchanged.
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    if (any(lessThan(pixel, color.rectangle.xy)) ||
        any(greaterThanEqual(pixel, color.rectangle.zw))) discard;
    outputColor = color.value;
}
