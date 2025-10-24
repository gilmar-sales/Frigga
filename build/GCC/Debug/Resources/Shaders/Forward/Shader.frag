#version 450

layout (binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout (set = 1, binding = 0) uniform sampler2D albedoSampler;
layout (set = 1, binding = 1) uniform sampler2D normalSampler;
layout (set = 1, binding = 2) uniform sampler2D roughnessSampler;

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosition;
layout (location = 2) in vec2 fragTexCoord;
layout (location = 3) in mat3 TBN;

layout (location = 0) out vec4 outColor;

vec3 getNormalFromMap() {
    vec3 normal = texture(normalSampler, fragTexCoord).rgb * 2.0 - 1.0;
    return normalize(TBN * normal);
}

vec3 getCameraPosition(mat4 viewMatrix) {
    return -viewMatrix[3].xyz * mat3(viewMatrix);
}

void main()
{
    vec3 albedo = texture(albedoSampler, fragTexCoord).rgb;
    vec3 normal = getNormalFromMap();
    float roughness = texture(roughnessSampler, fragTexCoord).r;

    vec3 lightDirNorm = normalize(-pub.ambientLight.xyz);
    vec3 viewDir = normalize(getCameraPosition(pub.view) - fragPosition);

    float diff = max(dot(normal, lightDirNorm), pub.ambientLight.w);

    vec3 halfwayDir = normalize(lightDirNorm + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), mix(16.0, 2.0, roughness));
    
    vec3 lighting = (diff + spec) * vec3(1.0, 1.0, 1.0);
    outColor = vec4(albedo * lighting, 1.0);
}