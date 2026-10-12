// NewPixie CRT by Mattias Gustavsson (2016), Slang adaptation by hunterk.
// Native adaptation: same four passes and default parameters, except CURVATURE.
// The upstream shader overscans the picture and hides its edges under bezel
// artwork; without the bezel that overscan sampled outside the frame and smeared
// the edge pixels. Here the picture maps 1:1 to the screen and everything outside
// the curved tube is black.
// See licenses/NewPixie-MIT.txt. Input UV is top-to-bottom on both backends.
#define CURVATURE 1.0
float inside(vec2 tc) {
    vec2 a=step(vec2(0.0),tc)*step(tc,vec2(1.0));
    return a.x*a.y;
}
vec3 tsample(sampler2D tex, vec2 tc) {
    return pow(abs(texture2D(tex,vec2(tc.x,1.0-tc.y)).rgb),vec3(2.2))*1.25*inside(tc);
}
vec3 filmic(vec3 c) {
    vec3 x=max(vec3(0.0),c-0.004);
    return (x*(6.2*x+0.5))/(x*(6.2*x+1.7)+0.06);
}
vec2 curve(vec2 uv) {
    uv=(uv-0.5)*vec2(0.925,1.095)*CURVATURE;
    uv.x*=1.0+pow(abs(uv.y)/4.0,2.0);
    uv.y*=1.0+pow(abs(uv.x)/3.0,2.0);
    return uv/(vec2(0.925,1.095)*CURVATURE)+0.5;
}
float noise(vec2 co) { return fract(sin(dot(co,vec2(12.9898,78.233)))*43758.5453); }
vec4 newpixie(sampler2D backbuffer, sampler2D blurbuffer, vec2 inputUV, vec2 resolution, float frame) {
    vec2 uv=vec2(inputUV.x,1.0-inputUV.y);
    vec2 curved=mix(curve(uv),uv,0.4);
    vec2 scuv=curved;
    float x=sin(uv.y*resolution.y*1.5)/resolution.x*0.25;
    float time=mod(frame,640.0);
    vec3 col;
    col.r=tsample(backbuffer,vec2(x+scuv.x+0.0009,scuv.y+0.0009)).r+0.02;
    col.g=tsample(backbuffer,vec2(x+scuv.x,scuv.y-0.0011)).g+0.02;
    col.b=tsample(backbuffer,vec2(x+scuv.x-0.0015,scuv.y)).b+0.02;
    float i=clamp(dot(col,vec3(0.299,0.587,0.114)),0.0,1.0);
    i=i*i*0.85+0.15;
    vec3 r=tsample(blurbuffer,vec2(x-0.014,-0.027)*0.85+0.007*vec2(
        0.35*sin(1.0/7.0+15.0*curved.y+0.9*time),
        0.35*sin(2.0/7.0+10.0*curved.y+1.37*time))+scuv+vec2(0.001))*vec3(0.5,0.25,0.25);
    vec3 g=tsample(blurbuffer,vec2(x-0.019,-0.020)*0.85+0.007*vec2(
        0.35*cos(1.0/9.0+15.0*curved.y+0.5*time),
        0.35*sin(2.0/9.0+10.0*curved.y+1.50*time))+scuv+vec2(0.000,-0.002))*vec3(0.25,0.5,0.25);
    vec3 b=tsample(blurbuffer,vec2(x-0.017,-0.003)*0.85+0.007*vec2(
        0.35*sin(2.0/3.0+15.0*curved.y+0.7*time),
        0.35*cos(2.0/3.0+10.0*curved.y+1.63*time))+scuv+vec2(-0.002,0.000))*vec3(0.25,0.25,0.5);
    col+=0.15*(1.0-0.299)*pow(clamp(3.0*r,vec3(0),vec3(1)),vec3(2))*i;
    col+=0.15*(1.0-0.587)*pow(clamp(3.0*g,vec3(0),vec3(1)),vec3(2))*i;
    col+=0.15*(1.0-0.114)*pow(clamp(3.0*b,vec3(0),vec3(1)),vec3(2))*i;
    col*=vec3(0.95,1.05,0.95);
    col=clamp(col*1.3+0.75*col*col+1.25*col*col*col*col*col,vec3(0),vec3(10));
    float vig=0.01+16.0*curved.x*curved.y*(1.0-curved.x)*(1.0-curved.y);
    col*=1.3*sqrt(max(vig,0.0));
    col*=pow(clamp(0.35+0.18*sin(6.0*time-curved.y*resolution.y*1.5),0.0,1.0),0.9);
    col*=1.0-0.23*clamp(mod(uv.x*resolution.x,3.0)/2.0,0.0,1.0);
    col=filmic(col);
    vec2 seed=curved*resolution;
    col-=0.015*pow(vec3(noise(seed+time),noise(seed+time*2.0),noise(seed+time*3.0)),vec3(1.5));
    col*=1.0-0.004*(sin(50.0*time+curved.y*2.0)*0.5+0.5);
    // Antialiased tube edge, about one output pixel wide.
    vec2 edge=1.5/resolution;
    vec2 m=smoothstep(vec2(0.0),edge,curved)*smoothstep(vec2(0.0),edge,1.0-curved);
    return vec4(max(col,vec3(0.0))*m.x*m.y,1.0);
}
vec4 blur9(sampler2D tex,vec2 uv,vec2 stepUV) {
    vec4 c=texture2D(tex,uv)*0.2270270270;
    c+=(texture2D(tex,uv-stepUV)+texture2D(tex,uv+stepUV))*0.1945945946;
    c+=(texture2D(tex,uv-stepUV*2.0)+texture2D(tex,uv+stepUV*2.0))*0.1216216216;
    c+=(texture2D(tex,uv-stepUV*3.0)+texture2D(tex,uv+stepUV*3.0))*0.0540540541;
    c+=(texture2D(tex,uv-stepUV*4.0)+texture2D(tex,uv+stepUV*4.0))*0.0162162162;
    return c;
}
