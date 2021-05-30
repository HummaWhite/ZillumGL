//$Vertex
#version 450 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;

uniform mat4 model;
uniform mat4 VPmatrix;
uniform mat3 modelInv;

out VSOut
{
    vec2 texCoord;
    vec3 fragPos;
    vec3 normal;
    mat3 TBN;
} vsOut;

void main()
{
    gl_Position = VPmatrix * model * vec4(pos, 1.0f);
    vsOut.fragPos = vec3(model * vec4(pos, 1.0f));
    vsOut.texCoord = texCoord;
    vsOut.normal = normalize(modelInv * normal);
    vec3 bitangent = normalize(cross(normal, tangent));
    vec3 T = normalize(vec3(model * vec4(tangent, 0.0)));
    vec3 B = normalize(vec3(model * vec4(bitangent, 0.0)));
    vec3 N = normalize(vec3(model * vec4(normal, 0.0)));
    vsOut.TBN = mat3(T, B, N);
}

//$Fragment
#version 450 core

struct MaterialPBR
{
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
};

in VSOut
{
    vec2 texCoord;
    vec3 fragPos;
    vec3 normal;
    mat3 TBN;
} fsIn;

layout(location = 0) out vec4 Albedo;
layout(location = 1) out vec4 Normal;
layout(location = 2) out vec4 DepMetRou;
layout(location = 3) out vec4 Position;

uniform MaterialPBR material;
uniform sampler2D ordTex;
uniform sampler2D normalMap;
uniform bool useTexture;
uniform bool useNormalMap;
uniform bool forceFlatNormals;

void main()
{
    vec3 fragPos = fsIn.fragPos;
    vec2 texCoord = fsIn.texCoord;
    vec3 addNorm = texture(normalMap, texCoord).rgb;
    addNorm = normalize(addNorm * 2.0 - 1.0);
    vec3 newNorm = useNormalMap ? normalize(fsIn.TBN * addNorm) : fsIn.normal;
    newNorm = forceFlatNormals ? normalize(cross(dFdx(fragPos), dFdy(fragPos))) : newNorm;

    float fragDepth = gl_FragCoord.z + material.ao * 1e-14;

    vec3 albedo = material.albedo;
    if (useTexture) albedo *= texture(ordTex, texCoord).rgb;
    Albedo = vec4(albedo, 1.0);
    Normal = vec4(newNorm, 1.0);
    DepMetRou = vec4(fragDepth, material.metallic, material.roughness, 1.0);
    Position = vec4(fragPos, 1.0);
}