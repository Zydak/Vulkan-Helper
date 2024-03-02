#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT hitPayload payload;
#include "CookTorranceSampling.glsl"

hitAttributeEXT vec2 attribs;

layout(push_constant) uniform _PushConstantRay { PushConstantRay push; };

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { int i[]; };

layout(set = 0, binding = 0) uniform accelerationStructureEXT uTopLevelAS;
layout(set = 0, binding = 2, scalar) buffer MeshAdressesUbo { MeshAdresses uMeshAdresses[]; };
layout(set = 0, binding = 3, scalar) buffer MaterialsUbo { Material uMaterials[]; };
layout(set = 0, binding = 4) uniform sampler2D uAlbedoTextures[];
layout(set = 0, binding = 5) uniform sampler2D uNormalTextures[];
layout(set = 0, binding = 6) uniform sampler2D uRoghnessTextures[];
layout(set = 0, binding = 7) uniform sampler2D uMetallnessTextures[];
layout(set = 1, binding = 1) uniform sampler2D uEnvMap;

layout(set = 1, binding = 2) readonly buffer uEnvMapAccel
{
    EnvAccel[] uAccels;
};

#define SAMPLE_IMPORTANCE
#include "envSampling.glsl"

struct HitState
{
    vec3 RayDir;
    vec3 HitValue;
    vec3 RayOrigin;
    vec3 Weight;

    bool Valid;
};

