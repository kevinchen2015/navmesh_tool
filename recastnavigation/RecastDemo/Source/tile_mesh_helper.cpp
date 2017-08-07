#include "tile_mesh_helper.h"
#include "define.h"
#include "Sample_TileMesh.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "InputGeom.h"
#include "SampleInterfaces.h"
#include "DetourNavMeshBuilder.h"
#include <stdio.h>
#include <list>
#include <vector>

unsigned char* buildTileMesh(struct Editor* editor, const int tx, const int ty, const float* bmin, const float* bmax, int& dataSize)
{
#if 1
	if (!editor->input_geom_ || !editor->input_geom_ ->getMesh() || !editor->input_geom_ ->getChunkyMesh())
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Input mesh is not specified.");
		return 0;
	}

	float m_tileMemUsage = 0;
	float m_tileBuildTime = 0;

	const float* verts = editor->input_geom_ ->getMesh()->getVerts();
	const int nverts = editor->input_geom_ ->getMesh()->getVertCount();
	const int ntris = editor->input_geom_ ->getMesh()->getTriCount();
	const rcChunkyTriMesh* chunkyMesh = editor->input_geom_ ->getChunkyMesh();

	rcConfig m_cfg;
	memset(&m_cfg, 0, sizeof(m_cfg));
	m_cfg.cs = editor->build_setting_.cellSize;
	m_cfg.ch = editor->build_setting_.cellHeight;
	m_cfg.walkableSlopeAngle = editor->build_setting_.agentMaxSlope;
	m_cfg.walkableHeight = (int)ceilf(editor->build_setting_.agentHeight / m_cfg.ch);
	m_cfg.walkableClimb = (int)floorf(editor->build_setting_.agentMaxClimb / m_cfg.ch);
	m_cfg.walkableRadius = (int)ceilf(editor->build_setting_.agentRadius / m_cfg.cs);
	m_cfg.maxEdgeLen = (int)(editor->build_setting_.edgeMaxLen / editor->build_setting_.cellSize);
	m_cfg.maxSimplificationError = editor->build_setting_.edgeMaxError;
	m_cfg.minRegionArea = (int)rcSqr(editor->build_setting_.regionMinSize);		// Note: area = size*size
	m_cfg.mergeRegionArea = (int)rcSqr(editor->build_setting_.regionMergeSize);	// Note: area = size*size
	m_cfg.maxVertsPerPoly = (int)editor->build_setting_.vertsPerPoly;
	m_cfg.tileSize = (int)editor->build_setting_.tileSize;
	m_cfg.borderSize = m_cfg.walkableRadius + 3; // Reserve enough padding.
	m_cfg.width = m_cfg.tileSize + m_cfg.borderSize * 2;
	m_cfg.height = m_cfg.tileSize + m_cfg.borderSize * 2;
	m_cfg.detailSampleDist = editor->build_setting_.detailSampleDist < 0.9f ? 0 : editor->build_setting_.cellSize * editor->build_setting_.detailSampleDist;
	m_cfg.detailSampleMaxError = editor->build_setting_.cellHeight * editor->build_setting_.detailSampleMaxError;

	// Expand the heighfield bounding box by border size to find the extents of geometry we need to build this tile.
	//
	// This is done in order to make sure that the navmesh tiles connect correctly at the borders,
	// and the obstacles close to the border work correctly with the dilation process.
	// No polygons (or contours) will be created on the border area.
	//
	// IMPORTANT!
	//
	//   :''''''''':
	//   : +-----+ :
	//   : |     | :
	//   : |     |<--- tile to build
	//   : |     | :  
	//   : +-----+ :<-- geometry needed
	//   :.........:
	//
	// You should use this bounding box to query your input geometry.
	//
	// For example if you build a navmesh for terrain, and want the navmesh tiles to match the terrain tile size
	// you will need to pass in data from neighbour terrain tiles too! In a simple case, just pass in all the 8 neighbours,
	// or use the bounding box below to only pass in a sliver of each of the 8 neighbours.
	rcVcopy(m_cfg.bmin, bmin);
	rcVcopy(m_cfg.bmax, bmax);
	m_cfg.bmin[0] -= m_cfg.borderSize*m_cfg.cs;
	m_cfg.bmin[2] -= m_cfg.borderSize*m_cfg.cs;
	m_cfg.bmax[0] += m_cfg.borderSize*m_cfg.cs;
	m_cfg.bmax[2] += m_cfg.borderSize*m_cfg.cs;

	// Reset build times gathering.
	editor->build_context_->resetTimers();

	// Start the build process.
	editor->build_context_->startTimer(RC_TIMER_TOTAL);

	editor->build_context_->log(RC_LOG_PROGRESS, "Building navigation:");
	editor->build_context_->log(RC_LOG_PROGRESS, " - %d x %d cells", m_cfg.width, m_cfg.height);
	editor->build_context_->log(RC_LOG_PROGRESS, " - %.1fK verts, %.1fK tris", nverts / 1000.0f, ntris / 1000.0f);

	// Allocate voxel heightfield where we rasterize our input data to.
	rcHeightfield* m_solid = rcAllocHeightfield();
	if (!m_solid)
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'solid'.");
		return 0;
	}
	if (!rcCreateHeightfield(editor->build_context_, *m_solid, m_cfg.width, m_cfg.height, m_cfg.bmin, m_cfg.bmax, m_cfg.cs, m_cfg.ch))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not create solid heightfield.");
		return 0;
	}

	// Allocate array that can hold triangle flags.
	// If you have multiple meshes you need to process, allocate
	// and array which can hold the max number of triangles you need to process.
	unsigned char* m_triareas = new unsigned char[chunkyMesh->maxTrisPerChunk];
	if (!m_triareas)
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'm_triareas' (%d).", chunkyMesh->maxTrisPerChunk);
		return 0;
	}

	float tbmin[2], tbmax[2];
	tbmin[0] = m_cfg.bmin[0];
	tbmin[1] = m_cfg.bmin[2];
	tbmax[0] = m_cfg.bmax[0];
	tbmax[1] = m_cfg.bmax[2];
	int cid[512];// TODO: Make grow when returning too many items.
	const int ncid = rcGetChunksOverlappingRect(chunkyMesh, tbmin, tbmax, cid, 512);
	if (!ncid)
		return 0;

	int m_tileTriCount = 0;

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
		const int* ctris = &chunkyMesh->tris[node.i * 3];
		const int nctris = node.n;

		m_tileTriCount += nctris;

		memset(m_triareas, 0, nctris * sizeof(unsigned char));
		rcMarkWalkableTriangles(editor->build_context_, m_cfg.walkableSlopeAngle,
			verts, nverts, ctris, nctris, m_triareas);

		if (!rcRasterizeTriangles(editor->build_context_, verts, nverts, ctris, m_triareas, nctris, *m_solid, m_cfg.walkableClimb))
			return 0;
	}


	// Once all geometry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	if (editor->build_setting_.filterLowHangingObstacles)
		rcFilterLowHangingWalkableObstacles(editor->build_context_, m_cfg.walkableClimb, *m_solid);
	if (editor->build_setting_.filterLedgeSpans)
		rcFilterLedgeSpans(editor->build_context_, m_cfg.walkableHeight, m_cfg.walkableClimb, *m_solid);
	if (editor->build_setting_.filterWalkableLowHeightSpans)
		rcFilterWalkableLowHeightSpans(editor->build_context_, m_cfg.walkableHeight, *m_solid);

	// Compact the heightfield so that it is faster to handle from now on.
	// This will result more cache coherent data as well as the neighbours
	// between walkable cells will be calculated.
	rcCompactHeightfield* m_chf = rcAllocCompactHeightfield();
	if (!m_chf)
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'chf'.");
		return 0;
	}
	if (!rcBuildCompactHeightfield(editor->build_context_, m_cfg.walkableHeight, m_cfg.walkableClimb, *m_solid, *m_chf))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not build compact data.");
		return 0;
	}


	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(editor->build_context_, m_cfg.walkableRadius, *m_chf))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not erode.");
		return 0;
	}

	// (Optional) Mark areas.
	const ConvexVolume* vols = editor->input_geom_ ->getConvexVolumes();
	for (int i = 0; i < editor->input_geom_ ->getConvexVolumeCount(); ++i)
		rcMarkConvexPolyArea(editor->build_context_, vols[i].verts, vols[i].nverts, vols[i].hmin, vols[i].hmax, (unsigned char)vols[i].area, *m_chf);


	// Partition the heightfield so that we can use simple algorithm later to triangulate the walkable areas.
	// There are 3 martitioning methods, each with some pros and cons:
	// 1) Watershed partitioning
	//   - the classic Recast partitioning
	//   - creates the nicest tessellation
	//   - usually slowest
	//   - partitions the heightfield into nice regions without holes or overlaps
	//   - the are some corner cases where this method creates produces holes and overlaps
	//      - holes may appear when a small obstacles is close to large open area (triangulation can handle this)
	//      - overlaps may occur if you have narrow spiral corridors (i.e stairs), this make triangulation to fail
	//   * generally the best choice if you precompute the nacmesh, use this if you have large open areas
	// 2) Monotone partioning
	//   - fastest
	//   - partitions the heightfield into regions without holes and overlaps (guaranteed)
	//   - creates long thin polygons, which sometimes causes paths with detours
	//   * use this if you want fast navmesh generation
	// 3) Layer partitoining
	//   - quite fast
	//   - partitions the heighfield into non-overlapping regions
	//   - relies on the triangulation code to cope with holes (thus slower than monotone partitioning)
	//   - produces better triangles than monotone partitioning
	//   - does not have the corner cases of watershed partitioning
	//   - can be slow and create a bit ugly tessellation (still better than monotone)
	//     if you have large open areas with small obstacles (not a problem if you use tiles)
	//   * good choice to use for tiled navmesh with medium and small sized tiles

	if (editor->build_setting_.partitionType == SAMPLE_PARTITION_WATERSHED)
	{
		// Prepare for region partitioning, by calculating distance field along the walkable surface.
		if (!rcBuildDistanceField(editor->build_context_, *m_chf))
		{
			editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not build distance field.");
			return 0;
		}

		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions(editor->build_context_, *m_chf, m_cfg.borderSize, m_cfg.minRegionArea, m_cfg.mergeRegionArea))
		{
			editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not build watershed regions.");
			return 0;
		}
	}
	else if (editor->build_setting_.partitionType == SAMPLE_PARTITION_MONOTONE)
	{
		// Partition the walkable surface into simple regions without holes.
		// Monotone partitioning does not need distancefield.
		if (!rcBuildRegionsMonotone(editor->build_context_, *m_chf, m_cfg.borderSize, m_cfg.minRegionArea, m_cfg.mergeRegionArea))
		{
			editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not build monotone regions.");
			return 0;
		}
	}
	else // SAMPLE_PARTITION_LAYERS
	{
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildLayerRegions(editor->build_context_, *m_chf, m_cfg.borderSize, m_cfg.minRegionArea))
		{
			editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not build layer regions.");
			return 0;
		}
	}

	// Create contours.
	rcContourSet* m_cset = rcAllocContourSet();
	if (!m_cset)
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'cset'.");
		return 0;
	}
	if (!rcBuildContours(editor->build_context_, *m_chf, m_cfg.maxSimplificationError, m_cfg.maxEdgeLen, *m_cset))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not create contours.");
		return 0;
	}

	if (m_cset->nconts == 0)
	{
		return 0;
	}

	// Build polygon navmesh from the contours.
	rcPolyMesh* m_pmesh = rcAllocPolyMesh();
	if (!m_pmesh)
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'pmesh'.");
		return 0;
	}
	if (!rcBuildPolyMesh(editor->build_context_, *m_cset, m_cfg.maxVertsPerPoly, *m_pmesh))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could not triangulate contours.");
		return 0;
	}

	// Build detail mesh.
	rcPolyMeshDetail* m_dmesh = rcAllocPolyMeshDetail();
	if (!m_dmesh)
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'dmesh'.");
		return 0;
	}

	if (!rcBuildPolyMeshDetail(editor->build_context_, *m_pmesh, *m_chf,
		m_cfg.detailSampleDist, m_cfg.detailSampleMaxError,
		*m_dmesh))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildNavigation: Could build polymesh detail.");
		return 0;
	}


	unsigned char* navData = 0;
	int navDataSize = 0;
	if (m_cfg.maxVertsPerPoly <= DT_VERTS_PER_POLYGON)
	{
		if (m_pmesh->nverts >= 0xffff)
		{
			// The vertex indices are ushorts, and cannot point to more than 0xffff vertices.
			editor->build_context_->log(RC_LOG_ERROR, "Too many vertices per tile %d (max: %d).", m_pmesh->nverts, 0xffff);
			return 0;
		}

		// Update poly flags from areas.
		for (int i = 0; i < m_pmesh->npolys; ++i)
		{
			m_pmesh->flags[i] = AreaToFlag(&m_pmesh->areas[i]);
		}

		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = m_pmesh->verts;
		params.vertCount = m_pmesh->nverts;
		params.polys = m_pmesh->polys;
		params.polyAreas = m_pmesh->areas;
		params.polyFlags = m_pmesh->flags;
		params.polyCount = m_pmesh->npolys;
		params.nvp = m_pmesh->nvp;
		params.detailMeshes = m_dmesh->meshes;
		params.detailVerts = m_dmesh->verts;
		params.detailVertsCount = m_dmesh->nverts;
		params.detailTris = m_dmesh->tris;
		params.detailTriCount = m_dmesh->ntris;
		params.offMeshConVerts = editor->input_geom_ ->getOffMeshConnectionVerts();
		params.offMeshConRad = editor->input_geom_ ->getOffMeshConnectionRads();
		params.offMeshConDir = editor->input_geom_ ->getOffMeshConnectionDirs();
		params.offMeshConAreas = editor->input_geom_ ->getOffMeshConnectionAreas();
		params.offMeshConFlags = editor->input_geom_ ->getOffMeshConnectionFlags();
		params.offMeshConUserID = editor->input_geom_ ->getOffMeshConnectionId();
		params.offMeshConCount = editor->input_geom_ ->getOffMeshConnectionCount();
		params.walkableHeight = editor->build_setting_.agentHeight;
		params.walkableRadius = editor->build_setting_.agentRadius;
		params.walkableClimb = editor->build_setting_.agentMaxClimb;
		params.tileX = tx;
		params.tileY = ty;
		params.tileLayer = 0;
		rcVcopy(params.bmin, m_pmesh->bmin);
		rcVcopy(params.bmax, m_pmesh->bmax);
		params.cs = m_cfg.cs;
		params.ch = m_cfg.ch;
		params.buildBvTree = true;

		if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
		{
			editor->build_context_->log(RC_LOG_ERROR, "Could not build Detour navmesh.");
			return 0;
		}
	}
	m_tileMemUsage = navDataSize / 1024.0f;

	editor->build_context_->stopTimer(RC_TIMER_TOTAL);

	// Show performance stats.
	duLogBuildTimes(*editor->build_context_, editor->build_context_->getAccumulatedTime(RC_TIMER_TOTAL));
	editor->build_context_->log(RC_LOG_PROGRESS, ">> Polymesh: %d vertices  %d polygons", m_pmesh->nverts, m_pmesh->npolys);

	m_tileBuildTime = editor->build_context_->getAccumulatedTime(RC_TIMER_TOTAL) / 1000.0f;

	dataSize = navDataSize;
	
	delete[] m_triareas;
	m_triareas = 0;
	rcFreeHeightField(m_solid);
	m_solid = 0;
	rcFreeCompactHeightfield(m_chf);
	m_chf = 0;
	rcFreeContourSet(m_cset);
	m_cset = 0;
	rcFreePolyMesh(m_pmesh);
	m_pmesh = 0;
	rcFreePolyMeshDetail(m_dmesh);
	m_dmesh = 0;

	return navData;
