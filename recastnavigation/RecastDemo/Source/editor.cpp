#include "editor.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "InputGeom.h"
#include "SampleInterfaces.h"
#include "PerfTimer.h"
#include "tile_mesh_helper.h"
#include "navmesh_query_helper.h"
#include "define.h"
#include "Sample.h"
#include <stdio.h>
#include <string>


static Editor* g_editor;
log_callback g_log_callback;

//------------------------------------------------------------------
inline unsigned int nextPow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

inline unsigned int ilog2(unsigned int v)
{
	unsigned int r;
	unsigned int shift;
	r = (v > 0xffff) << 4; v >>= r;
	shift = (v > 0xff) << 3; v >>= shift; r |= shift;
	shift = (v > 0xf) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
	return r;
}


 bool inRange(const float* v1, const float* v2, const float r, const float h)
{
	const float dx = v2[0] - v1[0];
	const float dy = v2[1] - v1[1];
	const float dz = v2[2] - v1[2];
	return (dx*dx + dz*dz) < r*r && fabsf(dy) < h;
}

//------------------------------------------------------------------------------------------
void EditorContext::doLog(const rcLogCategory category, const char* msg, const int len)
{
	if (g_log_callback)
	{
		g_log_callback((int)category, msg);
	}
}


unsigned short AreaToFlag(unsigned char* area)
{
	unsigned short flag = 0;
	if (*area == RC_WALKABLE_AREA)
		*area = SAMPLE_POLYAREA_GROUND;

	if (*area == SAMPLE_POLYAREA_GROUND ||
		*area == SAMPLE_POLYAREA_GRASS ||
		*area == SAMPLE_POLYAREA_ROAD)
	{
		flag = SAMPLE_POLYFLAGS_WALK;
	}
	else if (*area == SAMPLE_POLYAREA_WATER)
	{
		flag = SAMPLE_POLYFLAGS_SWIM;
	}
	else if (*area == SAMPLE_POLYAREA_DOOR)
	{
		flag = SAMPLE_POLYFLAGS_WALK | SAMPLE_POLYFLAGS_DOOR;
	}
	else if (*area == POLYAREA_OBSTACLE_0)
	{
		flag = SAMPLE_POLYFLAGS_WALK | POLYFLAGS_OBSTACLE_0;
	}
	else if (*area == POLYAREA_OBSTACLE_1)
	{
		flag = SAMPLE_POLYFLAGS_WALK | POLYFLAGS_OBSTACLE_1;
	}
	return flag;
}

//-----------------------------------------------------------------------------------------------

void reset_common_settings(Editor* editor)
{
	editor->build_setting_.cellSize = 0.3f;
	editor->build_setting_.cellHeight = 0.2f;
	editor->build_setting_.agentHeight = 2.0f;
	editor->build_setting_.agentRadius = 0.6f;
	editor->build_setting_.agentMaxClimb = 0.9f;
	editor->build_setting_.agentMaxSlope = 45.0f;
	editor->build_setting_.regionMinSize = 8;
	editor->build_setting_.regionMergeSize = 20;
	editor->build_setting_.edgeMaxLen = 12.0f;
	editor->build_setting_.edgeMaxError = 1.3f;
	editor->build_setting_.vertsPerPoly = 6.0f;
	editor->build_setting_.detailSampleDist = 6.0f;
	editor->build_setting_.detailSampleMaxError = 1.0f;
	editor->build_setting_.partitionType = SAMPLE_PARTITION_WATERSHED;
	editor->build_setting_.tileSize = 32;

	editor->build_setting_.filterLowHangingObstacles = true;
	editor->build_setting_.filterLedgeSpans = true;
	editor->build_setting_.filterWalkableLowHeightSpans = true; 

}

void computer_setting(Editor* editor)
{
	int gw = 0, gh = 0;
	const float* bmin = editor->input_geom_->getNavMeshBoundsMin();
	const float* bmax = editor->input_geom_->getNavMeshBoundsMax();

	memcpy(editor->build_setting_.navMeshBMin, bmin, 3*sizeof(float));
	memcpy(editor->build_setting_.navMeshBMax, bmax, 3*sizeof(float));

	rcCalcGridSize(bmin, bmax, editor->build_setting_.cellSize, &gw, &gh);
	const int ts = (int)editor->build_setting_.tileSize;
	const int tw = (gw + ts - 1) / ts;
	const int th = (gh + ts - 1) / ts;

	// Max tiles and max polys affect how the tile IDs are caculated.
	// There are 22 bits available for identifying a tile and a polygon.
	int tileBits = rcMin((int)ilog2(nextPow2(tw*th)), 14);
	if (tileBits > 14) tileBits = 14;
	int polyBits = 22 - tileBits;
	editor->build_setting_.maxTiles = 1 << tileBits;
	editor->build_setting_.maxPolysPerTile = 1 << polyBits;
}

