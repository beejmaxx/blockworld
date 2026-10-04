#pragma once
#include "player.hpp"
#include "ui.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <unordered_map>

namespace bw {
class Renderer {
public:
  explicit Renderer(SDL_Window* window);
  ~Renderer();
  Renderer(const Renderer&)=delete;
  Renderer& operator=(const Renderer&)=delete;
  void sync(World& world,ChunkPos center,int budget=3);
  void draw(const Player& player,const std::optional<RayHit>& hit,HudState hud,float time,
            const DebrisCloud& debris,const World& world,const std::filesystem::path& screenshot={});
  std::size_t triangleCount() const;
  std::size_t meshBytes() const { return triangleCount()*3*sizeof(Vertex); }
  std::size_t meshCount() const { return meshes_.size(); }
private:
  struct Mesh { SDL_GPUBuffer* buffer{}; Uint32 vertices{},opaqueVertices{}; std::vector<glm::vec3> lights; };
  SDL_Window* window_{};
  SDL_GPUDevice* device_{};
  SDL_GPUGraphicsPipeline *worldPipeline_{},*glassPipeline_{},*skyPipeline_{},*uiPipeline_{},*previewPipeline_{},*placementPipeline_{};
  SDL_GPUTexture *color_{},*depth_{};
  SDL_GPUTexture* fontTexture_{};
  SDL_GPUSampler* fontSampler_{};
  Uint32 width_{},height_{};
  SDL_GPUTextureFormat format_{};
  SDL_GPUBuffer* uiBuffer_{};
  SDL_GPUTransferBuffer* uiTransfer_{};
  SDL_GPUBuffer* previewBuffer_{};
  SDL_GPUTransferBuffer* previewTransfer_{};
  SDL_GPUBuffer* debrisBuffer_{};
  SDL_GPUTransferBuffer* debrisTransfer_{};
  SDL_GPUBuffer* chickenBuffer_{};
  SDL_GPUTransferBuffer* chickenTransfer_{};
  Uint32 uiCapacity_{};
  std::unordered_map<ChunkPos,Mesh,PositionHash> meshes_;
  Ui ui_;
  std::vector<Vertex> skyline_;
  glm::vec3 skylineEye_{};
  float skylineUpdated_=-100;
  std::optional<Cell> skylineOrigin_;
  MiniMap mapCache_;
  float mapUpdated_=-100;
  bool mapOverview_=false,mapDriving_=false;
  SDL_GPUShader* shader(const char* file,const char* entry,SDL_GPUShaderStage stage,Uint32 uniforms,Uint32 samplers=0);
  SDL_GPUGraphicsPipeline* pipeline(const char* file,const char* vertex,const char* fragment,int kind);
  void targets(Uint32 width,Uint32 height);
  void cleanup() noexcept;
};
} // namespace bw