#else
	return (unsigned char*)0;
#endif
}



bool tile_mesh_build(struct Editor* editor)
{
	if (!editor) return false;

	if (!editor->input_geom_ || !editor->input_geom_->getMesh())
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildTiledNavigation: No vertices and triangles.");
		return false;
	}

	dtFreeNavMesh(editor->nav_mesh_);
	editor->nav_mesh_ = dtAllocNavMesh();

	if (!editor->nav_mesh_)
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildTiledNavigation: Could not allocate navmesh.");
		return false;
	}

	dtNavMeshParams params;
	rcVcopy(params.orig, editor->input_geom_->getNavMeshBoundsMin());
	params.tileWidth = editor->build_setting_.tileSize * editor->build_setting_.cellSize;
	params.tileHeight = editor->build_setting_.tileSize * editor->build_setting_.cellSize;
	params.maxTiles = editor->build_setting_.maxTiles;
	params.maxPolys = editor->build_setting_.maxPolysPerTile;

	dtStatus status;

	status = editor->nav_mesh_->init(&params);
	if (dtStatusFailed(status))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildTiledNavigation: Could not init navmesh.");
		return false;
	}

	status = editor->nav_mesh_query_->init(editor->nav_mesh_, 2048);
	if (dtStatusFailed(status))
	{
		editor->build_context_->log(RC_LOG_ERROR, "buildTiledNavigation: Could not init Detour navmesh query");
		return false;
	}

	//--------------------build all-------------------------
	if (!editor->input_geom_) return false;
	if (!editor->nav_mesh_) return false;

	const float* bmin = editor->input_geom_->getNavMeshBoundsMin();
	const float* bmax = editor->input_geom_->getNavMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, editor->build_setting_.cellSize, &gw, &gh);
	const int ts = (int)editor->build_setting_.tileSize;
	const int tw = (gw + ts - 1) / ts;
	const int th = (gh + ts - 1) / ts;
	const float tcs = editor->build_setting_.tileSize*editor->build_setting_.cellSize;

	// Start the build process.
	editor->build_context_->startTimer(RC_TIMER_TEMP);

	float lastBuiltTileBmin[3];
	float lastBuiltTileBmax[3];
	memset(lastBuiltTileBmin, 0, sizeof(lastBuiltTileBmin));
	memset(lastBuiltTileBmax, 0, sizeof(lastBuiltTileBmax));

	for (int y = 0; y < th; ++y)
	{
		for (int x = 0; x < tw; ++x)
		{
			lastBuiltTileBmin[0] = bmin[0] + x*tcs;
			lastBuiltTileBmin[1] = bmin[1];
			lastBuiltTileBmin[2] = bmin[2] + y*tcs;

			lastBuiltTileBmax[0] = bmin[0] + (x + 1)*tcs;
			lastBuiltTileBmax[1] = bmax[1];
			lastBuiltTileBmax[2] = bmin[2] + (y + 1)*tcs;

			int dataSize = 0;
			unsigned char* data = buildTileMesh(editor,x, y, lastBuiltTileBmin, lastBuiltTileBmax, dataSize);
			if (data)
			{
				// Remove any previous data (navmesh owns and deletes the data).
				editor->nav_mesh_->removeTile(editor->nav_mesh_->getTileRefAt(x, y, 0), 0, 0);
				// Let the navmesh own the data.
				dtStatus status = editor->nav_mesh_->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, 0);
				if (dtStatusFailed(status))
					dtFree(data);
			}
		}
	}

	// Start the build process.	
	editor->build_context_->stopTimer(RC_TIMER_TEMP);

	return true;
}

