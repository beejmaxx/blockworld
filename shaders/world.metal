#include <metal_stdlib>
using namespace metal;

struct Camera {
  float4x4 viewProjection;
  float4 eye;
  float4 selection;
  float4 forward;
  float4 right;
  float4 up;
  float4 screen; // width, height, tan(fov/2), time
  float4 sun; // direction, twilight
  float4 horizon;
  float4 zenith;
  float4 ambient; // rgb light tint/intensity, daylight blend
  float4 breaking; // block coordinates, damage progress
  float4 lights[8];
  float4 metropolis;
};
struct Input {
  float3 position [[attribute(0)]];
  float2 uv [[attribute(1)]];
  float material [[attribute(2)]];
  float light [[attribute(3)]];
  float3 block [[attribute(4)]];
};
struct Varying {
  float4 position [[position]];
  float3 world;
  float2 uv;
  float material [[flat]];
  float light;
  float3 block [[flat]];
};
vertex Varying worldVertex(Input in [[stage_in]], constant Camera& camera [[buffer(0)]]) {
  Varying out;
  out.position=camera.viewProjection*float4(in.position,1);
  out.world=in.position; out.uv=in.uv; out.material=in.material; out.light=in.light; out.block=in.block;
  return out;
}
float hash21(float2 p) {
  uint2 cell=uint2(int2(p));
  uint n=cell.x*1597334677u ^ cell.y*3812015801u;
  n=(n^(n>>16))*2246822519u; n^=n>>13;
  return float(n&65535u)/65535.0;
}
float waterNoise(float2 p) {
  float2 cell=floor(p),f=fract(p);
  f=f*f*(3.0-2.0*f);
  return mix(mix(hash21(cell),hash21(cell+float2(1,0)),f.x),
             mix(hash21(cell+float2(0,1)),hash21(cell+1),f.x),f.y);
}
float3 blockTexture(int material,float2 uv) {
  float2 p=floor(clamp(uv,0.0,0.9999)*16.0);
  float noise=hash21(p+float2(material*31,material*17));
  float detail=1.0-smoothstep(.3,1.0,max(fwidth(uv).x,fwidth(uv).y)*16.0);
  noise=.5+(noise-.5)*detail;
  float3 color;
  switch(material) {
    case 1: color=float3(.40,.59,.25); break;
    case 2: color=float3(.48,.32,.20); break;
    case 3: color=float3(.53,.55,.55); break;
    case 4: color=float3(.83,.77,.53); break;
    case 5: color=float3(.44,.29,.15); break;
    case 6: color=float3(.29,.47,.20); break;
    case 7: color=float3(.69,.49,.28); break;
    case 8: color=float3(.65,.30,.21); break;
    case 9: color=float3(.22,.23,.24); break;
    case 10: color=p.y>12.0-floor(hash21(float2(p.x,2))*3.0) ? float3(.40,.58,.24) : float3(.48,.32,.20); break;
    case 12: color=float3(.65,.83,.87); break;
    case 13: color=float3(.45,.29,.15); break;
    case 14: color=float3(1.0,.69,.16); break;
    case 15: case 16: color=float3(.64,.43,.23); break;
    case 17: color=float3(.71,.25,.21); break;
    case 18: color=float3(.95,.91,.77); break;
    case 19: color=float3(.53,.34,.18); break;
    case 21: color=float3(.75,.56,.32); break;
    case 24: color=float3(.37,.65,.19); break;
    case 25: color=float3(.92,.72,.25); break;
    case 26: color=float3(.97,.94,.83); break;
    case 27: color=float3(.99,.59,.14); break;
    case 28: color=float3(.85,.17,.12); break;
    case 29: color=float3(.055,.065,.05); break;
    case 30: color=float3(.80,.54,.28); break;
    case 31: color=float3(1.0,.94,.75); break;
    case 32: color=float3(1.0,.36,.47); break;
    case 33: color=float3(.27,.18,.12); break;
    case 34: color=float3(.97,.48,.10); break;
    case 35: color=float3(.88,.16,.26); break;
    case 36: color=float3(.96,.45,.08); break;
    case 37: color=float3(.38,.79,1.0); break;
    case 38: color=float3(1.0,.95,.81); break;
    case 39: color=float3(.29,.76,.24); break;
    case 40: color=float3(.37,.24,.14); break;
    case 41: color=float3(.23,.15,.10); break;
    case 42: color=float3(.36,.69,.74); break;
    case 60: color=float3(.88,.89,.84); break;
    case 61: color=float3(.78,.72,.55); break;
    case 62: color=float3(.72,.34,.22); break;
    case 63: color=float3(.34,.53,.40); break;
    case 64: color=float3(.20,.24,.26); break;
    case 65: color=float3(.16,.19,.21); break;
    case 66: color=float3(.15,.52,.63); break;
    case 67: color=float3(1.0,.80,.43); break;
    case 68: color=float3(.27,.54,.65); break;
    case 69: color=float3(.20,.58,.69); break;
    case 70: color=float3(.83,.22,.19); break;
    case 71: color=float3(.96,.71,.20); break;
    case 72: color=float3(.36,.23,.14); break;
    case 73: color=float3(.85,.82,.73); break;
    case 74: color=float3(.83,.055,.035); break; // GT2 red paint.
    case 75: color=float3(.045,.052,.060); break;
    case 76: color=float3(.10,.20,.25); break;
    case 77: color=float3(.67,.71,.73); break;
    case 78: color=float3(.92,.98,1.0); break;
    case 79: color=float3(1.0,.045,.018); break;
    case 80: color=float3(.83,.055,.035); break;
    case 81: color=float3(.06,.48,.85); break;
    case 82: color=float3(.09,.10,.13); break;
    case 83: color=float3(.85,.89,.91); break;
    case 84: color=float3(1,.65,.04); break;
    case 85: color=float3(.12,.35,.23); break;
    case 86: color=float3(.82,.04,.12); break;
    case 87: color=float3(.61,.68,.72); break;
    case 88: color=float3(.09,.16,.72); break;
    case 89: color=float3(.47,.83,.10); break;
    case 90: color=float3(.65,.29,.13); break;
    case 91: color=float3(.45,.17,.69); break;
    case 92: color=float3(.90,.87,.78); break;
    case 93: color=float3(.67,.52,.30); break;
    case 94: color=float3(.30,.79,.85); break;
    case 95: color=float3(.15,.18,.25); break;
    case 96: color=float3(.94,.30,.06); break;
    case 97: color=float3(.05,.48,.30); break;
    case 98: color=float3(.58,.04,.13); break;
    case 99: color=float3(.73,.77,.80); break;
    case 100: color=float3(.82,.84,.80); break;
    case 101: color=float3(.30,.50,.56); break;
    case 102: color=float3(.69,.61,.45); break;
    case 103: color=float3(.65,.33,.23); break;
    case 104: color=float3(.34,.45,.40); break;
    case 105: color=float3(.76,.53,.36); break;
    case 106: color=float3(.94,.74,.59); break;
    case 107: color=float3(.42,.26,.18); break;
    default: {
      float ring=fmod(floor(max(abs(p.x-7.5),abs(p.y-7.5))),3.0);
      color=ring==0.0 ? float3(.45,.30,.16) : float3(.70,.52,.30); break;
    }
  }
  if(material==5) color*=.76+.30*hash21(float2(p.x,floor(p.y/5.0)));
  if(material==7) {
    noise=.5+(noise-.5)*.25;
    if(fmod(p.y,4.0)==0.0 || (fmod(p.x+floor(p.y/4.0)*7.0,16.0)==0.0)) color*=mix(1.0,.84,detail);
  }
  if(material==8) {
    if(fmod(p.y,5.0)==0.0 || fmod(p.x+floor(p.y/5.0)*4.0,8.0)==0.0) color=float3(.68,.64,.54);
  }
  if(material==3 && noise>.84) color*=.82;
  if(material>=60 && material<=64) {
    noise=.5+(noise-.5)*.25;
    if((material==61 || material==62) && (p.y==0 || (p.x==0 && fmod(p.y,8.0)<1.0))) color*=.88;
  }
  if(material==72) color*=.92+.12*sin(p.x*.8+sin(p.y*.18));
  if(material==73) noise=.5+(noise-.5)*.16;
  if(material>=74 && material<=107) noise=.5;
  if(material==67 && (p.x<1 || p.x>14 || p.y<1 || p.y>14)) color*=.28;
  if(material==35 && fmod(p.x+floor(p.y/4.0)*2.0,5.0)==0.0 && fmod(p.y,4.0)==1.0) color=float3(1.0,.81,.35);
  if(material==36 && fmod(p.x,4.0)==0.0) color*=.72;
  if((material==40 || material==41) && fmod(p.x,4.0)<1.0) color*=.65;
  if(material==6) color*=mix(1.0,noise>.55 ? 1.1 : .9,detail);
  if(material==15 || material==16) {
    if(p.x<1 || p.x>14 || p.y<1 || p.y>14 || (p.x>6 && p.x<9)) color*=.66;
    if(material==15 && p.x>12 && p.y>9 && p.y<12) color=float3(.88,.73,.36);
    if(material==16 && p.x>2 && p.x<13 && p.y>4 && p.y<13) color=float3(.26,.32,.28);
  }
  if(material==17) {
    if(fmod(p.x,4.0)==0.0 || fmod(p.y,4.0)==0.0) color*=.86;
    if(p.x<1 || p.x>14 || p.y<1 || p.y>14) color=float3(.85,.65,.40);
  }
  if(material==19 && (p.x<2 || p.x>13 || p.y<3 || p.y>13)) color*=.55;
  if(material==21) {
    if(p.x<1 || p.x>14 || p.y<1 || p.y>14) color*=.55;
    if((fmod(p.x,5.0)==0 || fmod(p.y,5.0)==0) && p.x>2 && p.x<13 && p.y>2 && p.y<13) color*=.56;
  }
  return color*(.87+noise*.26);
}
fragment float4 worldFragment(Varying in [[stage_in]],constant Camera& camera [[buffer(0)]]) {
  int material=int(in.material+.5);
  float edge=min(min(in.uv.x,in.uv.y),min(1-in.uv.x,1-in.uv.y));
  if(material==20) {
    float pulse=.76+.17*sin(camera.screen.w*3.0);
    return float4(float3(1.0,.80,.37)*pulse,edge<.025 ? .85 : .035);
  }
  if(material==22 || material==23) {
    float3 tint=material==22 ? float3(.45,1.0,.65) : float3(1.0,.32,.24);
    return float4(tint,edge<.03 ? .85 : .13);
  }
  float opacity=1.0;
  if(material==12) {
    float aa=max(fwidth(edge),.002);
    float frame=1.0-smoothstep(.014-aa,.014+aa,edge);
    opacity=mix(.10,.72,frame);
  }
  float3 base=blockTexture(material,in.uv);
  if(material==108) {
    float2 p=in.world.xz-camera.metropolis.xy;
    float2 d=abs(fract((p+32.0)/64.0)*64.0-32.0);
    base=min(d.x,d.y)<=5.5 ? float3(.16,.19,.21) : min(d.x,d.y)<=8.5 ? float3(.78,.72,.55) : float3(.40,.59,.25);
    if((d.x<.5 && fract(p.y/12.0)<.5) || (d.y<.5 && fract(p.x/12.0)<.5))base=float3(.88,.89,.84);
  }
  float3 color=base*in.light*camera.ambient.rgb;
  if(material>=100 && material<=104) {
    float vertical=fract((in.world.x+in.world.z)/5.0),level=fract((in.world.y-23.0)/5.0);
    bool window=vertical>.15 && vertical<.88 && level>.24 && level<.90;
    if(window) {
      float light=hash21(floor(in.world.xz/5.0)+floor(in.world.y/5.0));
      color=mix(float3(.15,.28,.34)*camera.ambient.rgb,camera.horizon.rgb,.38);
      if(light>.38)color=mix(color,float3(.96,.75,.43),(1-camera.ambient.w)*.85);
    }
  }
  if(material==66) {
    float depth=max(1.0,in.light);
    float2 waterPosition=in.world.xz;
    float time=camera.screen.w;
    float rippleFade=1.0/(1.0+length(fwidth(waterPosition))*.8);
    float2 wave=(float2(waterNoise(waterPosition*.8+float2(time*.13,0)),
                             waterNoise(waterPosition*.7+float2(29,-time*.11)))-.5)*.055*rippleFade;
    wave+=float2(sin(dot(waterPosition,float2(.43,.19))+time*.65),
                 sin(dot(waterPosition,float2(-.17,.51))-time*.48))*.008;
    float3 normal=normalize(float3(wave.x,1,wave.y));
    float3 view=normalize(camera.eye.xyz-in.world);
    float fresnel=.10+.60*pow(1.0-saturate(dot(normal,view)),4.0);
    float3 reflection=mix(camera.horizon.rgb,camera.zenith.rgb,.3);
    float spec=pow(saturate(dot(reflect(-camera.sun.xyz,normal),view)),140.0);
    float3 water=mix(float3(.23,.62,.61),float3(.035,.15,.25),smoothstep(1.0,10.0,depth));
    float caustic=pow(saturate(1.0-abs(sin(in.world.x*3.1+wave.x*8)*cos(in.world.z*3.7+wave.y*8))*3.0),8.0);
    water+=float3(.018,.028,.02)*caustic*(1-smoothstep(2.0,5.0,depth));
    color=mix(water*camera.ambient.rgb,reflection,fresnel);
    color+=float3(1.0,.88,.65)*spec*camera.ambient.w*.3;
  }
  if(material==12) {
    float3 n=normalize(cross(dfdx(in.world),dfdy(in.world)));
    float3 view=normalize(camera.eye.xyz-in.world);
    float fresnel=pow(1-abs(dot(n,view)),4.0);
    color=mix(color,camera.horizon.rgb,.45+.3*fresnel);
    opacity=min(.85,opacity+fresnel*.12);
  }
  if(material==68) {
    // Opaque curtain-wall panels suggest sky reflection; real Glass is used
    // at lobbies and viewing decks so the player can see through them.
    float reflection=.20+.18*saturate(in.world.y/64.0);
    color=mix(color,camera.horizon.rgb,reflection);
    if(edge<.018 || abs(in.uv.x-.5)<.008) color*=.65;
    float night=1.0-camera.ambient.w;
    if(hash21(in.block.xz+in.block.y*13)>.58 && edge>.12)
      color=mix(color,float3(.94,.71,.38),night*.85);
  }
  if(material>=69 && material<=71 && edge<.014) color*=.85;
  if(material==67) color=base*.95;
  if(material==37) color=base*.95;
  if((material>=74 && material<=77) || (material>=80 && material<=99)) {
    float3 n=normalize(cross(dfdx(in.world),dfdy(in.world)));
    float3 view=normalize(camera.eye.xyz-in.world);
    if(dot(n,view)<0) n=-n;
    float fresnel=pow(1-saturate(dot(n,view)),3.0);
    color=mix(color,camera.horizon.rgb,fresnel*(material==76 ? .65 : .18));
    color+=pow(saturate(dot(reflect(-camera.sun.xyz,n),view)),56.0)*camera.ambient.w*(material==75 ? .12 : .5);
  }
  if(material==78 || material==79) color=base;
  for(int i=0;i<8;++i) if(camera.lights[i].w>0) {
    float falloff=saturate(1.0-length(in.world-camera.lights[i].xyz)/camera.lights[i].w);
    color+=base*float3(1.35,.73,.26)*falloff*falloff*mix(1.0,.18,camera.ambient.w);
  }
  if(material==14) color=float3(1.0,.62+.14*sin(camera.screen.w*9+in.world.x),.13);
  if(camera.selection.w>0.5 && all(abs(in.block-camera.selection.xyz)<.1)) {
    if(edge<.025) color=float3(.10,.13,.10);
    else color=mix(color,float3(1.0,.95,.75),.08);
  }
  if(camera.breaking.w>0 && all(abs(in.block-camera.breaking.xyz)<.1)) {
    float progress=camera.breaking.w;
    float2 p=floor(in.uv*24.0)/24.0;
    float jag=(hash21(float2(floor(p.y*12),8))-.5)*.16;
    bool crack=abs(p.x-.48+jag)<.028 && abs(p.y-.5)<progress*.72;
    crack=crack || (progress>.28 && abs(p.y-.18-p.x*.67)<.032 && p.x<progress);
    crack=crack || (progress>.55 && abs(p.y-.87+p.x*.54)<.025 && p.x>1-progress);
    if(crack) color*=.16;
    color=mix(color,float3(1.0,.89,.58),progress*.05);
  }
  float distance=length(in.world.xz-camera.eye.xz);
  float fog=camera.eye.w>1.5 ? smoothstep(120.0,205.0,distance) : camera.eye.w>.5 ? smoothstep(78.0,138.0,distance) : smoothstep(52.0,91.0,distance);
  bool downtown=camera.metropolis.z>0 && all(in.world.xz>=camera.metropolis.xy-8) && all(in.world.xz<camera.metropolis.xy+camera.metropolis.zw+8);
  if(camera.metropolis.z>0 || in.block.x== -100006 || in.block.x== -100008 || downtown)fog=smoothstep(450.0,850.0,distance);
  float3 fogColor=camera.horizon.rgb;
  return float4(mix(color,fogColor,fog),opacity);
}

