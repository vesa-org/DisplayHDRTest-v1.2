// Custom effects using pixel shaders should use HLSL helper functions defined in
// d2d1effecthelpers.hlsli to make use of effect shader linking.
#define D2D_INPUT_COUNT 0           // The pixel shader is a source and does not take inputs.
#define D2D_REQUIRES_SCENE_POSITION // The pixel shader requires the SCENE_POSITION input.

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float dpi : packoffset(c0.x);
    float2 center : packoffset(c0.y);
    float initialWavelength : packoffset(c0.w);
    float wavelengthHalvingDistance : packoffset (c1.x);
    float whiteLevelMultiplier : packoffset (c1.y);			// actual nits to tone map to
};

#define iTime (center.x)    // TODO: replace hack with real declaration of time as an input constant to shader.

float pi = 3.14159265;
float pisquared = 9.8696;

// rational approximation to cosine
float bhaskara( float n)
{
    float r1 = fmod( n, 6.283185307f );  // Last arg of fmod() has to be an immediate constant else result is zero!!!!!
    return r1*5.f;

    float r2 = r1-pi;
    if (abs(r2)<pi*.5)
    {  
        return -(pisquared-4.0*r2*r2)/(pisquared+r2*r2);
    }
    else
    {
        r2-=pi*1.0;
        if (r2<-pi*.5)
        {
            r2+=2.0*pi;
        }
        return (pisquared-4.0*r2*r2)/(pisquared+r2*r2);
    }   
}

#if 0
float hash( float2 p )
{
//  return frac(bhaskara(p.x + p.y * 57.1235)*54671.57391);
    return frac(     cos(p.x + p.y * 57.1235)*54671.57391);
}

float hash( float2 p)       // hash12 floating point
{
    float3 p3 = frac(float3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}
#endif

float hash( int2 p )            // integers require shader model 5.0 or higher
{
    // 2D -> 1D
    int n = p.x * 3 + p.y * 113;

    // 1D hash by Hugo Elias
    n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
//  return -1.0 + 2.0 * float(n & 0x0fffffff) / float(0x0fffffff);
    return              float(n & 0x0fffffff) / float(0x0fffffff);
}

float noise( float2 p )
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f*f*(3.0-2.0*f);             // cubic interpolant aka smoothstep()

    return lerp(lerp( hash( i + float2( 0.0, 0.0 ) ), hash( i + float2( 1.0, 0.0 ) ), f.x ),
                lerp( hash( i + float2( 0.0, 1.0 ) ), hash( i + float2( 1.0, 1.0 ) ), f.x ), f.y);
}

D2D_PS_ENTRY(main)
{
    float n = 0.0;
    float wave = 3.0;
    float inv = 1.0/1440.f;

    float2 pos = D2DGetScenePosition().xy;				// units of pixels

    for (int i=0; i<8; i++)
    {
        n += noise(floor(pos.xy*wave + iTime*3840.0.x)*inv);
        wave *= 3.0;
    }
    
    n *= 0.125f;  // divide by number of layers
    n -= 0.1;
    n *= n;     // make symmetrical in gamma-corrected space
    n *= whiteLevelMultiplier;

    return float4( float3(n,n,n), 1.f );
}