#include "navmesh_query_helper.h"
#include "define.h"
#include "DetourNavMeshQuery.h"
#include "DetourCommon.h"
#include <stdio.h>
#include <list>
#include <vector>

static const int MAX_POLYS = 256;
static const int MAX_SMOOTH = 2048;


static bool getSteerTarget(dtNavMeshQuery* navQuery, const float* startPos, const float* endPos,
	const float minTargetDist,
	const dtPolyRef* path, const int pathSize,
	float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef,
	float* outPoints = 0, int* outPointCount = 0)
{
	// Find steer target.
	static const int MAX_STEER_POINTS = 3;
	float steerPath[MAX_STEER_POINTS * 3];
	unsigned char steerPathFlags[MAX_STEER_POINTS];
	dtPolyRef steerPathPolys[MAX_STEER_POINTS];
	int nsteerPath = 0;
	navQuery->findStraightPath(startPos, endPos, path, pathSize,
		steerPath, steerPathFlags, steerPathPolys, &nsteerPath, MAX_STEER_POINTS);
	if (!nsteerPath)
		return false;

	if (outPoints && outPointCount)
	{
		*outPointCount = nsteerPath;
		for (int i = 0; i < nsteerPath; ++i)
			dtVcopy(&outPoints[i * 3], &steerPath[i * 3]);
	}

	// Find vertex far enough to steer to.
	int ns = 0;
	while (ns < nsteerPath)
	{
		// Stop at Off-Mesh link or when point is further than slop away.
		if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
			!inRange(&steerPath[ns * 3], startPos, minTargetDist, 1000.0f))
			break;
		ns++;
	}
	// Failed to find good point to steer to.
	if (ns >= nsteerPath)
		return false;

	dtVcopy(steerPos, &steerPath[ns * 3]);
	steerPos[1] = startPos[1];
	steerPosFlag = steerPathFlags[ns];
	steerPosRef = steerPathPolys[ns];

	return true;
}

static int fixupCorridor(dtPolyRef* path, const int npath, const int maxPath,
	const dtPolyRef* visited, const int nvisited)
{
	int furthestPath = -1;
	int furthestVisited = -1;

	// Find furthest common polygon.
	for (int i = npath - 1; i >= 0; --i)
	{
		bool found = false;
		for (int j = nvisited - 1; j >= 0; --j)
		{
			if (path[i] == visited[j])
			{
				furthestPath = i;
				furthestVisited = j;
				found = true;
			}
		}
		if (found)
			break;
	}

	// If no intersection found just return current path. 
	if (furthestPath == -1 || furthestVisited == -1)
		return npath;

	// Concatenate paths.	

	// Adjust beginning of the buffer to include the visited.
	const int req = nvisited - furthestVisited;
	const int orig = rcMin(furthestPath + 1, npath);
	int size = rcMax(0, npath - orig);
	if (req + size > maxPath)
		size = maxPath - req;
	if (size)
		memmove(path + req, path + orig, size * sizeof(dtPolyRef));

	// Store visited
	for (int i = 0; i < req; ++i)
		path[i] = visited[(nvisited - 1) - i];

	return req + size;
}

