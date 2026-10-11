#include <metal_stdlib>
using namespace metal;
float4 sampleTex(texture2d<float> tex, float2 uv) {
 constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::linear);
 return tex.sample(s,uv);
}
struct V { float4 position [[position]]; float2 uv; };
vertex V crtVertex(uint id [[vertex_id]]) {
 float2 p=float2((id<<1)&2,id&2);
 V v;v.position=float4(p*float2(2,-2)+float2(-1,1),0,1);v.uv=p;return v;
}
struct Params { float2 sourceSize;float2 outputSize;float frame;int mode; };
// NewPixie CRT by Mattias Gustavsson (2016), Slang adaptation by hunterk.
// Native adaptation: same four passes, default parameters, optional frame artwork omitted.
// See licenses/NewPixie-MIT.txt. Input UV is top-to-bottom on both backends.
float3 tsample(texture2d<float> tex, float2 tc) {
    tc=tc*float2(1.025,0.92)+float2(-0.0125,0.04);
    return pow(abs(sampleTex(tex,float2(tc.x,1.0-tc.y)).rgb),float3(2.2))*1.25;
}
float3 filmic(float3 c) {
    float3 x=max(float3(0.0),c-0.004);
    return (x*(6.2*x+0.5))/(x*(6.2*x+1.7)+0.06);
}
float2 curve(float2 uv) {
    uv=(uv-0.5)*float2(0.925,1.095)*2.0;
    uv.x*=1.0+pow(abs(uv.y)/4.0,2.0);
    uv.y*=1.0+pow(abs(uv.x)/3.0,2.0);
    return (uv/2.0+0.5)*0.92+0.04;
}
float noise(float2 co) { return fract(sin(dot(co,float2(12.9898,78.233)))*43758.5453); }
float4 newpixie(texture2d<float> backbuffer, texture2d<float> blurbuffer, float2 inputUV, float2 resolution, float frame) {
    float2 uv=float2(inputUV.x,1.0-inputUV.y);
    float2 curved=mix(curve(uv),uv,0.4);
    float2 scuv=curved*1.101-0.0505+float2(0.003,-0.001);
    float x=sin(uv.y*resolution.y*1.5)/resolution.x*0.25;
    float time=fmod(frame,640.0);
    float3 col;
    col.r=tsample(backbuffer,float2(x+scuv.x+0.0009,scuv.y+0.0009)).r+0.02;
    col.g=tsample(backbuffer,float2(x+scuv.x,scuv.y-0.0011)).g+0.02;
    col.b=tsample(backbuffer,float2(x+scuv.x-0.0015,scuv.y)).b+0.02;
    float i=clamp(dot(col,float3(0.299,0.587,0.114)),0.0,1.0);
    i=i*i*0.85+0.15;
    float3 r=tsample(blurbuffer,float2(x-0.014,-0.027)*0.85+0.007*float2(
        0.35*sin(1.0/7.0+15.0*curved.y+0.9*time),
        0.35*sin(2.0/7.0+10.0*curved.y+1.37*time))+scuv+float2(0.001))*float3(0.5,0.25,0.25);
    float3 g=tsample(blurbuffer,float2(x-0.019,-0.020)*0.85+0.007*float2(
        0.35*cos(1.0/9.0+15.0*curved.y+0.5*time),
        0.35*sin(2.0/9.0+10.0*curved.y+1.50*time))+scuv+float2(0.000,-0.002))*float3(0.25,0.5,0.25);
    float3 b=tsample(blurbuffer,float2(x-0.017,-0.003)*0.85+0.007*float2(
        0.35*sin(2.0/3.0+15.0*curved.y+0.7*time),
        0.35*cos(2.0/3.0+10.0*curved.y+1.63*time))+scuv+float2(-0.002,0.000))*float3(0.25,0.25,0.5);
    col+=0.15*(1.0-0.299)*pow(clamp(3.0*r,float3(0),float3(1)),float3(2))*i;
    col+=0.15*(1.0-0.587)*pow(clamp(3.0*g,float3(0),float3(1)),float3(2))*i;
    col+=0.15*(1.0-0.114)*pow(clamp(3.0*b,float3(0),float3(1)),float3(2))*i;
    col*=float3(0.95,1.05,0.95);
    col=clamp(col*1.3+0.75*col*col+1.25*col*col*col*col*col,float3(0),float3(10));
    float vig=0.01+16.0*curved.x*curved.y*(1.0-curved.x)*(1.0-curved.y);
    col*=1.3*sqrt(max(vig,0.0));
    col*=pow(clamp(0.35+0.18*sin(6.0*time-curved.y*resolution.y*1.5),0.0,1.0),0.9);
    col*=1.0-0.23*clamp(fmod(uv.x*resolution.x,3.0)/2.0,0.0,1.0);
    col=filmic(col);
    float2 seed=curved*resolution;
    col-=0.015*pow(float3(noise(seed+time),noise(seed+time*2.0),noise(seed+time*3.0)),float3(1.5));
    col*=1.0-0.004*(sin(50.0*time+curved.y*2.0)*0.5+0.5);
    return float4(col,1.0);
}
float4 blur9(texture2d<float> tex,float2 uv,float2 stepUV) {
    float4 c=sampleTex(tex,uv)*0.2270270270;
    c+=(sampleTex(tex,uv-stepUV)+sampleTex(tex,uv+stepUV))*0.1945945946;
    c+=(sampleTex(tex,uv-stepUV*2.0)+sampleTex(tex,uv+stepUV*2.0))*0.1216216216;
    c+=(sampleTex(tex,uv-stepUV*3.0)+sampleTex(tex,uv+stepUV*3.0))*0.0540540541;
    c+=(sampleTex(tex,uv-stepUV*4.0)+sampleTex(tex,uv+stepUV*4.0))*0.0162162162;
    return c;
}

fragment float4 crtFragment(V v [[stage_in]], texture2d<float> src [[texture(0)]],
 texture2d<float> feedback [[texture(1)]],texture2d<float> blur [[texture(2)]],constant Params &p [[buffer(0)]]) {
 if(p.mode==1) return max(sampleTex(feedback,v.uv)*0.65,sampleTex(src,v.uv)*0.96);
 if(p.mode==2) return blur9(src,v.uv,float2(1.0/p.sourceSize.x,0));
 if(p.mode==3) return blur9(src,v.uv,float2(0,1.0/p.sourceSize.y));
 if(p.mode==4) return newpixie(src,blur,v.uv,p.outputSize,p.frame);
 if(p.mode==5) return sampleTex(src,v.uv);
 constexpr sampler nearestSampler(coord::normalized,address::clamp_to_edge,filter::nearest);
 return src.sample(nearestSampler,v.uv);
}
