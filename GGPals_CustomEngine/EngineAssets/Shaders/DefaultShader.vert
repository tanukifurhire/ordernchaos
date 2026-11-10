#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;

struct InstanceData {
    vec3 translation;
    float rotation;
    vec3 scale;
    uint useTexture;
    vec4 uvBounds;  // Now: minU, minV, maxU, maxV
    uint textureID;
    uint _pad_texID;
    int layer;
    int ySort;
    vec4 colorAdd;
    vec4 colorMultiply;
    uint isFontTexture;  //  1 for font, 0 for sprite
    uint effectType;    
    uint loop;    // 
    uint useEffect; 
    uint padding;  //
    float intensity;  //  
    float startTime;  
    float effectDuration;  //  
    vec4 effectColor; 
 };

layout(std430, binding = 0) buffer TranslationBuffer {
    InstanceData translations[];
};

uniform bool uUseInstancing = false;
uniform bool uUseUVTransform = true;
uniform mat4 projection;
uniform mat4 worldscale;
uniform mat4 cameraView;

out vec4 ourColor;
out vec4 instanceColorAdd;
out vec4 instanceColorMultiply;
out vec2 TexCoord;
flat out uint   isFontTexture;


flat out uint   shouldUseTexture;
flat out uint   isInstanced;

flat out uint  vEffectType;
flat out uint  vLoop;
flat out float vIntensity;
flat out float vEffectDuration;
out vec4       vEffectColor;
flat out float vEffectStartTime;
flat out uint vUseEffect;

mat4 computeModelMatrix(InstanceData data) {
    // Scale
    mat4 S = mat4(1.0);
    S[0][0] = data.scale.x;
    S[1][1] = data.scale.y;
    S[2][2] = data.scale.z;

    // Rotation (Z-axis)
    float c = cos(data.rotation);
    float s = sin(data.rotation);
    mat4 R = mat4(1.0);
    R[0][0] =  c; R[0][1] = s;
    R[1][0] = -s; R[1][1] = c;

    // Translation
    mat4 T = mat4(1.0);
    T[3].x = data.translation.x;
    T[3].y = data.translation.y;
    T[3].z = 0.0;  // Don't use translation.z for actual translation

    return T * R * S;
}

void main() {
    if (uUseInstancing) {
        //vInstanceID = gl_InstanceID;
        InstanceData data = translations[gl_InstanceID];
        mat4 model = computeModelMatrix(data);

        gl_Position = projection *(cameraView *(model * (worldscale * vec4(aPos, 1.0))));  

        if (data.translation.z > 0.0)
        {
            gl_Position.w = 1.0 + gl_Position.y * data.translation.z;
        }

        if (uUseUVTransform) {
            // Interpolate between min (xy) and max (zw) UV coordinates
            // aTexCoord goes from (0,0) to (1,1), so we map it to our frame bounds
            TexCoord.x = mix(data.uvBounds.x, data.uvBounds.z, aTexCoord.x);
            TexCoord.y = mix(data.uvBounds.y, data.uvBounds.w, aTexCoord.y);
        } else {
            // Fallback to original texture coordinates  
            TexCoord = aTexCoord;
        }

        ourColor = aColor;
        instanceColorAdd = data.colorAdd;
        instanceColorMultiply = data.colorMultiply;
        isInstanced = 1;

        shouldUseTexture = data.useTexture;
        isFontTexture = data.isFontTexture;
        vEffectType     = data.effectType;
        vLoop           = data.loop;
        vIntensity      = data.intensity;
        
        vEffectDuration = data.effectDuration;
        vEffectColor    = data.effectColor;
        vUseEffect = data.useEffect;
        vEffectStartTime = data.startTime;
    } else {
        gl_Position = projection * vec4(aPos, 1.0);
        ourColor = aColor;

        instanceColorAdd = vec4(0.0);
        instanceColorMultiply = vec4(0.0);
        isInstanced = 0;
        isFontTexture = 0; 

        vEffectType     = 0;
        vLoop           = 0;
        vIntensity      = 1.0;

        vEffectDuration = 1.0;
        vEffectColor    = vec4(1.0);
       vUseEffect =0;
       vEffectStartTime = 0;
        TexCoord = aTexCoord;
    }
}