#version 450

layout (binding = 0) uniform ProjectionUniformBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} pub;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inTexCoord;
layout (location = 4) in mat4 inModel;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outTexCoord;

void main() {
    vec4 position = vec4(inPosition, 1.0);

    gl_Position = pub.proj * pub.view * pub.model * position;
    outPosition = (pub.model * position).xyz;
    outTexCoord = inTexCoord;

    mat3 mNormal = transpose(inverse(mat3(pub.model)));
    outNormal = mNormal * normalize(inNormal);
}