#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive :enable

#include "constants.h"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    outColor = vec4(fragColor, 1);
    vec4 tex = texture(texSampler, vec2(1.0 - fragTexCoord.x, 1.0 - fragTexCoord.y));
    tex.w = 1.0;

    // For now we use default coloring w/o texture
    outColor = vec4(fragColor, 1.0);
}