dtNavMesh* tile_mesh_save_load(const char* path)
{
	FILE* fp = fopen(path, "rb");
	if (!fp) return 0;

	// Read header.
	NavMeshSetHeader header;
	size_t readLen = fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	if (readLen != 1)
	{
		fclose(fp);
		return 0;
	}
	if (header.magic != NAVMESHSET_MAGIC)
	{
		fclose(fp);
		return 0;
	}
	if (header.version != NAVMESHSET_VERSION)
	{
		fclose(fp);
		return 0;
	}

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh)
	{
		fclose(fp);
		return 0;
	}
	dtStatus status = mesh->init(&header.params);
	if (dtStatusFailed(status))
	{
		fclose(fp);
		return 0;
	}

	// Read tiles.
	for (int i = 0; i < header.numTiles; ++i)
	{
		NavMeshTileHeader tileHeader;
		readLen = fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (readLen != 1)
		{
			fclose(fp);
			return 0;
		}

		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		readLen = fread(data, tileHeader.dataSize, 1, fp);
		if (readLen != 1)
		{
			fclose(fp);
			return 0;
		}
		mesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);
	}
	fclose(fp);

	return mesh;
}

void tile_mesh_save(char* path,  dtNavMesh* mesh)
{
	if (!mesh) return;

	FILE* fp = fopen(path, "wb");
	if (!fp)
		return;

	// Store header.
	NavMeshSetHeader header;
	header.magic = NAVMESHSET_MAGIC;
	header.version = NAVMESHSET_VERSION;
	header.numTiles = 0;
	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || !tile->dataSize) continue;
		header.numTiles++;
	}
	memcpy(&header.params, mesh->getParams(), sizeof(dtNavMeshParams));
	fwrite(&header, sizeof(NavMeshSetHeader), 1, fp);

	// Store tiles.
	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || !tile->dataSize) continue;

		NavMeshTileHeader tileHeader;
		tileHeader.tileRef = mesh->getTileRef(tile);
		tileHeader.dataSize = tile->dataSize;
		fwrite(&tileHeader, sizeof(tileHeader), 1, fp);

		fwrite(tile->data, tile->dataSize, 1, fp);
	}

	fclose(fp);
}


