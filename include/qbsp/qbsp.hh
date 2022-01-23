/*
    Copyright (C) 1996-1997  Id Software, Inc.
    Copyright (C) 1997       Greg Lewis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
// qbsp.h

#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <array>
#include <optional>

#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdint>
//#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <common/bspfile.hh>
#include <common/aabb.hh>

enum texcoord_style_t
{
    TX_QUAKED = 0,
    TX_QUARK_TYPE1 = 1,
    TX_QUARK_TYPE2 = 2,
    TX_VALVE_220 = 3,
    TX_BRUSHPRIM = 4
};

enum class conversion_t
{
    quake,
    quake2,
    valve,
    bp
};

class options_t
{
public:
    bool fNofill = false;
    bool fNoclip = false;
    bool fNoskip = false;
    bool fNodetail = false;
    bool fOnlyents = false;
    bool fConvertMapFormat = false;
    conversion_t convertMapFormat = conversion_t::quake;
    bool fVerbose = true;
    bool fAllverbose = false;
    bool fSplitspecial = false;
    bool fSplitturb = false;
    bool fSplitsky = false;
    bool fTranswater = true;
    bool fTranssky = false;
    bool fOldaxis = true;
    bool fNoverbose = false;
    bool fNopercent = false;
    bool forceGoodTree = false;
    bool fixRotateObjTexture = true;
    bool fbspx_brushes = false;
    bool fNoTextures = false;
    const bspversion_t *target_version = &bspver_q1;
    const gamedef_t *target_game = target_version->game;
    int dxSubdivide = 240;
    int dxLeakDist = 2;
    int maxNodeSize = 1024;
    /**
     * if 0 (default), use maxNodeSize for deciding when to switch to midsplit bsp heuristic.
     *
     * if 0 < midsplitSurfFraction <=1, switch to midsplit if the node contains more than this fraction of the model's
     * total surfaces. Try 0.15 to 0.5. Works better than maxNodeSize for maps with a 3D skybox (e.g. +-128K unit maps)
     */
    float midsplitSurfFraction = 0.f;
    std::filesystem::path szMapName;
    std::filesystem::path szBSPName;

    struct wadpath
    {
        std::filesystem::path path;
        bool external; // wads from this path are not to be embedded into the bsp, but will instead require the engine
                       // to load them from elsewhere. strongly recommended for eg halflife.wad
    };

    std::vector<wadpath> wadPathsVec;
    vec_t on_epsilon = 0.0001;
    bool fObjExport = false;
    bool fOmitDetail = false;
    bool fOmitDetailWall = false;
    bool fOmitDetailIllusionary = false;
    bool fOmitDetailFence = false;
    bool fForcePRT1 = false;
    bool fTestExpand = false;
    bool fLeakTest = false;
    bool fContentHack = false;
    vec_t worldExtent = 65536.0f;
    bool fNoThreads = false;
    bool includeSkip = false;
    bool fNoTJunc = false;
};

extern options_t options;

/*
 * Clipnodes need to be stored as a 16-bit offset. Originally, this was a
 * signed value and only the positive values up to 32767 were available. Since
 * the negative range was unused apart from a few values reserved for flags,
 * this has been extended to allow up to 65520 (0xfff0) clipnodes (with a
 * suitably modified engine).
 */
#define MAX_BSP_CLIPNODES 0xfff0

// Various other geometry maximums
constexpr size_t MAXEDGES = 64;

// For brush.c, normal and +16 (?)
#define NUM_HULLS 2

// 0-2 are axial planes
// 3-5 are non-axial planes snapped to the nearest
#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2
#define PLANE_ANYX 3
#define PLANE_ANYY 4
#define PLANE_ANYZ 5

// planenum for a leaf (?)
constexpr int32_t PLANENUM_LEAF = -1;

/*
 * The quality of the bsp output is highly sensitive to these epsilon values.
 * Notes:
 * - T-junction calculations are sensitive to errors and need the various
 *   epsilons to be such that EQUAL_EPSILON < T_EPSILON < CONTINUOUS_EPSILON.
 *     ( TODO: re-check if CONTINUOUS_EPSILON is still directly related )
 */
#define ANGLEEPSILON 0.000001
#define ZERO_EPSILON 0.0001
#define DISTEPSILON 0.0001
#define POINT_EPSILON 0.0001
#define ON_EPSILON options.on_epsilon
#define EQUAL_EPSILON 0.0001
#define T_EPSILON 0.0002
#define CONTINUOUS_EPSILON 0.0005

// from q3map
#define MAX_WORLD_COORD (128 * 1024)
#define MIN_WORLD_COORD (-128 * 1024)
#define WORLD_SIZE (MAX_WORLD_COORD - MIN_WORLD_COORD)

