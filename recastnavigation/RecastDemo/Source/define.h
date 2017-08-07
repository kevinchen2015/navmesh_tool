#ifndef _EDITOR_DEFINE_H_
#define _EDITOR_DEFINE_H_

#include "editor.h"
#include "InputGeom.h"
#include "Recast.h"

struct EditorBuildSetting : public BuildSettings
{
	int maxTiles;
	int maxPolysPerTile;

	bool filterLowHangingObstacles;
	bool filterLedgeSpans;
	bool filterWalkableLowHeightSpans;
};

class EditorContext : public rcContext
{

public:
	virtual void doLog(const rcLogCategory category, const char* msg, const int len);
};

class dtNavMesh;
class dtNavMeshQuery;

struct Editor
{
	InputGeom* input_geom_;
	dtNavMesh* nav_mesh_;
	dtNavMeshQuery* nav_mesh_query_;
	EditorContext* build_context_;

	event_callback nav_mesh_changed_;
	NavmeshType nav_mesh_type_;

	EditorBuildSetting build_setting_;

	Editor()
	{
		input_geom_ = (InputGeom*)0;
		nav_mesh_ = (dtNavMesh*)0;
		nav_mesh_query_ = (dtNavMeshQuery*)0;
		nav_mesh_changed_ = (event_callback)0;
		build_context_ = (EditorContext*)0;
		nav_mesh_type_ = NAVMESH_SOLOMESH;
	}
};


inline unsigned int nextPow2(unsigned int v);
inline unsigned int ilog2(unsigned int v);
bool inRange(const float* v1, const float* v2, const float r, const float h);
unsigned short AreaToFlag(unsigned char* area);

#endif

