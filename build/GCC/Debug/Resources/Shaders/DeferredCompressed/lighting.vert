#version 450

layout (input_attachment_index = 1, binding = 1) uniform subpassInput inDepthBuffer;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inGBuffer;

layout (location = 0) out vec4 outColor;

struct Light {
    vec4 position;
    vec3 color;
    float radius;
};

layout (std140, binding = 3) buffer LightsBuffer
{
    Light lights[];
};

void main()
{
    vec4 gBuffer = subpassLoad(inGBuffer);

    vec4 albedo = uintBitsToFloat(gBuffer.r);

    vec4 positionSpecular = uintBitsToFloat(gBuffer.g);
    vec3 position = positionSpecular.rgb;
    float specular = positionSpecular.a;
    
    vec3 normal = uintBitsToFloat(gBuffer.b).rgb;

    #define ambient 0.05

    // Ambient part
    vec3 fragcolor = albedo.rgb * ambient;

    for (int i = 0; i < lights.length(); ++i)
    {
        vec3 L = lights[i].position.xyz - fragPos;
        float dist = length(L);

        L = normalize(L);
        float atten = lights[i].radius / (pow(dist, 3.0) + 1.0);

        vec3 N = normalize(normal);
        float NdotL = max(0.0, dot(N, L));
        vec3 diff = lights[i].color * albedo.rgb * NdotL * atten;

        fragcolor += diff;
    }

    outColor = vec4(fragcolor, 1.0);
}