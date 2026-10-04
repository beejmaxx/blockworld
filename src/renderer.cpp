#include "renderer.hpp"
#include "ui_font.hpp"
#include "ranch.hpp"
#include "city.hpp"
#include "coast.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace bw {
namespace {
[[noreturn]] void fail(const char* operation) { throw std::runtime_error(std::string(operation)+": "+SDL_GetError()); }
struct alignas(16) Camera {
  glm::mat4 viewProjection;
  glm::vec4 eye,selection,forward,right,up,screen;
  glm::vec4 sun,horizon,zenith,ambient;
  glm::vec4 breaking;
  std::array<glm::vec4,8> lights{};
};
static_assert(sizeof(Camera)==368);
bool visible(ChunkPos p,const glm::mat4& m) {
  glm::vec3 center{p.x*chunkSize+8.f,worldHeight*.5f,p.z*chunkSize+8.f},extent{8.f,worldHeight*.5f,8.f};
  glm::vec4 rows[4];
  for(int i=0;i<4;++i) rows[i]={m[0][i],m[1][i],m[2][i],m[3][i]};
  for(auto plane : {rows[3]+rows[0],rows[3]-rows[0],rows[3]+rows[1],rows[3]-rows[1],rows[2],rows[3]-rows[2]}) {
    if(glm::dot(glm::vec3(plane),center)+plane.w+glm::dot(glm::abs(glm::vec3(plane)),extent)<0) return false;
  }
  return true;
}
}
Renderer::Renderer(SDL_Window* window) : window_(window) {
  try {
    device_=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL,true,"metal");
    if(!device_) fail("Create Metal device");
    if(!SDL_ClaimWindowForGPUDevice(device_,window_)) fail("Claim window");
    if(!SDL_SetGPUSwapchainParameters(device_,window_,SDL_GPU_SWAPCHAINCOMPOSITION_SDR,SDL_GPU_PRESENTMODE_VSYNC)) fail("Set swapchain");
    format_=SDL_GetGPUSwapchainTextureFormat(device_,window_);
    worldPipeline_=pipeline("world.metal","worldVertex","worldFragment",0);
    glassPipeline_=pipeline("world.metal","worldVertex","worldFragment",5);
    skyPipeline_=pipeline("world.metal","skyVertex","skyFragment",1);
    uiPipeline_=pipeline("ui.metal","uiVertex","uiFragment",2);
    previewPipeline_=pipeline("world.metal","worldVertex","worldFragment",3);
    placementPipeline_=pipeline("world.metal","worldVertex","worldFragment",4);
    SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_VERTEX,72*sizeof(Vertex),0};
    previewBuffer_=SDL_CreateGPUBuffer(device_,&bi);
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,72*sizeof(Vertex),0};
    previewTransfer_=SDL_CreateGPUTransferBuffer(device_,&ti);
    if(!previewBuffer_ || !previewTransfer_) fail("Create building guide buffers");
    bi.size=Uint32(DebrisCloud::capacity*6*sizeof(Vertex)); ti.size=bi.size;
    debrisBuffer_=SDL_CreateGPUBuffer(device_,&bi); debrisTransfer_=SDL_CreateGPUTransferBuffer(device_,&ti);
    if(!debrisBuffer_ || !debrisTransfer_) fail("Create debris buffers");
    bi.size=Uint32((chickenVertexLimit+ranchVertexLimit)*sizeof(Vertex)); ti.size=bi.size;
    chickenBuffer_=SDL_CreateGPUBuffer(device_,&bi); chickenTransfer_=SDL_CreateGPUTransferBuffer(device_,&ti);
    if(!chickenBuffer_ || !chickenTransfer_) fail("Create chicken buffers");
    const auto& font=readableFont();
    SDL_GPUTextureCreateInfo fi{}; fi.type=SDL_GPU_TEXTURETYPE_2D; fi.format=SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    fi.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER; fi.width=FontAtlas::width; fi.height=FontAtlas::height;
    fi.layer_count_or_depth=1; fi.num_levels=1; fi.sample_count=SDL_GPU_SAMPLECOUNT_1;
    fontTexture_=SDL_CreateGPUTexture(device_,&fi);
    SDL_GPUSamplerCreateInfo si{}; si.min_filter=si.mag_filter=SDL_GPU_FILTER_LINEAR;
    si.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    si.address_mode_u=si.address_mode_v=si.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    fontSampler_=SDL_CreateGPUSampler(device_,&si);
    if(!fontTexture_ || !fontSampler_) fail("Create interface font texture");
    ti.size=Uint32(font.pixels.size());
    auto upload=SDL_CreateGPUTransferBuffer(device_,&ti);
    if(!upload) fail("Create font upload");
    auto pixels=SDL_MapGPUTransferBuffer(device_,upload,false);
    if(!pixels) { SDL_ReleaseGPUTransferBuffer(device_,upload); fail("Map font upload"); }
    std::memcpy(pixels,font.pixels.data(),font.pixels.size()); SDL_UnmapGPUTransferBuffer(device_,upload);
    auto command=SDL_AcquireGPUCommandBuffer(device_);
    if(!command) { SDL_ReleaseGPUTransferBuffer(device_,upload); fail("Create font upload command"); }
    auto copy=SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureTransferInfo source{upload,0,FontAtlas::width,FontAtlas::height};
    SDL_GPUTextureRegion destination{}; destination.texture=fontTexture_; destination.w=FontAtlas::width; destination.h=FontAtlas::height; destination.d=1;
    SDL_UploadToGPUTexture(copy,&source,&destination,false); SDL_EndGPUCopyPass(copy);
    bool submitted=SDL_SubmitGPUCommandBuffer(command); SDL_ReleaseGPUTransferBuffer(device_,upload);
    if(!submitted) fail("Upload interface font");
    std::cout<<"Renderer: "<<SDL_GetGPUDeviceDriver(device_)<<"\n";
  } catch(...) { cleanup(); throw; }
}
Renderer::~Renderer() { cleanup(); }
void Renderer::cleanup() noexcept {
  if(!device_) return;
  SDL_WaitForGPUIdle(device_);
  for(auto& [p,m] : meshes_) if(m.buffer) SDL_ReleaseGPUBuffer(device_,m.buffer);
  if(uiBuffer_) SDL_ReleaseGPUBuffer(device_,uiBuffer_);
  if(uiTransfer_) SDL_ReleaseGPUTransferBuffer(device_,uiTransfer_);
  if(previewBuffer_) SDL_ReleaseGPUBuffer(device_,previewBuffer_);
  if(previewTransfer_) SDL_ReleaseGPUTransferBuffer(device_,previewTransfer_);
  if(debrisBuffer_) SDL_ReleaseGPUBuffer(device_,debrisBuffer_);
  if(debrisTransfer_) SDL_ReleaseGPUTransferBuffer(device_,debrisTransfer_);
  if(chickenBuffer_) SDL_ReleaseGPUBuffer(device_,chickenBuffer_);
  if(chickenTransfer_) SDL_ReleaseGPUTransferBuffer(device_,chickenTransfer_);
  if(color_) SDL_ReleaseGPUTexture(device_,color_);
  if(depth_) SDL_ReleaseGPUTexture(device_,depth_);
  if(fontTexture_) SDL_ReleaseGPUTexture(device_,fontTexture_);
  if(fontSampler_) SDL_ReleaseGPUSampler(device_,fontSampler_);
  for(auto p : {worldPipeline_,glassPipeline_,skyPipeline_,uiPipeline_,previewPipeline_,placementPipeline_}) if(p) SDL_ReleaseGPUGraphicsPipeline(device_,p);
  SDL_ReleaseWindowFromGPUDevice(device_,window_);
  SDL_DestroyGPUDevice(device_); device_=nullptr;
}
SDL_GPUShader* Renderer::shader(const char* file,const char* entry,SDL_GPUShaderStage stage,Uint32 uniforms,Uint32 samplers) {
  auto path=std::filesystem::path(SDL_GetBasePath())/"shaders"/file;
  std::ifstream input(path);
  if(!input) throw std::runtime_error("Cannot read shader: "+path.string());
  std::string source{std::istreambuf_iterator<char>(input),{}};
  SDL_GPUShaderCreateInfo info{};
  info.code_size=source.size()+1; info.code=reinterpret_cast<const Uint8*>(source.c_str());
  info.entrypoint=entry; info.format=SDL_GPU_SHADERFORMAT_MSL; info.stage=stage; info.num_uniform_buffers=uniforms;
  info.num_samplers=samplers;
  auto result=SDL_CreateGPUShader(device_,&info);
  if(!result) fail(entry);
  return result;
}
SDL_GPUGraphicsPipeline* Renderer::pipeline(const char* file,const char* vs,const char* fs,int kind) {
  auto vertex=shader(file,vs,SDL_GPU_SHADERSTAGE_VERTEX,kind==1 ? 0 : 1);
  SDL_GPUShader* fragment{};
  try { fragment=shader(file,fs,SDL_GPU_SHADERSTAGE_FRAGMENT,kind==2 ? 0 : 1,kind==2 ? 1 : 0); }
  catch(...) { SDL_ReleaseGPUShader(device_,vertex); throw; }
  SDL_GPUVertexBufferDescription buffer{0,Uint32(kind==2 ? sizeof(UiVertex) : sizeof(Vertex)),SDL_GPU_VERTEXINPUTRATE_VERTEX,0};
  const SDL_GPUVertexAttribute worldAttributes[]{
    {0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(Vertex,position)},
    {1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(Vertex,uv)},
    {2,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,offsetof(Vertex,material)},
    {3,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,offsetof(Vertex,light)},
    {4,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(Vertex,block)}};
  const SDL_GPUVertexAttribute uiAttributes[]{
    {0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(UiVertex,position)},
    {1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,offsetof(UiVertex,color)},
    {2,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(UiVertex,uv)}};
  SDL_GPUColorTargetDescription target{}; target.format=format_;
  if(kind==2 || kind==3 || kind==4 || kind==5) {
    target.blend_state.enable_blend=true;
    target.blend_state.src_color_blendfactor=SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op=SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
  }
  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader=vertex; info.fragment_shader=fragment; info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  if(kind!=1) info.vertex_input_state={&buffer,1,kind==2 ? uiAttributes : worldAttributes,Uint32(kind==2 ? 3 : 5)};
  info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
  info.rasterizer_state.cull_mode=(kind==0 || kind==5) ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.front_face=SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  info.rasterizer_state.enable_depth_clip=true;
  info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
  info.depth_stencil_state.compare_op=SDL_GPU_COMPAREOP_LESS;
  info.depth_stencil_state.enable_depth_test=kind==0 || kind==4 || kind==5;
  info.depth_stencil_state.enable_depth_write=kind==0;
  info.target_info.color_target_descriptions=&target; info.target_info.num_color_targets=1;
  info.target_info.has_depth_stencil_target=true; info.target_info.depth_stencil_format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  auto result=SDL_CreateGPUGraphicsPipeline(device_,&info);
  SDL_ReleaseGPUShader(device_,vertex); SDL_ReleaseGPUShader(device_,fragment);
  if(!result) fail("Create graphics pipeline");
  return result;
}
void Renderer::targets(Uint32 width,Uint32 height) {
  if(width==width_ && height==height_) return;
  if(color_) { SDL_ReleaseGPUTexture(device_,color_); color_=nullptr; }
  if(depth_) { SDL_ReleaseGPUTexture(device_,depth_); depth_=nullptr; }
  SDL_GPUTextureCreateInfo info{};
  info.type=SDL_GPU_TEXTURETYPE_2D; info.format=format_; info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width=width; info.height=height; info.layer_count_or_depth=1; info.num_levels=1; info.sample_count=SDL_GPU_SAMPLECOUNT_1;
  color_=SDL_CreateGPUTexture(device_,&info); if(!color_) fail("Create color target");
  info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT; info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  depth_=SDL_CreateGPUTexture(device_,&info); if(!depth_) fail("Create depth target");
  width_=width; height_=height;
}
void Renderer::sync(World& world,ChunkPos center,int budget) {
  for(auto it=meshes_.begin();it!=meshes_.end();) {
    if(!world.chunks.contains(it->first)) {
      if(it->second.buffer) SDL_ReleaseGPUBuffer(device_,it->second.buffer);
      it=meshes_.erase(it);
    } else ++it;
  }
  std::vector<Chunk*> dirty;
  for(auto& [p,c] : world.chunks) if(c.dirty) dirty.push_back(&c);
  std::ranges::sort(dirty,[&](auto* a,auto* b) {
    auto distance=[&](ChunkPos p){return (p.x-center.x)*(p.x-center.x)+(p.z-center.z)*(p.z-center.z);};
    return distance(a->pos)<distance(b->pos);
  });
  if(dirty.empty()) return;
  auto* command=SDL_AcquireGPUCommandBuffer(device_); if(!command) fail("Acquire upload command");
  auto* copy=SDL_BeginGPUCopyPass(command);
  int done=0;
  for(auto* chunk : dirty) {
    if(done++>=budget) break;
    auto vertices=buildMesh(world,*chunk);
    // Draw the opaque world first. Only window frames need alpha blending;
    // enabling it for terrain and every tower wall wastes tile bandwidth.
    auto glass=std::stable_partition(vertices.begin(),vertices.end(),[](const Vertex& v){return v.material!=12.f;});
    Mesh replacement{}; replacement.vertices=Uint32(vertices.size());
    replacement.opaqueVertices=Uint32(glass-vertices.begin());
    for(int y=1;y<worldHeight;++y) for(int z=0;z<chunkSize;++z) for(int x=0;x<chunkSize;++x)
      if(chunk->get(x,y,z)==Block::Torch || chunk->get(x,y,z)==Block::Lamp)
        replacement.lights.emplace_back(chunk->pos.x*chunkSize+x+.5f,y+.68f,chunk->pos.z*chunkSize+z+.5f);
    if(!vertices.empty()) {
      Uint32 bytes=Uint32(vertices.size()*sizeof(Vertex));
      SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_VERTEX,bytes,0};
      replacement.buffer=SDL_CreateGPUBuffer(device_,&bi); if(!replacement.buffer) fail("Create mesh buffer");
      SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};
      auto* transfer=SDL_CreateGPUTransferBuffer(device_,&ti); if(!transfer) fail("Create mesh transfer");
      auto* data=SDL_MapGPUTransferBuffer(device_,transfer,false); if(!data) fail("Map mesh transfer");
      std::memcpy(data,vertices.data(),bytes); SDL_UnmapGPUTransferBuffer(device_,transfer);
      SDL_GPUTransferBufferLocation source{transfer,0}; SDL_GPUBufferRegion target{replacement.buffer,0,bytes};
      SDL_UploadToGPUBuffer(copy,&source,&target,false); SDL_ReleaseGPUTransferBuffer(device_,transfer);
    }
    auto& old=meshes_[chunk->pos];
    if(old.buffer) SDL_ReleaseGPUBuffer(device_,old.buffer);
    old=replacement; chunk->dirty=false;
  }
  SDL_EndGPUCopyPass(copy);
  if(!SDL_SubmitGPUCommandBuffer(command)) fail("Submit mesh upload");
}
std::size_t Renderer::triangleCount() const {
  std::size_t count{}; for(const auto& [p,m] : meshes_) count+=m.vertices/3; return count;
}
void Renderer::draw(const Player& player,const std::optional<RayHit>& hit,HudState hud,float time,const DebrisCloud& debris,const World& world,const std::filesystem::path& screenshot) {
  auto* command=SDL_AcquireGPUCommandBuffer(device_); if(!command) fail("Acquire frame");
  SDL_GPUTexture* swapchain{}; Uint32 w{},h{};
  if(!SDL_WaitAndAcquireGPUSwapchainTexture(command,window_,&swapchain,&w,&h)) fail("Acquire swapchain");
  if(!swapchain) { SDL_CancelGPUCommandBuffer(command); return; }
  targets(w,h);
  Camera camera{};
  auto eye=player.eye(),forward=player.direction(),right=glm::normalize(glm::cross(forward,glm::vec3(0,1,0))),up=glm::cross(right,forward);
  bool city=atCity(world,player),coast=atCoast(world,player);
  camera.viewProjection=glm::perspective(glm::radians(73.f),float(w)/float(h),.06f,coast ? 260.f : city ? 160.f : 115.f)*glm::lookAt(eye,eye+forward,glm::vec3(0,1,0));
  camera.eye=glm::vec4(eye,coast ? 2.f : city ? 1.f : 0.f); camera.forward=glm::vec4(forward,0); camera.right=glm::vec4(right,0); camera.up=glm::vec4(up,0);
  camera.screen={float(w),float(h),std::tan(glm::radians(73.f)*.5f),time};
  auto sky=hud.clock.sky();
  if(world.farm.garden.raining()) {
    sky.horizon=glm::mix(sky.horizon,glm::vec3(.40f,.49f,.54f)*(.3f+.7f*sky.daylight),.55f);
    sky.zenith=glm::mix(sky.zenith,glm::vec3(.32f,.42f,.49f)*(.3f+.7f*sky.daylight),.65f);
    sky.ambient*=.88f;
  }
  camera.sun=glm::vec4(sky.sun,sky.twilight); camera.horizon=glm::vec4(sky.horizon,0);
  camera.zenith=glm::vec4(sky.zenith,0); camera.ambient=glm::vec4(sky.ambient,sky.daylight);
  if(hit && !hud.hidden) camera.selection={float(hit->block.x),float(hit->block.y),float(hit->block.z),1};
  if(hud.breaking) camera.breaking={float(hud.breaking->x),float(hud.breaking->y),float(hud.breaking->z),hud.breakProgress};
  std::vector<glm::vec3> lights;
  for(const auto& [pos,mesh] : meshes_) for(auto light : mesh.lights) lights.push_back(light);
  std::ranges::sort(lights,[&](auto a,auto b){return glm::length(a-eye)<glm::length(b-eye);});
  for(std::size_t i=0;i<std::min(lights.size(),camera.lights.size());++i) camera.lights[i]=glm::vec4(lights[i],7.f);
  if(hud.guide.destination) {
    glm::vec4 clip=camera.viewProjection*glm::vec4(*hud.guide.destination,1);
    hud.waypointDistance=glm::length(*hud.guide.destination-eye);
    if(clip.w>.1f) hud.waypoint=glm::vec2((clip.x/clip.w*.5f+.5f)*float(w),(.5f-clip.y/clip.w*.5f)*float(h));
  }
  hud.animalLabels.clear();
  if(!hud.paused && !hud.menuOpen() && !hud.sleeping) {
    for(std::size_t i=0;i<world.farm.chickens.size();++i) {
      const auto& c=world.farm.chickens[i];
      if(!world.chunks.contains(chunkAt(int(std::floor(c.position.x)),int(std::floor(c.position.z))))) continue;
      auto point=c.position+glm::vec3(0,chickenScale(c)*1.1f,0),delta=point-eye;
      float distance=glm::length(delta);
      if(distance>12 || distance<.1f) continue;
      if(auto wall=world.raycast(eye,delta,distance); wall && wall->distance<distance-.1f) continue;
      auto clip=camera.viewProjection*glm::vec4(point,1);
      if(clip.w<=.1f) continue;
      glm::vec2 screen{(clip.x/clip.w*.5f+.5f)*float(w),(.5f-clip.y/clip.w*.5f)*float(h)};
      auto name=animalName(c,i); float half=float(name.size())*3.6f+10;
      if(screen.x<half || screen.x>float(w)-half || screen.y<114 || screen.y>float(h)-190) continue;
      hud.animalLabels.push_back({screen,std::move(name),isChick(c)});
    }
  }
  hud.width=int(w); hud.height=int(h); ui_.build(hud);
  Uint32 bytes=Uint32(ui_.vertices.size()*sizeof(UiVertex));
  if(bytes>uiCapacity_) {
    if(uiBuffer_) SDL_ReleaseGPUBuffer(device_,uiBuffer_);
    if(uiTransfer_) SDL_ReleaseGPUTransferBuffer(device_,uiTransfer_);
    uiCapacity_=bytes*2;
    SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_VERTEX,uiCapacity_,0}; uiBuffer_=SDL_CreateGPUBuffer(device_,&bi);
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,uiCapacity_,0}; uiTransfer_=SDL_CreateGPUTransferBuffer(device_,&ti);
    if(!uiBuffer_ || !uiTransfer_) fail("Create UI buffers");
  }
  auto* copy=SDL_BeginGPUCopyPass(command);
  if(bytes>0) {
    void* data=SDL_MapGPUTransferBuffer(device_,uiTransfer_,true); if(!data) fail("Map UI transfer");
    std::memcpy(data,ui_.vertices.data(),bytes); SDL_UnmapGPUTransferBuffer(device_,uiTransfer_);
    SDL_GPUTransferBufferLocation source{uiTransfer_,0}; SDL_GPUBufferRegion target{uiBuffer_,0,bytes};
    SDL_UploadToGPUBuffer(copy,&source,&target,true);
  }
  bool guides=hud.guide.preview && hud.help && !hud.hidden && !hud.paused && !hud.menuOpen() && !hud.sleeping;
  bool placement=hud.placement && !hud.hidden && !hud.paused && !hud.menuOpen() && !hud.sleeping && !hud.breaking;
  std::vector<Vertex> previews;
  if(guides) previews=buildPreview(*hud.guide.preview,hud.guide.previewBlock);
  Uint32 placementStart=Uint32(previews.size());
  if(placement) {
    const auto& p=*hud.placement;
    float material=p.status==PlacementStatus::Ready ? 22.f : 23.f;
    if(p.volume) appendBox(previews,*p.volume,p.cell,material,1.f);
    else {
      auto vertices=buildPreview(p.cell,p.block,material,true);
      previews.insert(previews.end(),vertices.begin(),vertices.end());
    }
  }
  if(!previews.empty()) {
    auto* memory=SDL_MapGPUTransferBuffer(device_,previewTransfer_,true); if(!memory) fail("Map building guide");
    Uint32 previewBytes=Uint32(previews.size()*sizeof(Vertex));
    std::memcpy(memory,previews.data(),previewBytes); SDL_UnmapGPUTransferBuffer(device_,previewTransfer_);
    SDL_GPUTransferBufferLocation ps{previewTransfer_,0}; SDL_GPUBufferRegion pd{previewBuffer_,0,previewBytes};
    SDL_UploadToGPUBuffer(copy,&ps,&pd,true);
  }
  auto particles=debris.mesh(right,up);
  if(!particles.empty()) {
    auto* memory=SDL_MapGPUTransferBuffer(device_,debrisTransfer_,true); if(!memory) fail("Map debris");
    Uint32 particleBytes=Uint32(particles.size()*sizeof(Vertex));
    std::memcpy(memory,particles.data(),particleBytes); SDL_UnmapGPUTransferBuffer(device_,debrisTransfer_);
    SDL_GPUTransferBufferLocation source{debrisTransfer_,0}; SDL_GPUBufferRegion destination{debrisBuffer_,0,particleBytes};
    SDL_UploadToGPUBuffer(copy,&source,&destination,true);
  }
  auto chickens=chickenMesh(world);
  auto ranch=ranchMesh(world); chickens.insert(chickens.end(),ranch.begin(),ranch.end());
  if(!chickens.empty()) {
    if(chickens.size()>chickenVertexLimit+ranchVertexLimit) throw std::runtime_error("Farm mesh exceeds its buffer");
    auto* memory=SDL_MapGPUTransferBuffer(device_,chickenTransfer_,true); if(!memory) fail("Map chickens");
    Uint32 size=Uint32(chickens.size()*sizeof(Vertex));
    std::memcpy(memory,chickens.data(),size); SDL_UnmapGPUTransferBuffer(device_,chickenTransfer_);
    SDL_GPUTransferBufferLocation source{chickenTransfer_,0}; SDL_GPUBufferRegion destination{chickenBuffer_,0,size};
    SDL_UploadToGPUBuffer(copy,&source,&destination,true);
  }
  SDL_EndGPUCopyPass(copy);
  SDL_GPUColorTargetInfo color{}; color.texture=color_; color.load_op=SDL_GPU_LOADOP_CLEAR; color.store_op=SDL_GPU_STOREOP_STORE; color.clear_color={.69f,.8f,.81f,1}; color.cycle=true;
  SDL_GPUDepthStencilTargetInfo depth{}; depth.texture=depth_; depth.clear_depth=1; depth.load_op=SDL_GPU_LOADOP_CLEAR; depth.store_op=SDL_GPU_STOREOP_DONT_CARE;
  depth.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE; depth.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE; depth.cycle=true;
  auto* pass=SDL_BeginGPURenderPass(command,&color,1,&depth);
  SDL_PushGPUFragmentUniformData(command,0,&camera,sizeof(camera));
  SDL_BindGPUGraphicsPipeline(pass,skyPipeline_); SDL_DrawGPUPrimitives(pass,3,1,0,0);
  SDL_BindGPUGraphicsPipeline(pass,worldPipeline_); SDL_PushGPUVertexUniformData(command,0,&camera,sizeof(camera));
  for(const auto& [pos,mesh] : meshes_) if(mesh.buffer && visible(pos,camera.viewProjection)) {
    SDL_GPUBufferBinding binding{mesh.buffer,0}; SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,mesh.opaqueVertices,1,0,0);
  }
  if(!chickens.empty()) {
    SDL_GPUBufferBinding binding{chickenBuffer_,0}; SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,Uint32(chickens.size()),1,0,0);
  }
  if(!particles.empty()) {
    SDL_GPUBufferBinding binding{debrisBuffer_,0}; SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,Uint32(particles.size()),1,0,0);
  }
  SDL_BindGPUGraphicsPipeline(pass,glassPipeline_);
  for(const auto& [pos,mesh] : meshes_) if(mesh.buffer && mesh.vertices>mesh.opaqueVertices && visible(pos,camera.viewProjection)) {
    SDL_GPUBufferBinding binding{mesh.buffer,0}; SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,mesh.vertices-mesh.opaqueVertices,1,mesh.opaqueVertices,0);
  }
  if(placement) {
    SDL_BindGPUGraphicsPipeline(pass,placementPipeline_);
    SDL_GPUBufferBinding binding{previewBuffer_,0}; SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,36,1,placementStart,0);
  }
  if(guides) {
    SDL_BindGPUGraphicsPipeline(pass,previewPipeline_);
    SDL_GPUBufferBinding previewBinding{previewBuffer_,0}; SDL_BindGPUVertexBuffers(pass,0,&previewBinding,1);
    SDL_DrawGPUPrimitives(pass,36,1,0,0);
  }
  if(!ui_.vertices.empty()) {
    SDL_BindGPUGraphicsPipeline(pass,uiPipeline_);
    SDL_GPUTextureSamplerBinding fontBinding{fontTexture_,fontSampler_}; SDL_BindGPUFragmentSamplers(pass,0,&fontBinding,1);
    glm::vec4 screen{float(w),float(h),0,0}; SDL_PushGPUVertexUniformData(command,0,&screen,sizeof(screen));
    SDL_GPUBufferBinding binding{uiBuffer_,0}; SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,Uint32(ui_.vertices.size()),1,0,0);
  }
  SDL_EndGPURenderPass(pass);

  SDL_GPUTransferBuffer* download{};
  if(!screenshot.empty()) {
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*4,0};
    download=SDL_CreateGPUTransferBuffer(device_,&ti); if(!download) fail("Create screenshot buffer");
    copy=SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureRegion region{}; region.texture=color_; region.w=w; region.h=h; region.d=1;
    SDL_GPUTextureTransferInfo destination{download,0,w,h}; SDL_DownloadFromGPUTexture(copy,&region,&destination);
    SDL_EndGPUCopyPass(copy);
  }
  SDL_GPUBlitInfo blit{};
  blit.source.texture=color_; blit.source.w=w; blit.source.h=h;
  blit.destination.texture=swapchain; blit.destination.w=w; blit.destination.h=h;
  blit.load_op=SDL_GPU_LOADOP_DONT_CARE; blit.filter=SDL_GPU_FILTER_NEAREST;
  SDL_BlitGPUTexture(command,&blit);
  if(download) {
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command); if(!fence) fail("Submit screenshot");
    if(!SDL_WaitForGPUFences(device_,true,&fence,1)) fail("Wait for screenshot");
    auto* pixels=SDL_MapGPUTransferBuffer(device_,download,false); if(!pixels) fail("Read screenshot");
    auto* surface=SDL_CreateSurfaceFrom(int(w),int(h),SDL_GetPixelFormatFromGPUTextureFormat(format_),pixels,int(w*4));
    if(!surface) fail("Create screenshot surface");
    if(!screenshot.parent_path().empty()) std::filesystem::create_directories(screenshot.parent_path());
    bool saved=SDL_SaveBMP(surface,screenshot.string().c_str()); SDL_DestroySurface(surface);
    SDL_UnmapGPUTransferBuffer(device_,download); SDL_ReleaseGPUTransferBuffer(device_,download); SDL_ReleaseGPUFence(device_,fence);
    if(!saved) fail("Save screenshot");
    std::cout<<"Screenshot: "<<screenshot<<" ("<<w<<'x'<<h<<")\n";
  } else if(!SDL_SubmitGPUCommandBuffer(command)) fail("Submit frame");
}
} // namespace bw
