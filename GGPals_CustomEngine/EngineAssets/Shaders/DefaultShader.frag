#version 450 core

out vec4 FragColor;

in vec4 ourColor;
in vec4 instanceColorAdd;
in vec4 instanceColorMultiply;
in vec2 TexCoord;

flat in uint   shouldUseTexture;
flat in uint   isInstanced;
flat in uint   isFontTexture;

flat in uint  vEffectType;
flat in uint  vLoop;
flat in float vIntensity;
flat in float vEffectDuration;
in  vec4      vEffectColor;
flat in uint vUseEffect;
flat in float vEffectStartTime;

uniform sampler2D uTexture;
uniform vec4 colorAdd;
uniform vec4 colorMultiply;
uniform float uTime;

// EffectType enum values matching your C++ enum
#define EFFECT_NONE           0u
#define EFFECT_FOG            1u
#define EFFECT_FADE           2u
#define EFFECT_BLOOM          3u
#define EFFECT_OUTLINE        4u
#define EFFECT_SQUARE_DISSOLVE 5u
#define EFFECT_SHINE          6u
#define EFFECT_SHINE_RTL      7u
#define EFFECT_BURN_DISSOLVE   8u
#define EFFECT_PIXEL_DISSOLVE   9u
#define EFFECT_PULSE  10u

// Hash function for dissolve noise (no texture needed)
float hash(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}
// Smooth noise: bilinear interpolation over hash grid
float smoothNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f); // smoothstep curve

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Two-octave FBM — gives the organic "cloud texture" feel from the blog
float fbm(vec2 p) {
    float v = 0.0;
    v += 0.600 * smoothNoise(p * 1.0);
    v += 0.400 * smoothNoise(p * 2.5);
    return v; // [0, 1]
}

// Normalised progress 0 1, respects loop flag
float EffectProgress() {
     if (vEffectDuration <= 0.0) return 1.0;
    float elapsed = uTime - vEffectStartTime;
    float t = elapsed / vEffectDuration;
    if (vLoop == 1u)
        t = fract(t);
    return clamp(t, 0.0, 1.0);
}

