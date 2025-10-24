#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inTexCoord;
layout (location = 5) in mat4 inModel;

layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;

void main() 
{
    vec4 worldPos = inModel * vec4(inPosition, 1.0);

    gl_Position = pub.proj * pub.view * worldPos;
	
	outTexCoord = inTexCoord;

	// Vertex position in world space
	outWorldPos = worldPos.xyz;
	
	// Normal in world space
	mat3 mNormal = transpose(inverse(mat3(inModel)));
	outNormal = mNormal * normalize(inNormal);	
	outTangent = mNormal * normalize(inTangent);
	
	// Currently just vertex color
	outColor = inColor;
}
