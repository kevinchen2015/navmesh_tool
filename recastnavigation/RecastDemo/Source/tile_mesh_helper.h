#ifndef _TILE_MESH_HELPER_H_
#define _TILE_MESH_HELPER_H_



struct Editor;
struct MyNavmeshRenderInfo;
class dtNavMesh;

bool tile_mesh_build(struct Editor* editor);
void tile_mesh_save(char* path, dtNavMesh* mesh);
dtNavMesh* tile_mesh_save_load(const char* path);
void tile_mesh_get_render_info(dtNavMesh* mesh, MyNavmeshRenderInfo* info);

#endif