// the exact bounding box of the brushes is expanded some for the headnode
// volume.  is this still needed?
#define SIDESPACE 24

// AllocMem types
enum
{
    WINDING,
    OTHER
};

#include <common/cmdlib.hh>
#include <common/mathlib.hh>
#include <qbsp/winding.hh>

struct mtexinfo_t
{
    texvecf vecs; /* [s/t][xyz offset] */
    int32_t miptex = 0;
    surfflags_t flags = {};
    int32_t value = 0; // Q2-specific
    std::optional<size_t> outputnum = std::nullopt; // nullopt until added to bsp

    constexpr auto as_tuple() const { return std::tie(vecs, miptex, flags, value); }

    constexpr bool operator<(const mtexinfo_t &other) const { return as_tuple() < other.as_tuple(); }

    constexpr bool operator>(const mtexinfo_t &other) const { return as_tuple() > other.as_tuple(); }
};

class mapentity_t;

struct face_t
{
    face_t *next;

    int planenum;
    int planeside; // which side is the front of the face
    int texinfo;
    twosided<contentflags_t> contents;
    twosided<int16_t> lmshift;

    mapentity_t *src_entity; // source entity
    face_t *original; // face on node
    std::optional<size_t> outputnumber; // only valid for original faces after
                                        // write surfaces
    bool touchesOccupiedLeaf; // internal use in outside.cc
    qvec3d origin;
    vec_t radius;

    std::vector<size_t> edges; // only filled in MakeFaceEdges
    winding_t w;
};

struct surface_t
{
    surface_t *next;
    int planenum;
    std::optional<size_t> outputplanenum; // only valid after WriteSurfacePlanes
    bool onnode; // true if surface has already been used
                 //   as a splitting node
    bool detail_separator; // true if ALL faces are detail
    face_t *faces; // links to all faces on either side of the surf

    // bounds of all the face windings; calculated via calculateInfo
    aabb3d bounds;
    // 1 if the surface has non-detail brushes; calculated via calculateInfo
    bool has_struct;
    // smallest lmshift of all faces; calculated via calculateInfo
    short lmshift;

    // calculate bounds & info
    inline void calculateInfo()
    {
        bounds = {};
        lmshift = std::numeric_limits<short>::max();

        for (const face_t *f = faces; f; f = f->next) {
            for (auto &contents : f->contents)
                if (!contents.is_valid(options.target_game, false))
                    FError("Bad contents in face: {}", contents.to_string(options.target_game));

            lmshift = min(f->lmshift.front, f->lmshift.back);

            has_struct = !((f->contents[0].extended | f->contents[1].extended) &
                (CFLAGS_DETAIL | CFLAGS_DETAIL_ILLUSIONARY | CFLAGS_DETAIL_FENCE | CFLAGS_WAS_ILLUSIONARY));

            bounds += f->w.bounds();

            Q_assert(!qv::emptyExact(bounds.size()));
        }
    }
};

// there is a node_t structure for every node and leaf in the bsp tree

struct brush_t;
struct portal_t;

struct node_t
{
    aabb3d bounds; // bounding volume, not just points inside

    // information for decision nodes
    int planenum; // -1 = leaf node
    int firstface; // decision node only
    int numfaces; // decision node only
    node_t *children[2]; // children[0] = front side, children[1] = back side of plane. only valid for decision nodes
    face_t *faces; // decision nodes only, list for both sides

    // information for leafs
    contentflags_t contents; // leaf nodes (0 for decision nodes)
    face_t **markfaces; // leaf nodes only, point to node faces
    portal_t *portals;
    int visleafnum; // -1 = solid
    int viscluster; // detail cluster for faster vis
    int outside_distance; // -1 = can't reach outside, 0 = first void node, >0 = distance from void, in number of portals
                          // used to write leak lines that take the shortest path to the void
    mapentity_t *occupant; // example occupant, for leak hunting
    bool detail_separator; // for vis portal generation. true if ALL faces on node, and on all descendant nodes/leafs,
                           // are detail.
    uint32_t firstleafbrush; // Q2
    uint32_t numleafbrushes;
    int32_t area;

    bool opaque() const;
};

#include <qbsp/brush.hh>
#include <qbsp/csg4.hh>
#include <qbsp/solidbsp.hh>
#include <qbsp/merge.hh>
#include <qbsp/surfaces.hh>
#include <qbsp/portals.hh>
#include <qbsp/region.hh>
#include <qbsp/writebsp.hh>
#include <qbsp/outside.hh>
#include <qbsp/map.hh>

int qbsp_main(int argc, const char **argv);