void main() {

    vec4 baseColor;

    if (shouldUseTexture == 1u) 
    {
        vec4 texColor = texture(uTexture, TexCoord);

        vec3 finalColor;
        float finalAlpha;
        float glyphAlpha = texColor.r;

        if (isInstanced == 1u)
        {
            if (isFontTexture == 1u) {
                if (glyphAlpha < 0.01)
                    discard;

                // Font rendering: RED channel is the alpha mask

                finalAlpha = glyphAlpha * instanceColorMultiply.a;
                finalColor = instanceColorMultiply.rgb * finalAlpha;
            }
            else
            {
                // Regular sprite rendering
                finalColor = (texColor.rgb + instanceColorAdd.rgb * instanceColorAdd.a) * instanceColorMultiply.rgb;
                finalAlpha = texColor.a * instanceColorMultiply.a;
            }
        }
        else
        {
            if (isFontTexture == 1u) {
                // Font rendering: RED channel is the alpha mask
                glyphAlpha = texColor.r;
                if (glyphAlpha < 0.01) discard;
                finalAlpha = glyphAlpha * colorMultiply.a;
                finalColor = colorMultiply.rgb * finalAlpha;
            } 
            else 
            {
                // Regular sprite rendering
                finalColor = (texColor.rgb + colorAdd.rgb) * colorMultiply.rgb;
                finalAlpha = texColor.a;
            }
        }
        
        baseColor = vec4(finalColor, finalAlpha);
    }
    else 
    {
        // No texture - use vertex/instance colors
        if (isInstanced == 1u)
        {
            baseColor = (ourColor + instanceColorAdd) * instanceColorMultiply;
        }
        else
        {
            baseColor = (ourColor + colorAdd) * colorMultiply;
        }
    }

    vec4 result = baseColor;
    
    if(vUseEffect==1u)
    {
        // ----------------------------------------------------------------
        // 2. Early out — no effect, output base colour directly
        // ----------------------------------------------------------------
        if (vEffectType == EFFECT_NONE) {
            FragColor = baseColor;
            return;
        }

        // ----------------------------------------------------------------
        // 3. Effect processing
        // ----------------------------------------------------------------
        float progress = EffectProgress();
    

        // --- FADE ---
        // effectTimer goes 0→effectDuration
        // alpha fades from intensity down to 0
        if (vEffectType == EFFECT_FADE) {
            result.a *= mix(vIntensity, 0.0, progress);
            if (result.a < 0.001) discard;
        }

        // --- FOG ---
        // Blends the sprite toward a fog colour over time
        // effectColor = fog colour, intensity = max fog strength
        else if (vEffectType == EFFECT_FOG) 
        {
            vec2 fogOrigin = vec2(0.5, 1.0);
            float radial   = distance(TexCoord, fogOrigin);
            float vertical = TexCoord.y;

            // use progress directly, intensity controls edge softness not speed
            float reveal = progress;
            float edgeSoftness = max(0.05, 1.0 / max(vIntensity, 0.1));

            float fog = smoothstep(reveal, reveal + edgeSoftness, radial + vertical * 0.5);
            fog = 1.0 - fog;
            result.a   = mix(result.a, 0.0, 1.0 - fog);
        }

        // --- BLOOM ---
        else if (vEffectType == EFFECT_BLOOM) 
        {
            result.rgb *= vIntensity;
        }

        // --- SHINE ---
        else if (vEffectType == EFFECT_SHINE) 
        {
            vec2 pos = TexCoord;
            pos.x -= mix(-0.5, 1.5, progress);

            vec2 a = vec2(0.0, 1.0);
            vec2 b = vec2(1.0, 0.0);
            vec2 pa = pos - a;
            vec2 ba = b - a;
            float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
            float s = length(pa - ba * h);

            s = 1.0 - smoothstep(0.0, max(vIntensity, 0.01), s);
            s = max(s, 0.0);
            s *= step(0.1, result.a);

            // Unpremultiply, add shine, repremultiply
            vec3 straight = result.a > 0.0 ? result.rgb / result.a : vec3(0.0);
            straight += vEffectColor.rgb * s * vEffectColor.a;
            result.rgb = straight * result.a;
        }

        // --- SQUARE_DISSOLVE ---
        else if (vEffectType == EFFECT_SQUARE_DISSOLVE) 
        {
            // Snap UV to grid of blocks
            float blockSize = max(vIntensity * 0.1, 0.01);
            vec2  blockUV   = floor(TexCoord / blockSize) * blockSize;
            float noise     = hash(blockUV);
            // Discard blocks whose noise value is below progress threshold
            if (noise < progress) discard;
        }

        // --- OUTLINE ---

        else if (vEffectType == EFFECT_OUTLINE) 
        {
            if (shouldUseTexture == 1u) 
            {
                vec2 texelSize = vec2(1.0) / vec2(textureSize(uTexture, 0));
                float thickness = max(vIntensity, 1.0); // ensure at least 1 texel offset

                // Sample neighbors
                float alphaUp    = texture(uTexture, TexCoord + vec2(0.0,  texelSize.y * thickness)).a;
                float alphaDown  = texture(uTexture, TexCoord + vec2(0.0, -texelSize.y * thickness)).a;
                float alphaLeft  = texture(uTexture, TexCoord + vec2(-texelSize.x * thickness, 0.0)).a;
                float alphaRight = texture(uTexture, TexCoord + vec2( texelSize.x * thickness, 0.0)).a;

                float maxNeighbourAlpha = max(max(alphaUp, alphaDown), max(alphaLeft, alphaRight));

                // Only draw outline around transparent pixels with opaque neighbors
                if (result.a < 0.1 && maxNeighbourAlpha > 0.1) {
                    result = vec4(vEffectColor.rgb, vEffectColor.a); // constant, not affected by time
                }
            }
        }

        else if(vEffectType == EFFECT_SHINE_RTL)
        {
             vec2 pos = TexCoord;
             // Mirror: sweep from right (1.5) to left (-0.5) as progress goes 0→1
             pos.x -= mix(1.5, -0.5, progress);

             vec2 a = vec2(1.0, 1.0);
             vec2 b = vec2(0.0, 0.0);
             vec2 pa = pos - a;
             vec2 ba = b - a;
             float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
             float s = length(pa - ba * h);

             s = 1.0 - smoothstep(0.0, max(vIntensity, 0.01), s);
             s = max(s, 0.0);
             s *= step(0.1, result.a);

             vec3 straight = result.a > 0.0 ? result.rgb / result.a : vec3(0.0);
             straight += vEffectColor.rgb * s * vEffectColor.a;
             result.rgb = straight * result.a;

        }
        
        else if (vEffectType == EFFECT_BURN_DISSOLVE)
        {
            float freq      = max(vIntensity, 0.5);
            float noise     = fbm(TexCoord * freq);
            float isVisible = noise - progress;

            // Dissolve — clip pixels whose noise falls below progress threshold
            if (isVisible < 0.0) discard;

            // Burn edge — smoothstep gradient from hole edge outward
            float burnSize = 0.08 + 0.04 * (1.0 / freq);
            float burnEdge = abs(burnSize) + 0.0001;

            // Use correct smoothstep arg order (edge0 < edge1) to avoid AMD UB
            float isBurning = (1.0 - smoothstep(0.0, burnEdge, isVisible))
                            * step(0.0001, progress); // suppress glow at progress=0

            // Unpremultiply, add glow, repremultiply
            vec3 straight = result.a > 0.0 ? result.rgb / result.a : vec3(0.0);
            straight += vEffectColor.rgb * isBurning * vEffectColor.a * 2.0;
            result.rgb = straight * result.a;
        }

        
        else if (vEffectType == EFFECT_PIXEL_DISSOLVE)
        {
            float freq = max(vIntensity, 0.5);

            // Two phases
           float dissolveT = progress;

            // Drift starts slightly later for smoother feel
            float driftT = smoothstep(0.15, 1.0, progress);

            // Second noise layer for drift direction per fragment
            float noiseAngle = fbm(TexCoord * freq * 0.7 + vec2(5.3, 1.7)) * 6.2832;
            float noiseDrift = fbm(TexCoord * freq * 1.3 + vec2(2.1, 8.4));

            vec2 driftDir    = vec2(cos(noiseAngle), sin(noiseAngle));
            vec2 driftOffset = driftDir * driftT * noiseDrift * 0.15;

            // Control particle size (IMPORTANT)
            float particleSize = 0.04; // try 0.02 (small) → 0.08 (big)

            // Snap UVs to a grid → creates "chunks"
            vec2 gridUV = floor(TexCoord / particleSize) * particleSize;

            // Apply drift to the whole chunk instead of per-pixel
            vec2 driftedUV = gridUV + driftOffset;

            // Sample noise per chunk (not per pixel)
            float driftNoise = fbm(driftedUV * freq);

            // Dissolve threshold — same logic as effect 8
            float isVisible  = driftNoise - dissolveT;
            if (isVisible < 0.0) discard;

            // Burn edge on dissolve front
            float burnSize  = 0.08 + 0.04 * (1.0 / freq);
            float burnEdge  = abs(burnSize) + 0.0001;
            // Use correct smoothstep arg order (edge0 < edge1) to avoid AMD UB
            float isBurning = (1.0 - smoothstep(0.0, burnEdge, isVisible))
                            * step(0.0001, dissolveT);

            // Phase 2: fade remaining fragments out progressively
            // Lower noise value = fades sooner, consistent with dissolve order
            float fragmentFade = 1.0;
            if (driftT > 0.0)
            {
                float fadeThreshold = driftT;
                fragmentFade = clamp((driftNoise - fadeThreshold) / 0.3, 0.0, 1.0);
                fragmentFade = smoothstep(0.0, 1.0, fragmentFade);
            }

            result.a *= fragmentFade;
            if (result.a < 0.005) discard;

            // Unpremultiply -> add glow + ember tint -> repremultiply
            vec3 straight = result.a > 0.0 ? result.rgb / result.a : vec3(0.0);

            // Burn edge glow
            straight += vEffectColor.rgb * isBurning * vEffectColor.a * 2.0;

            // Ember tint on drifting fragments — intensifies as they fade out
            if (driftT > 0.0)
            {
                float emberStrength = driftT * (1.0 - fragmentFade) * vEffectColor.a;
                straight = mix(straight, vEffectColor.rgb * 2.0, clamp(emberStrength, 0.0, 0.8));
            }

            result.rgb = straight * result.a;
        }
        
        else if (vEffectType == EFFECT_PULSE)
        {
             float elapsed = uTime - vEffectStartTime;
             float loopT   = fract(elapsed / max(vEffectDuration, 0.001));

             vec2 center = vec2(0.5);
             float radius = length(TexCoord - center);

             // Ring expands outward
             float ringRadius = loopT * 0.8;

             // MUCH more visible thickness
             float thickness = max(vIntensity * 0.2, 0.02);

             // Smooth ring band (THIS is the key fix)
             float ring = smoothstep(ringRadius - thickness, ringRadius, radius)
                        - smoothstep(ringRadius, ringRadius + thickness, radius);

             // Fade near edges to avoid clipping
             float edgeFade = 1.0 - smoothstep(0.7, 0.9, ringRadius);
             ring *= edgeFade;

             // Only affect visible pixels
             ring *= step(0.1, result.a);

             if (ring > 0.001)
             {
             vec3 straight = result.a > 0.0 ? result.rgb / result.a : vec3(0.0);

             float darken = ring * vEffectColor.a;
             straight *= (1.0 - darken);

             result.rgb = straight * result.a;
             }
        }
    }

    FragColor = result;
}
