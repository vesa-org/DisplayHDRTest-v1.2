//********************************************************* 
// 
// Copyright (c) Microsoft. All rights reserved. 
// This code is licensed under the MIT License (MIT). 
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY 
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR 
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT. 
// 
//*********************************************************
// 
#define M_E 2.718281828459045235360f
// 
// Custom effects using pixel shaders should use HLSL helper functions defined in
// d2d1effecthelpers.hlsli to make use of effect shader linking.
#define D2D_INPUT_COUNT 0           // The pixel shader is a source and does not take inputs.
#define D2D_REQUIRES_SCENE_POSITION // The pixel shader requires the SCENE_POSITION input.

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float dpi : packoffset(c0.x);
    float APL : packoffset(c0.y);           // APL -Average Picture Level
    float Clamp : packoffset(c0.z);         // No pixel values above this limit (nits)
    float iTime : packoffset (c0.w);        // time since app start in seconds
};

/*
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

 float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);             // cubic interpolant aka smoothstep()

    return lerp(lerp(hash(i + float2(0.0, 0.0)), hash(i + float2(1.0, 0.0)), f.x),
        lerp(hash(i + float2(0.0, 1.0)), hash(i + float2(1.0, 1.0)), f.x), f.y);
}
*/

// https://www.pcg-random.org/
uint pcg(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// https://www.shadertoy.com/view/XlGcRh
// Uniform value in range [0..1)
float hash( int2 u )
{
    return float(pcg(pcg(u.x) + u.y)) / float(0xFFFFFFFFU);     // normalize to 1.0
}

/*
a = 1;  G = (a - 1)! = 1;           // exponential distribution
a = 2;  G = (a - 1)! = 1;           // gamma distribution
*/

D2D_PS_ENTRY(main)
{
    float x;
    float c = 10000;                                    // output color init to out of range
    float2 pos = D2DGetScenePosition().xy;      	    // units of pixels

    pos.x -= iTime * 60.0;                              // animate
    pos.y += iTime * 60.0;

    //  pos /= 4;                                       // derez into tiles

    int i = 0;
    while (c > Clamp)                                   // check vs Clamp
    {
        x = hash(pos + x * i);                          // uniform from [0 to 1.)
        c = -log(1.0 - x) * APL;
        i++;
    }

    c = c/80.f;                                         // scale from nits to CCCS brightness units
//  c = pow(c, 2.2);                                    // required in SDR mode
    return float4( c, c, c, 1.0 );
}