vec3 SampleHenyeyGreenstein(vec3 incomingDir, float g, float rand1, float rand2) 
{
    float phi = 2.0 * M_PI * rand1;
    float cosTheta;
    if (abs(g) < 1e-3) 
    {
        cosTheta = 1.0 - 2.0 * rand2;
    } else 
    {
        float sqrTerm = (1.0 - g * g) / (1.0 - g + 2.0 * g * rand2);
        cosTheta = (1.0 + g * g - sqrTerm * sqrTerm) / (2.0 * g);
    }
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    vec3 tangent = normalize(cross(incomingDir, abs(incomingDir.x) > 0.1 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 bitangent = cross(incomingDir, tangent);
    vec3 scatteredDir = incomingDir * cosTheta + tangent * sinTheta * cos(phi) + bitangent * sinTheta * sin(phi);
    return normalize(scatteredDir);
}

float SampleExponentialDistance(float extinctionCoefficient, float rand) 
{
    return -log(1.0 - rand) / extinctionCoefficient;
}

float CalculateOpticalDepth(float extinctionCoefficient, float distance) 
{
    return extinctionCoefficient * distance;
}

bool ShouldScatter(float opticalDepth, float rand) 
{
    return rand <= 1.0 - exp(-opticalDepth);
}

HitState ClosestHit(Material mat, Surface surface, vec3 worldPos)
{
    HitState state;
    
    vec3 hitValue = mat.Emissive.xyz;
    
#ifdef USE_FOG
    float distance = length(payload.RayOrigin - worldPos);
    
    float aFactor = 0.1f;
    float sFactor = 0.000001f;
    float tFactor = sFactor + aFactor;
    float Tr = exp(-tFactor * distance);
    float r0 = Rnd(payload.Seed);
    float r1 = Rnd(payload.Seed);
    float r2 = Rnd(payload.Seed);
    float r3 = Rnd(payload.Seed);

    float opticalDepth = CalculateOpticalDepth(aFactor, distance);

    float scatterDist = SampleExponentialDistance(aFactor, r3);
    if (scatterDist < distance)
    {
        state.RayOrigin = payload.RayOrigin + payload.RayDirection * scatterDist;
        state.RayDir = SampleHenyeyGreenstein(payload.RayDirection, 0.0f, r1, r2);
        state.HitValue = hitValue;
        state.Weight = vec3(1.0f) * (1.0f - Tr);
        
        state.Valid = true;
        return state;
    }
#endif
    
    BsdfSampleData sampleData;
    sampleData.View = -gl_WorldRayDirectionEXT;  // outgoing direction
    sampleData.Random = vec4(Rnd(payload.Seed), Rnd(payload.Seed), Rnd(payload.Seed), Rnd(payload.Seed));
    BsdfSample(sampleData, surface, mat);
    
    if (sampleData.EventType == EVENT_TYPE_END)
    {
        state.Valid = false;
        return state;
    }

// TODO: sampling env map is insanely slow for some reason, disabling this gives me almost 200% more performance
#ifdef SAMPLE_ENV_MAP
    vec3 dirToLight;
    vec3 contribution = vec3(0.0f);
    vec3 randVal = vec3(Rnd(payload.Seed), Rnd(payload.Seed), Rnd(payload.Seed));
    vec4 envColor = SampleImportanceEnvMap(uEnvMap, randVal, dirToLight);

    bool nextEventValid = !(dot(dirToLight, surface.Normal) < 0.0f);

    if (nextEventValid)
    {
        BsdfEvaluateData evalData;
        evalData.View    = -gl_WorldRayDirectionEXT;
        evalData.Light   = dirToLight;

        BsdfEvaluate(evalData, surface, mat);
        if(evalData.Pdf > 0.0)
        {
            const float misWeight = envColor.w / (envColor.w + evalData.Pdf);
            const vec3 w = (envColor.xyz / envColor.w) * misWeight;

            contribution += w * evalData.Bsdf;
        }

        uint prevDepth = payload.Depth;

        float tMin     = 0.001;
        float tMax     = 10000.0;
        uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        
        traceRayEXT(
            uTopLevelAS,        // acceleration structure
            flags,              // rayFlags
            0xFF,               // cullMask
            0,                  // sbtRecordOffset
            0,                  // sbtRecordStride
            0,                  // missIndex
            worldPos,           // ray origin
            tMin,               // ray min range
            dirToLight,         // ray direction
            tMax,               // ray max range
            0                   // payload (location = 0)
        );
        
        if (payload.Depth == DEPTH_INFINITE) // Didn't hit anything
        {
            hitValue += contribution;
        }

        payload.Depth = prevDepth;
        payload.MissedAllGeometry = false;
    }
#endif
    
    state.Weight    = sampleData.Bsdf / sampleData.Pdf;
    state.RayDir    = sampleData.RayDir;
    vec3 offsetDir  = dot(payload.RayDirection, surface.Normal) > 0 ? surface.Normal : -surface.Normal;
    state.RayOrigin = OffsetRay(worldPos, offsetDir);
    state.HitValue  = hitValue;

    state.Valid = true;

    return state;
}

void main() 
{
    // -------------------------------------------
    // Get Data From Buffers
    // -------------------------------------------
    MeshAdresses meshAdresses = uMeshAdresses[gl_InstanceCustomIndexEXT];
    Indices indices = Indices(meshAdresses.IndexBuffer);
    Vertices vertices = Vertices(meshAdresses.VertexBuffer);
    Material material = uMaterials[gl_InstanceCustomIndexEXT];
    
    int i1 = indices.i[(gl_PrimitiveID * 3)];
    int i2 = indices.i[(gl_PrimitiveID * 3)+1];
    int i3 = indices.i[(gl_PrimitiveID * 3)+2];
    
    Vertex v0 = vertices.v[i1];
    Vertex v1 = vertices.v[i2];
    Vertex v2 = vertices.v[i3];

    // -------------------------------------------
    // Calculate Surface Properties
    // -------------------------------------------

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 texCoord = v0.TexCoord.xy * barycentrics.x + v1.TexCoord.xy * barycentrics.y + v2.TexCoord.xy * barycentrics.z;

    const vec3 pos      = v0.Position.xyz * barycentrics.x + v1.Position.xyz * barycentrics.y + v2.Position.xyz * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space
     
    // Computing the normal at hit position
    const vec3 nrm      = v0.Normal.xyz * barycentrics.x + v1.Normal.xyz * barycentrics.y + v2.Normal.xyz * barycentrics.z;
    vec3 worldNrm = normalize(vec3(nrm * gl_ObjectToWorldEXT));  // Transforming the normal to world space

    material.eta = dot(gl_WorldRayDirectionEXT, worldNrm) < 0.0 ? (1.0 / material.Ior) : material.Ior;

    const vec3 V = -gl_WorldRayDirectionEXT;
    if (dot(worldNrm, V) < 0)
    {
        worldNrm = -worldNrm;
    }

    Surface surface;
    surface.Normal = worldNrm;
    surface.GeoNormal = worldNrm;
    CalculateTangents(worldNrm, surface.Tangent, surface.Bitangent);

#ifdef USE_NORMAL_MAPS
    vec3 normalMapVal = texture(uNormalTextures[gl_InstanceCustomIndexEXT], texCoord).xyz;
    normalMapVal = normalMapVal * 2.0f - 1.0f;
    
    normalMapVal = TangentToWorld(surface.Tangent, surface.Bitangent, worldNrm, normalMapVal);
    surface.Normal = normalize(normalMapVal);
    
    CalculateTangents(worldNrm, surface.Tangent, surface.Bitangent);
#endif

    // -------------------------------------------
    // Calculate Material Properties
    // -------------------------------------------
    float aspect = sqrt(1.0 - material.Anisotropic * 0.99);
    material.ax = max(0.001, material.Roughness / aspect);
    material.ay = max(0.001, material.Roughness * aspect);

#ifdef USE_ALBEDO
    material.Albedo *= texture(uAlbedoTextures[gl_InstanceCustomIndexEXT], texCoord);
#else
    material.Albedo = vec4(0.5f);
#endif

    material.Emissive.xyz *= material.Emissive.a;
    material.SpecTrans = 1.0f - material.Albedo.a;
    
    // -------------------------------------------
    // Hit
    // -------------------------------------------
    HitState hitState = ClosestHit(material, surface, worldPos);

    if (hitState.Valid == false)
    {
        payload.Depth = DEPTH_INFINITE;
        return;
    }
    
    payload.Weight       = hitState.Weight;
    payload.RayDirection = hitState.RayDir;
    payload.RayOrigin    = hitState.RayOrigin;
    payload.HitValue     = hitState.HitValue;
}