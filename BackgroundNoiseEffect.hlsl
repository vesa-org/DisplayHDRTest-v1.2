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

//  pos /= 4;                                           // derez into tiles

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