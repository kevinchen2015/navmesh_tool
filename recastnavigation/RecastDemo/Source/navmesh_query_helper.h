#ifndef _NAVMESH_QUERY_HELPER_H_
#define _NAVMESH_QUERY_HELPER_H_

class dtNavMeshQuery;

int navmesh_query_path(dtNavMeshQuery* query,float* start, float* end, unsigned short include_flag, unsigned short exclude_flag,
	int* num, float** path);

unsigned int navmesh_query_poly_ref(dtNavMeshQuery* query, float* pos);

#endif