void get_tile_render_info(dtNavMesh& mesh,dtMeshTile* tile, std::list<MyPoly> & my_ploy)
{
	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		const dtPoly* p = &tile->polys[i];
		if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)	// Skip off-mesh links.
			continue;

		const dtPolyDetail* pd = &tile->detailMeshes[i];

		MyPoly poly;
		poly.org_poly = (void*)p;
		poly.vertex_num = pd->triCount * 3;
		poly.index_num = pd->triCount * 3;

		poly.vertexs = (MyVertex*)malloc(sizeof(MyVertex)*poly.vertex_num);
		poly.indexs = (int*)malloc(sizeof(int)*poly.index_num);
		poly.area = p->areaAndtype;

		int index_count = 0;
		for (int j = 0; j < pd->triCount; ++j)
		{
			const unsigned char* t = &tile->detailTris[(pd->triBase + j) * 4];
			for (int k = 0; k < 3; ++k)
			{
				float* pos = (float*)0;
				if (t[k] < p->vertCount)
				{
					//vertex(&tile->verts[p->verts[t[k]] * 3]);
					pos = &tile->verts[p->verts[t[k]] * 3];
				}
				else
				{
					//vertex(&tile->detailVerts[(pd->vertBase + t[k] - p->vertCount) * 3]);
					pos = &tile->detailVerts[(pd->vertBase + t[k] - p->vertCount) * 3];
				}
					
				poly.vertexs[j*3+k].v[0] = pos[0];
				poly.vertexs[j*3+k].v[1] = pos[1];
				poly.vertexs[j*3+k].v[2] = pos[2];

				poly.indexs[index_count] = index_count;
				++index_count;
			}
		}
		my_ploy.push_back(poly);
	}
}

void tile_mesh_get_render_info(dtNavMesh* mesh, MyNavmeshRenderInfo* info)
{
	std::list<MyPoly> polys;
	if (!mesh || !info) return;

	for (int i = 0; i < info->poly_num; ++i)
	{
		MyPoly& poly = info->polys[i];
		if(poly.vertexs)
		free(poly.vertexs);
		if(poly.indexs)
		free(poly.indexs);
	}
	if(info->polys)
		free(info->polys);
	info->poly_num = 0;

	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		dtMeshTile* tile = mesh->getTile(i);
		if (!tile->header) continue;
		get_tile_render_info(*mesh,tile, polys);
	}

	info->poly_num = polys.size();
	if (info->poly_num == 0)
	{
		return;
	}

	info->polys = (MyPoly*)malloc(sizeof(MyPoly)*info->poly_num);
	std::list<MyPoly>::iterator iter = polys.begin();
	int i = 0;
	for (; iter != polys.end(); ++iter)
	{
		info->polys[i++] = *iter;
	}
}