int editor_init()
{
	g_log_callback(RC_LOG_PROGRESS, "editor_init() ....");

	g_editor = new Editor();
	g_editor->input_geom_ = new InputGeom();
	g_editor->build_context_ = new EditorContext();
	g_editor->nav_mesh_query_ = dtAllocNavMeshQuery();

	reset_common_settings(g_editor);
	computer_setting(g_editor);

	g_log_callback(RC_LOG_PROGRESS, "editor_init() finished!");

	return 0;
}

void editor_uninit()
{
	g_log_callback(RC_LOG_PROGRESS, "editor_uninit() ....");

	dtFreeNavMeshQuery(g_editor->nav_mesh_query_);
	g_editor->nav_mesh_query_ = (dtNavMeshQuery*)0;
	dtFreeNavMesh(g_editor->nav_mesh_);
	g_editor->nav_mesh_ = (dtNavMesh*)0;
	delete g_editor->input_geom_;
	g_editor->input_geom_ = (InputGeom*)0;
	delete g_editor->build_context_;
	g_editor->build_context_ = (EditorContext*)0;
	delete g_editor;
	g_editor = (Editor*)0;

	g_log_callback(RC_LOG_PROGRESS, "editor_uninit() finished!");
}



ErrorID editor_geom_reload(char* path)
{
	delete g_editor->input_geom_;
	g_editor->input_geom_ = new InputGeom();

	std::string obj_file = path; obj_file += ".obj";
	std::string gemo_file = path; gemo_file += ".gset";

	g_editor->input_geom_->load(g_editor->build_context_,obj_file.c_str());
	g_editor->input_geom_->load(g_editor->build_context_, gemo_file.c_str());

	computer_setting(g_editor);

	return EDITOR_ERROR_NO_ERROR;
}

ErrorID editor_geom_save(char* path)
{
	g_editor->input_geom_->saveGeomSet(&g_editor->build_setting_);
	return EDITOR_ERROR_NO_ERROR;
}

void editor_set_log_callback(log_callback cb)
{
	g_log_callback = cb;
}

void editor_set_nav_mesh_changed_cb(event_callback cb)
{
	g_editor->nav_mesh_changed_ = cb;
}

void editor_set_nav_mesh_type(NavmeshType type)
{
	g_editor->nav_mesh_type_ = type;
}

ErrorID editor_nav_mesh_build()
{
	if (g_editor->nav_mesh_type_ == NAVMESH_TILEMESH)
	{
		bool b = tile_mesh_build(g_editor);
		g_editor->nav_mesh_query_->init(g_editor->nav_mesh_, 2048);
		if(g_editor->nav_mesh_changed_)
			g_editor->nav_mesh_changed_();
		if (b)
		{
			g_log_callback(RC_LOG_PROGRESS, "build navmesh sucessed!");
			return EDITOR_ERROR_NO_ERROR;
		}
		g_log_callback(RC_LOG_ERROR, "build navmesh error!");
		return EDITOR_ERROR_UNKNOW;
	}
	g_log_callback(RC_LOG_ERROR, "build navmesh type error!");
	return EDITOR_MESH_TYPE_ERROR;
}

ErrorID editor_save_nav_mesh(char* path)
{
	if (g_editor->nav_mesh_type_ == NAVMESH_TILEMESH)
	{
		tile_mesh_save(path,g_editor->nav_mesh_);
		return EDITOR_ERROR_NO_ERROR;
	}
	g_log_callback(RC_LOG_ERROR, "save navmesh type error!");
	return EDITOR_MESH_TYPE_ERROR;
}

ErrorID editor_load_nav_mesh(char* path)
{
	if (g_editor->nav_mesh_type_ == NAVMESH_TILEMESH)
	{
		dtFreeNavMesh(g_editor->nav_mesh_);
		g_editor->nav_mesh_ = tile_mesh_save_load(path);
		g_editor->nav_mesh_query_->init(g_editor->nav_mesh_, 2048);
		g_editor->nav_mesh_changed_();
		return EDITOR_ERROR_NO_ERROR;
	}
	g_log_callback(RC_LOG_ERROR, "load navmesh type error!");
	return EDITOR_MESH_TYPE_ERROR;
}

