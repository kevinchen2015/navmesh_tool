#ifndef _EDITOR_H_
#define _EDITOR_H_

#define ErrorID int
enum EditorError
{
	EDITOR_ERROR_NO_ERROR = 0,
	EDITOR_ERROR_UNKNOW,
	EDITOR_MESH_TYPE_ERROR,
	EDITOR_NAVMESH_ERROR,
};

enum NavmeshType
{
	NAVMESH_SOLOMESH = 0,
	NAVMESH_TILEMESH,
	NAVMESH_TILECACHE,
};

struct MyVertex
{
	float v[3];
};



struct MyPoly    //对应dtPoly ，取部分数据
{
	unsigned char area;
	int vertex_num;
	int index_num;
	MyVertex* vertexs;
	int*	  indexs;
	
	void*   org_poly;  //dtPoly

	MyPoly()
	{
		vertex_num = 0;
		index_num = 0;
		vertexs = (MyVertex*)0;
		indexs = (int*)0;
		area = 0;
		org_poly = (void*)0;
	}
};

struct MyNavmeshRenderInfo
{
	int		poly_num;
	MyPoly* polys;

	MyNavmeshRenderInfo()
	{
		polys = (MyPoly*)0;
		poly_num = 0;
	}
};

struct MyConvexVolume
{
	unsigned char area;
	int nverts;
	float* verts;
	float hmin;
	float hmax;
};

struct MyConvexVolumeInfo
{
	int convex_num;
	MyConvexVolume* convex;
};


#define EXPORT_FUNC_API extern "C" __declspec(dllexport) 

typedef  void(__stdcall *log_callback)(int type,const char* msg);
typedef void (__stdcall *event_callback)();

EXPORT_FUNC_API ErrorID editor_init();
EXPORT_FUNC_API void	editor_uninit();
EXPORT_FUNC_API ErrorID editor_geom_reload(char* path);
EXPORT_FUNC_API ErrorID editor_geom_save(char* path);
EXPORT_FUNC_API void	editor_set_log_callback(log_callback cb);
EXPORT_FUNC_API void	editor_set_nav_mesh_changed_cb(event_callback cb);
EXPORT_FUNC_API void    editor_set_nav_mesh_type(NavmeshType type);
EXPORT_FUNC_API ErrorID editor_nav_mesh_build();
EXPORT_FUNC_API ErrorID editor_save_nav_mesh(char* path);
EXPORT_FUNC_API ErrorID editor_load_nav_mesh(char* path);
EXPORT_FUNC_API ErrorID editor_get_nav_mesh_render_info(MyNavmeshRenderInfo* info);
EXPORT_FUNC_API ErrorID editor_nav_mesh_path_query(MyVertex* start, MyVertex* end,unsigned short include_flag,unsigned short exclude_flag, int* num, float** vertext);
EXPORT_FUNC_API int		editor_geom_add_convex_volume(float* vertext,int count,float minh,float maxh,unsigned char area);
EXPORT_FUNC_API void	editor_geom_delete_convex_volume(int id);
EXPORT_FUNC_API void    editor_geom_get_convex_info(MyConvexVolumeInfo* info);
EXPORT_FUNC_API int     editor_nav_mesh_query(MyVertex* pos,unsigned int* poly_ref,unsigned short* flag,unsigned char* area);
EXPORT_FUNC_API void    editor_nav_mesh_modify(unsigned int poly_ref, unsigned short flag,unsigned char area);
/*
//todo
EXPORT_FUNC_API ErrorID editor_tile_cache_setting(struct TileCacheBuildSetting* setting);
EXPORT_FUNC_API ErrorID editor_tile_cache_build(bool save);

EXPORT_FUNC_API int editor_tile_cache_add_obstacles();
EXPORT_FUNC_API int editor_tile_cache_add_box_obstacles();
EXPORT_FUNC_API void editor_tile_cache_remove_obstacles();
*/






#endif

