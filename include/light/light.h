/*  Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __LIGHT_LIGHT_H__
#define __LIGHT_LIGHT_H__

#include <common/cmdlib.h>
#include <common/mathlib.h>
#include <common/bspfile.h>
#include <common/log.h>
#include <common/threads.h>
#include <light/litfile.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON_EPSILON    0.1
#define ANGLE_EPSILON 0.001

typedef struct {
    vec3_t normal;
    vec_t dist;
} plane_t;

/*
 * Convenience functions TestLight and TestSky will test against all shadow
 * casting bmodels and self-shadow the model 'self' if self != NULL. Returns
 * true if sky or light is visible, respectively.
 */
qboolean TestSky(const vec3_t start, const vec3_t dirn, const dmodel_t *self);
qboolean TestLight(const vec3_t start, const vec3_t stop, const dmodel_t *self);
qboolean DirtTrace(const vec3_t start, const vec3_t stop, const dmodel_t *self, vec3_t hitpoint_out, plane_t *hitplane_out, const bsp2_dface_t **face_out);

int
SampleTexture(const bsp2_dface_t *face, const bsp2_t *bsp, const vec3_t point);
    
typedef struct {
    vec_t light;
    vec3_t color;
    vec3_t direction;
} lightsample_t;

typedef struct {
    const dmodel_t *model;
    qboolean shadowself;
    lightsample_t minlight;
    char minlight_exclude[16]; /* texture name to exclude from minlight */
    float lightmapscale;
    vec3_t offset;
    qboolean nodirt;
    vec_t phongangle;
} modelinfo_t;

typedef struct sun_s {
    vec3_t sunvec;
    lightsample_t sunlight;
    struct sun_s *next;
    qboolean dirt;
    float anglescale;
} sun_t;

/* for vanilla this would be 18. some engines allow higher limits though, which will be needed if we're scaling lightmap resolution. */
/*with extra sampling, lit+lux etc, we need at least 46mb stack space per thread. yes, that's a lot. on the plus side, it doesn't affect bsp complexity (actually, can simplify it a little)*/
#define MAXDIMENSION (255+1)

/* Allow space for 4x4 oversampling */
//#define SINGLEMAP (MAXDIMENSION*MAXDIMENSION*4*4)

typedef struct {
    vec3_t data[3];     /* permuted 3x3 matrix */
    int row[3];         /* row permutations */
    int col[3];         /* column permutations */
} pmatrix3_t;
    
typedef struct {
    pmatrix3_t transform;
    const texinfo_t *texinfo;
    vec_t planedist;
} texorg_t;
    
/*Warning: this stuff needs explicit initialisation*/
typedef struct {
    const modelinfo_t *modelinfo;
    const bsp2_dface_t *face;
    /* these take precedence the values in modelinfo */
    lightsample_t minlight;
    qboolean nodirt;
    
    plane_t plane;
    vec3_t snormal;
    vec3_t tnormal;
    
    /* 16 in vanilla. engines will hate you if this is not power-of-two-and-at-least-one */
    float lightmapscale;
    qboolean curved; /*normals are interpolated for smooth lighting*/
    
    int texmins[2];
    int texsize[2];
    vec_t exactmid[2];
    vec3_t midpoint;
    
    int numpoints;
    vec3_t *points; // malloc'ed array of numpoints
    vec3_t *normals; // malloc'ed array of numpoints
    bool *occluded; // malloc'ed array of numpoints
    
    /*
     raw ambient occlusion amount per sample point, 0-1, where 1 is
     fully occluded. dirtgain/dirtscale are not applied yet
     */
    vec_t *occlusion; // malloc'ed array of numpoints
    
    /*
     pvs for the entire light surface. generated by ORing together
     the pvs at each of the sample points
     */
    byte *pvs;
    bool skyvisible;
    
    /* for sphere culling */
    vec3_t origin;
    vec_t radius;

    // for radiosity
    vec3_t radiosity; // direct light
    vec3_t texturecolor;
    vec3_t indirectlight; // indirect light
    
    /* stuff used by CalcPoint */
    vec_t starts, startt, st_step;
    texorg_t texorg;
    int width, height;
} lightsurf_t;

typedef struct {
    int style;
    lightsample_t *samples; // malloc'ed array of numpoints   //FIXME: this is stupid, we shouldn't need to allocate extra data here for -extra4
} lightmap_t;

struct ltface_ctx
{
    const bsp2_t *bsp;
    lightsurf_t lightsurf;
    lightmap_t lightmaps[MAXLIGHTMAPS + 1];
    lightmap_t lightmaps_bounce1[MAXLIGHTMAPS + 1];
};

extern struct ltface_ctx *ltface_ctxs;

/* bounce lights */
#if 0
typedef struct {
    vec3_t pos;
    vec3_t color;
    vec3_t surfnormal;
    vec_t area;
    const bsp2_dleaf_t *leaf;
} bouncelight_t;
    