ErrorID editor_get_nav_mesh_render_info(MyNavmeshRenderInfo* info)
{
	if (g_editor->nav_mesh_type_ == NAVMESH_TILEMESH)
	{
		if (!g_editor->nav_mesh_) return EDITOR_NAVMESH_ERROR;
		tile_mesh_get_render_info(g_editor->nav_mesh_, info);
		return EDITOR_ERROR_NO_ERROR;
	}
	return EDITOR_MESH_TYPE_ERROR;
}

ErrorID editor_nav_mesh_path_query(MyVertex* start, MyVertex* end, unsigned short include_flag, unsigned short exclude_flag, int* num, float** vertext)
{
	navmesh_query_path(g_editor->nav_mesh_query_, start->v, end->v, include_flag,exclude_flag, num, vertext);
	char str[1024] = { 0x00 };
	sprintf(str, "nav_mesh_path_query,star:%f,%f,%f, end:%f,%f,%f , num:%d",
		start->v[0], start->v[1], start->v[2], end->v[0], end->v[1], end->v[2],*num);
	g_log_callback(RC_LOG_PROGRESS, str);
	return EDITOR_ERROR_NO_ERROR;
}

unsigned int editor_nav_mesh_query_refid(MyVertex pos)
{
	if (!g_editor || !g_editor->nav_mesh_query_)
		return 0;

	return navmesh_query_poly_ref(g_editor->nav_mesh_query_, pos.v);
}

int editor_nav_mesh_query(MyVertex* pos, unsigned int* poly_ref, unsigned short* flag, unsigned char* area)
{
	if (!g_editor || !g_editor->nav_mesh_query_ || !g_editor->nav_mesh_)
		return EDITOR_ERROR_UNKNOW;

	*poly_ref = navmesh_query_poly_ref(g_editor->nav_mesh_query_, pos->v);
	if (*poly_ref > 0)
	{
		g_editor->nav_mesh_->getPolyArea(*poly_ref, area);
		g_editor->nav_mesh_->getPolyFlags(*poly_ref, flag);
	}
	else
	{
		*flag = 0;
		*area = 0;
	}
}

void editor_nav_mesh_modify(unsigned int poly_ref, unsigned short flag, unsigned char area)
{
	if (!g_editor || !g_editor->nav_mesh_)
		return;

	if (g_editor->nav_mesh_->isValidPolyRef(poly_ref))
	{
		g_editor->nav_mesh_->setPolyArea(poly_ref, area);
		g_editor->nav_mesh_->setPolyFlags(poly_ref, flag);
	}
}

int	editor_geom_add_convex_volume(float* vertext, int count, float minh, float maxh, unsigned char area)
{
	if (!g_editor || !g_editor->input_geom_)
		return -1;

	return g_editor->input_geom_->addConvexVolume(vertext, count, minh, maxh, area);
}

void editor_geom_delete_convex_volume(int id)
{
	if (g_editor && g_editor->input_geom_)
	{
		g_editor->input_geom_->deleteConvexVolume(id);
	}
}

void editor_geom_get_convex_info(MyConvexVolumeInfo* info)
{
	for (int i = 0; i < info->convex_num; ++i)
	{
		MyConvexVolume& coovex = info->convex[i];
		if (coovex.verts)
		{
			free(coovex.verts);
		}
	}
	free(info->convex);
	info->convex = (MyConvexVolume*)0;
	info->convex_num = 0;
	if (g_editor && g_editor->input_geom_)
	{
		info->convex_num = g_editor->input_geom_->getConvexVolumeCount();
		info->convex = (MyConvexVolume*)malloc(sizeof(MyConvexVolume)*info->convex_num);

		const ConvexVolume* convex_volume = g_editor->input_geom_->getConvexVolumes();
		for (int i = 0; i < info->convex_num; ++i)
		{
			info->convex[i].nverts = convex_volume[i].nverts;
			int size = sizeof(float)*info->convex[i].nverts * 3;
			info->convex[i].verts = (float*)malloc(size);
			memcpy(info->convex[i].verts, convex_volume[i].verts, size);

			info->convex[i].area = convex_volume[i].area;
			info->convex[i].hmin = convex_volume[i].hmin;
			info->convex[i].hmax = convex_volume[i].hmax;
		}
	}
}