struct SkyVarying { float4 position [[position]]; float2 uv; };
vertex SkyVarying skyVertex(uint id [[vertex_id]]) {
  float2 p=float2(id==1 ? 3.0 : -1.0,id==2 ? 3.0 : -1.0);
  SkyVarying out; out.position=float4(p,.99999,1); out.uv=p; return out;
}
fragment float4 skyFragment(SkyVarying in [[stage_in]],constant Camera& camera [[buffer(0)]]) {
  float3 dir=normalize(camera.forward.xyz+camera.right.xyz*in.uv.x*camera.screen.x/camera.screen.y*camera.screen.z
                      +camera.up.xyz*in.uv.y*camera.screen.z);
  float3 color=mix(camera.horizon.rgb,camera.zenith.rgb,pow(saturate(dir.y),.55));
  float daylight=camera.ambient.w;
  float night=1.0-daylight;
  // Stable spherical coordinates keep the stars fixed as the player looks around.
  float2 starUV=float2(atan2(dir.z,dir.x)/6.2831853+.5,asin(clamp(dir.y,-1.0,1.0))/3.1415927+.5)*float2(512,256);
  float2 starCell=floor(starUV),starLocal=abs(fract(starUV)-.5);
  float starSeed=hash21(starCell);
  if(starSeed>.994 && dir.y>.04) {
    float size=.10+.18*hash21(starCell+73.0);
    float aa=max(max(fwidth(starUV.x),fwidth(starUV.y)),.035);
    float star=1.0-smoothstep(size,size+aa,max(starLocal.x,starLocal.y));
    float twinkle=.72+.28*sin(camera.screen.w*(.65+starSeed)+hash21(starCell+9)*80);
    color+=float3(.77,.83,1.0)*star*twinkle*night*night*smoothstep(.04,.3,dir.y);
  }
  float3 sun=camera.sun.xyz;
  float sunDot=dot(dir,sun);
  color+=mix(float3(.20,.14,.055),float3(.50,.16,.045),camera.sun.w)*pow(saturate(sunDot),36.0)*smoothstep(-.1,.04,sun.y);
  float3 sunRight=normalize(cross(sun,float3(0,1,0))), sunUp=cross(sunRight,sun);
  if(sunDot>.97 && dir.y>-.025 && abs(dot(dir,sunRight))<.037 && abs(dot(dir,sunUp))<.037)
    color=mix(float3(1.0,.94,.73),float3(1.0,.56,.24),camera.sun.w*.7);
  float moonDot=dot(dir,-sun);
  float2 moonUV=float2(dot(dir,sunRight),dot(dir,sunUp));
  color+=float3(.045,.06,.12)*pow(saturate(moonDot),60.0)*night;
  if(moonDot>.98 && dir.y>0 && all(abs(moonUV)<.028)) {
    float crater=hash21(floor(moonUV*230.0));
    color=float3(.73,.79,.89)*(crater>.70 ? .73 : 1.0);
  }
  if(dir.y>.055) {
    float2 cloud=(camera.eye.xz+dir.xz*(170.0-camera.eye.y)/dir.y)/19.0;
    cloud.x+=camera.screen.w*.013;
    float2 cell=floor(cloud);
    float cover=hash21(floor(cell/float2(3,2)));
    float mask=cover>.64 && hash21(cell+4.0)>.20 ? 1.0 : 0.0;
    float fade=smoothstep(.055,.22,dir.y)*.82;
    float3 cloudColor=mix(float3(.08,.105,.18),float3(.94,.95,.88),daylight);
    cloudColor=mix(cloudColor,float3(.80,.50,.41),camera.sun.w*.65);
    color=mix(color,cloudColor,mask*fade);
  }
  return float4(color,1);
}