extern const bouncelight_t *bouncelights;
extern int numbouncelights;
#endif
extern byte thepalette[768];
    
/* tracelist is a null terminated array of BSP models to use for LOS tests */
extern const modelinfo_t *const *tracelist;
extern const modelinfo_t *const *selfshadowlist;

void LightFaceInit(const bsp2_t *bsp, struct ltface_ctx *ctx);
void LightFaceShutdown(struct ltface_ctx *ctx);
const modelinfo_t *ModelInfoForFace(const bsp2_t *bsp, int facenum);
void LightFace(bsp2_dface_t *face, facesup_t *facesup, const modelinfo_t *modelinfo, struct ltface_ctx *ctx);
void LightFaceIndirect(bsp2_dface_t *face, facesup_t *facesup, const modelinfo_t *modelinfo, struct ltface_ctx *ctx);
void FinishLightFace(bsp2_dface_t *face, facesup_t *facesup, const modelinfo_t *modelinfo, struct ltface_ctx *ctx);
void MakeTnodes(const bsp2_t *bsp);

/* access the final phong-shaded vertex normal */
const vec_t *GetSurfaceVertexNormal(const bsp2_t *bsp, const bsp2_dface_t *f, const int v);

const bsp2_dface_t *
Face_EdgeIndexSmoothed(const bsp2_t *bsp, const bsp2_dface_t *f, const int edgeindex);
    
extern float scaledist;
extern float rangescale;
extern float global_anglescale;
extern float fadegate;
extern int softsamples;
extern float lightmapgamma;
extern const vec3_t vec3_white;
extern float surflight_subdivide;
extern int sunsamples;

extern qboolean addminlight;
extern lightsample_t minlight;

extern sun_t *suns;

/* dirt */

extern qboolean dirty;          // should any dirtmapping take place?
extern qboolean dirtDebug;
extern int dirtMode;
extern float dirtDepth;
extern float dirtScale;
extern float dirtGain;
extern float dirtAngle;

extern qboolean globalDirt;     // apply dirt to all lights (unless they override it)?
extern qboolean minlightDirt;   // apply dirt to minlight?

extern qboolean dirtModeSetOnCmdline;
extern qboolean dirtDepthSetOnCmdline;
extern qboolean dirtScaleSetOnCmdline;
extern qboolean dirtGainSetOnCmdline;
extern qboolean dirtAngleSetOnCmdline;

/* bounce */

extern qboolean bounce;
extern qboolean bouncedebug;
extern vec_t bouncescale;

/*
 * Return space for the lightmap and colourmap at the same time so it can
 * be done in a thread-safe manner.
 */
void GetFileSpace(byte **lightdata, byte **colordata, byte **deluxdata, int size);

extern byte *filebase;
extern byte *lit_filebase;
extern byte *lux_filebase;

extern int oversample;
extern int write_litfile;
extern int write_luxfile;
extern qboolean onlyents;
extern qboolean parse_escape_sequences;
extern qboolean scaledonly;
extern uint32_t *extended_texinfo_flags;
extern qboolean novis;
    
void SetupDirt();

/* Used by fence texture sampling */
void WorldToTexCoord(const vec3_t world, const texinfo_t *tex, vec_t coord[2]);

vec_t
TriangleArea(const vec3_t v0, const vec3_t v1, const vec3_t v2);
    
extern qboolean testFenceTextures;
extern qboolean surflight_dump;
extern qboolean phongDebug;

extern char mapfilename[1024];

void
PrintFaceInfo(const bsp2_dface_t *face, const bsp2_t *bsp);
    
const char *
Face_TextureName(const bsp2_t *bsp, const bsp2_dface_t *face);

void
Face_MakeInwardFacingEdgePlanes(const bsp2_t *bsp, const bsp2_dface_t *face, plane_t *out);

plane_t Face_Plane(const bsp2_t *bsp, const bsp2_dface_t *f);
void Face_Normal(const bsp2_t *bsp, const bsp2_dface_t *f, vec3_t norm);

/* vis testing */
const bsp2_dleaf_t *Light_PointInLeaf( const bsp2_t *bsp, const vec3_t point );
int Light_PointContents( const bsp2_t *bsp, const vec3_t point );
bool Mod_LeafPvs(const bsp2_t *bsp, const bsp2_dleaf_t *leaf, byte *out);
int DecompressedVisSize(const bsp2_t *bsp);
bool Pvs_LeafVisible(const bsp2_t *bsp, const byte *pvs, const bsp2_dleaf_t *leaf);
    
/* PVS index (light.cc) */
bool Leaf_HasSky(const bsp2_t *bsp, const bsp2_dleaf_t *leaf);
const bsp2_dleaf_t **Face_CopyLeafList(const bsp2_t *bsp, const bsp2_dface_t *face);    
qboolean VisCullEntity(const bsp2_t *bsp, const byte *pvs, const bsp2_dleaf_t *entleaf);
    
#ifdef __cplusplus
}
#endif

#endif /* __LIGHT_LIGHT_H__ */