static int fixupShortcuts(dtPolyRef* path, int npath, dtNavMeshQuery* navQuery)
{
	if (npath < 3)
		return npath;

	// Get connected polygons
	static const int maxNeis = 16;
	dtPolyRef neis[maxNeis];
	int nneis = 0;

	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	if (dtStatusFailed(navQuery->getAttachedNavMesh()->getTileAndPolyByRef(path[0], &tile, &poly)))
		return npath;

	for (unsigned int k = poly->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
	{
		const dtLink* link = &tile->links[k];
		if (link->ref != 0)
		{
			if (nneis < maxNeis)
				neis[nneis++] = link->ref;
		}
	}

	// If any of the neighbour polygons is within the next few polygons
	// in the path, short cut to that polygon directly.
	static const int maxLookAhead = 6;
	int cut = 0;
	for (int i = dtMin(maxLookAhead, npath) - 1; i > 1 && cut == 0; i--) {
		for (int j = 0; j < nneis; j++)
		{
			if (path[i] == neis[j]) {
				cut = i;
				break;
			}
		}
	}
	if (cut > 1)
	{
		int offset = cut - 1;
		npath -= offset;
		for (int i = 1; i < npath; i++)
			path[i] = path[i + offset];
	}

	return npath;
}


unsigned int navmesh_query_poly_ref(dtNavMeshQuery* query, float* pos)
{
	dtPolyRef ref;
	dtQueryFilter filter;
	float polyPickExt[3];
	polyPickExt[0] = 2;
	polyPickExt[1] = 4;
	polyPickExt[2] = 2;
	query->findNearestPoly(pos, polyPickExt, &filter, &ref, 0);
	return (unsigned int)ref;
}

int navmesh_query_path(dtNavMeshQuery* query, float* start, float* end, unsigned short include_flag, unsigned short exclude_flag,
	int* num, float** path)
{
	*num = 0;

	dtPolyRef startRef;
	dtPolyRef endRef;
	float polyPickExt[3];
	polyPickExt[0] = 2;
	polyPickExt[1] = 4;
	polyPickExt[2] = 2;
	dtPolyRef org_polys[MAX_POLYS];
	dtPolyRef parent[MAX_POLYS];
	static float vsmoothPath[MAX_SMOOTH * 3];
	int npolys = 0;
	int smoothPath = 0;

	dtQueryFilter filter;
	filter.setIncludeFlags(include_flag);
	filter.setExcludeFlags(exclude_flag);

	query->findNearestPoly(start, polyPickExt, &filter, &startRef, 0);
	query->findNearestPoly(end, polyPickExt, &filter, &endRef, 0);

	//TOOLMODE_PATHFIND_FOLLOW
	{
		if (startRef && endRef)
		{
			query->findPath(startRef, endRef, start, end, &filter, org_polys, &npolys, MAX_POLYS);
			smoothPath = 0;
			if (npolys)
			{
				// Iterate over the path to find smooth path on the detail mesh surface.
				dtPolyRef polys[MAX_POLYS];
				memcpy(polys, org_polys, sizeof(dtPolyRef)*npolys);
				int npoly = npolys;

				float iterPos[3], targetPos[3];
				query->closestPointOnPoly(startRef, start, iterPos, 0);
				query->closestPointOnPoly(polys[npoly - 1], end, targetPos, 0);

				static const float STEP_SIZE = 0.5f;
				static const float SLOP = 0.01f;

				smoothPath = 0;

				dtVcopy(&vsmoothPath[smoothPath * 3], iterPos);
				smoothPath++;

				// Move towards target a small advancement at a time until target reached or
				// when ran out of memory to store the path.
				while (npoly && smoothPath < MAX_SMOOTH)
				{
					// Find location to steer towards.
					float steerPos[3];
					unsigned char steerPosFlag;
					dtPolyRef steerPosRef;

					if (!getSteerTarget(query, iterPos, targetPos, SLOP,
						polys, npoly, steerPos, steerPosFlag, steerPosRef))
						break;

					bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END) ? true : false;
					bool offMeshConnection = (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ? true : false;

					// Find movement delta.
					float delta[3], len;
					dtVsub(delta, steerPos, iterPos);
					len = dtMathSqrtf(dtVdot(delta, delta));
					// If the steer target is end of path or off-mesh link, do not move past the location.
					if ((endOfPath || offMeshConnection) && len < STEP_SIZE)
						len = 1;
					else
						len = STEP_SIZE / len;
					float moveTgt[3];
					dtVmad(moveTgt, iterPos, delta, len);

					// Move
					float result[3];
					dtPolyRef visited[16];
					int nvisited = 0;
					query->moveAlongSurface(polys[0], iterPos, moveTgt, &filter,
						result, visited, &nvisited, 16);

					npoly = fixupCorridor(polys, npoly, MAX_POLYS, visited, nvisited);
					npoly = fixupShortcuts(polys, npoly, query);

					float h = 0;
					query->getPolyHeight(polys[0], result, &h);
					result[1] = h;
					dtVcopy(iterPos, result);

					// Handle end of path and off-mesh links when close enough.
					if (endOfPath && inRange(iterPos, steerPos, SLOP, 1.0f))
					{
						// Reached end of path.
						dtVcopy(iterPos, targetPos);
						if (smoothPath < MAX_SMOOTH)
						{
							dtVcopy(&vsmoothPath[smoothPath * 3], iterPos);
							smoothPath++;
						}
						break;
					}
					else if (offMeshConnection && inRange(iterPos, steerPos, SLOP, 1.0f))
					{
						// Reached off-mesh connection.
						float startPos[3], endPos[3];

						// Advance the path up to and over the off-mesh connection.
						dtPolyRef prevRef = 0, polyRef = polys[0];
						int npos = 0;
						while (npos < npoly && polyRef != steerPosRef)
						{
							prevRef = polyRef;
							polyRef = polys[npos];
							npos++;
						}
						for (int i = npos; i < npoly; ++i)
							polys[i - npos] = polys[i];
						npoly -= npos;

						// Handle the connection.
						dtNavMesh* navmesh = query->GetNavMesh();
						dtStatus status = navmesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef, startPos, endPos);
						if (dtStatusSucceed(status))
						{
							if (smoothPath < MAX_SMOOTH)
							{
								dtVcopy(&vsmoothPath[smoothPath * 3], startPos);
								smoothPath++;
								// Hack to make the dotted path not visible during off-mesh connection.
								if (smoothPath & 1)
								{
									dtVcopy(&vsmoothPath[smoothPath * 3], startPos);
									smoothPath++;
								}
							}
							// Move position at the other side of the off-mesh link.
							dtVcopy(iterPos, endPos);
							float eh = 0.0f;
							query->getPolyHeight(polys[0], iterPos, &eh);
							iterPos[1] = eh;
						}
					}

					// Store results.
					if (smoothPath < MAX_SMOOTH)
					{
						dtVcopy(&vsmoothPath[smoothPath * 3], iterPos);
						smoothPath++;
					}
				}
			}
		}
		else
		{
			npolys = 0;
			smoothPath = 0;
		}
	}

	*num = smoothPath;
	*path = vsmoothPath;
	
	return EDITOR_ERROR_NO_ERROR;
}