#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 2) out vec4 outGBuffer;

layout (set = 0, binding = 1) uniform sampler2D uAlbedoTexture;
layout (set = 0, binding = 2) uniform sampler2D uNormalTexture;
layout (set = 0, binding = 3) uniform sampler2D uSpecularTexture;

void main() {
    outGBuffer.r = texture(uAlbedoTexture, inTexCoord);

    outGBuffer.b = vec4(inPosition, texture(uSpecularTexture, inTexCoord));

    vec3 N = normalize(inNormal);
    N.y = -N.y;
    outGBuffer.b = vec4(N, 1.0);
}