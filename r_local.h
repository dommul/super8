/*  Copyright (C) 1996-1997 Id Software, Inc.
    Copyright (C) 1999-2012 other authors as noted in code comments

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.   */
// r_local.h -- private refresh defs

#include "r_shared.h"

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle

#define BMODEL_FULLY_CLIPPED	0x10 // value returned by R_BmodelCheckBBox ()
									 //  if bbox is trivially rejected

#define FRAME_DELTA (1.0 / 72.0) //qb: constant physics framerate

//===========================================================================
// viewmodel lighting

typedef struct {
	int			ambientlight;
	int			shadelight;
	float		*plightvec;
} alight_t;

//===========================================================================
// clipped bmodel edges

typedef struct bedge_s
{
	mvertex_t		*v[2];
	struct bedge_s	*pnext;
} bedge_t;

typedef struct {
	float	fv[3];		// viewspace x, y
} auxvert_t;

//===========================================================================
extern vrect_t	scr_vrect;

extern cvar_t	r_draworder;
extern cvar_t	r_speeds;
extern cvar_t	r_timegraph;
extern cvar_t	r_graphheight;
extern cvar_t	r_clearcolor;
extern cvar_t	r_waterwarp;
extern cvar_t	r_fullbright;
extern cvar_t	r_drawentities;
extern cvar_t	r_aliasstats;
extern cvar_t	r_dspeeds;
extern cvar_t	r_drawflat;
extern cvar_t	r_ambient;
extern cvar_t	r_reportsurfout;
extern cvar_t	r_maxsurfs;
extern cvar_t	r_numsurfs;
extern cvar_t	r_reportedgeout;
extern cvar_t	r_maxedges;
extern cvar_t	r_numedges;
extern cvar_t	r_skyname; // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
extern cvar_t	r_skyalpha; // Manoel Kasimier - translucent sky
//extern cvar_t	sw_stipplealpha; // Manoel Kasimier
//extern cvar_t	r_sprite_addblend; // Manoel Kasimier
extern cvar_t   r_nolerp_list; //qb: //from FQ
extern cvar_t   r_novis; //qb: implementation from MarkV

extern cvar_t	r_coloredlights; //qb: at last!
extern cvar_t  r_saturation;

extern cvar_t   r_fog; //qb: fog!  not TOO bad...
extern int      r_fviews; //qb: number of fisheye views

#define XCENTERING	(1.0 / 2.0)
#define YCENTERING	(1.0 / 2.0)

#define CLIP_EPSILON		0.001

#define BACKFACE_EPSILON	0.01

//===========================================================================

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct clipplane_s
{
	vec3_t		normal;
	float		dist;
	struct		clipplane_s	*next;
	byte		leftedge;
	byte		rightedge;
	byte		reserved[2];
} clipplane_t;

extern	clipplane_t	view_clipplanes[4];

//=============================================================================

void R_RenderWorld (void);

//=============================================================================

extern	mplane_t	screenedge[4];

extern	vec3_t	r_origin;

extern	vec3_t	r_entorigin;

extern	float	screenAspect;
extern	float	verticalFieldOfView;
extern	float	xOrigin, yOrigin;

extern	int		r_visframecount;

//=============================================================================

extern int	vstartscan;


void R_ClearPolyList (void);
void R_DrawPolyList (void);

//
// current entity info
//
extern	qboolean		insubmodel;
extern	vec3_t			r_worldmodelorg;


void R_DrawSprite (void);
void R_RenderFace (msurface_t *fa, int clipflags);
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf);
void R_TransformPlane (mplane_t *p, float *normal, float *dist);
void R_TransformFrustum (void);
void R_SetSkyFrame (void);
void R_DrawSurfaceBlock16 (void);
void R_DrawSurfaceBlock8 (void);
texture_t *R_TextureAnimation (texture_t *base);

void R_GenSkyTile (void *pdest);
void R_GenSkyTile16 (void *pdest);
void R_DrawSubmodelPolygons (model_t *pmodel, int clipflags, int alphamask);
void R_DrawSolidClippedSubmodelPolygons (model_t *pmodel, int alphamask);

void R_AddPolygonEdges (emitpoint_t *pverts, int numverts, int miplevel);
surf_t *R_GetSurf (void);
void R_AliasDrawModel (/* alight_t *plighting */); // Manoel Kasimier - Model Interpolation - edited
void R_BeginEdgeFrame (void);
void R_ScanEdges (void);
void D_DrawSurfaces (void);
void R_InsertNewEdges (edge_t *edgestoadd, edge_t *edgelist);
void R_StepActiveU (edge_t *pedge);
void R_RemoveEdges (edge_t *pedge);

void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv,
        stvert_t *pstverts);
void R_AliasSetUpTransform (int trivial_accept);
void R_AliasTransformVector (vec3_t in, vec3_t out);
void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av,
                                trivertx_t *pverts, stvert_t *pstverts);
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av);
// Manoel Kasimier - model interpolation - begin
void R_AliasSetUpBlendedTransform (int trivial_accept);
void R_AliasTransformAndProjectFinalBlendedVerts (finalvert_t *fv,
        stvert_t *pstverts);
void R_AliasTransformFinalBlendedVert (finalvert_t *fv, auxvert_t *av,
                                       trivertx_t *pverts1, trivertx_t *pverts2, stvert_t *pstverts);

extern void R_Surf8Start (void);
extern void R_Surf8End (void);
extern void R_Surf16Start (void);
extern void R_Surf16End (void);
extern void R_EdgeCodeStart (void);
extern void R_EdgeCodeEnd (void);

extern void R_RotateBmodel (void);

extern int	c_faceclip;
extern int	r_polycount;
extern int	r_wholepolycount;

extern	model_t		*cl_worldmodel;

extern int		*pfrustum_indexes[4];

// !!! if this is changed, it must be changed in asm_draw.h too !!!
#define	NEAR_CLIP	0.01

extern int			ubasestep, errorterm, erroradjustup, erroradjustdown;
extern int			vstartscan;

extern fixed16_t	sadjust, tadjust;
extern fixed16_t	bbextents, bbextentt;

#define MAXBVERTINDEXES	4096 //qb: was 1000	// new clipped vertices when clipping bmodels
								//  to the world BSP
extern mvertex_t	*r_ptverts, *r_ptvertsmax;

extern vec3_t			sbaseaxis[3], tbaseaxis[3];
extern float			entity_rotation[3][3];

extern int		reinit_surfcache;

extern int		r_currentkey;
extern int		r_currentbkey;

typedef struct btofpoly_s {
	int			clipflags;
	msurface_t	*psurf;
} btofpoly_t;

extern int			numbtofpolys;
extern btofpoly_t	*pbtofpolys;

void	R_InitTurb (void);
//qb: remove all this     void	R_ZDrawSubmodelPolys (model_t *clmodel);

//=========================================================
// Alias models
//=========================================================

#define MAXALIASVERTS		4096 	//qb: 65535 per qsb - was 1024
#define ALIAS_Z_CLIP_PLANE	5

extern int				numverts;
extern int				a_skinwidth;
extern mtriangle_t		*ptriangles;
extern int				numtriangles;
extern aliashdr_t		*paliashdr;
extern mdl_t			*pmdl;
extern float			leftclip, topclip, rightclip, bottomclip;
extern int				r_acliptype;
extern finalvert_t		*pfinalverts;
extern auxvert_t		*pauxverts;
extern int	r_drawoutlines; // Manoel Kasimier - EF_CELSHADING

qboolean R_AliasCheckBBox (void);

//=========================================================
// turbulence stuff

#define	AMP		8*0x10000
#define	AMP2	3
#define	SPEED	20

//=========================================================
// particle stuff

void R_DrawParticles (void);
void R_InitParticles (void);
void R_ClearParticles (void);
void R_ReadPointFile_f (void);

void R_PushDlights (struct mnode_s *headnode); //qb: moved from render.h

extern int		r_amodels_drawn;
extern edge_t	*auxedges;
extern unsigned int		r_numallocatededges;
extern edge_t	*r_edges, *edge_p, *edge_max;

extern	edge_t	*newedges[MAXHEIGHT];
extern	edge_t	*removeedges[MAXHEIGHT];

extern	int	screenwidth;

// FIXME: make stack vars when debugging done
extern	edge_t	edge_head;
extern	edge_t	edge_tail;
extern	edge_t	edge_aftertail;
extern int		r_bmodelactive;
extern vrect_t	*pconupdate;

extern float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;
extern float		r_aliastransition, r_resfudge;

extern int		r_outofsurfaces;
extern int		r_outofedges;

extern mvertex_t	*r_pcurrentvertbase;
extern int			r_maxvalidedgeoffset;

void R_AliasClipTriangle (mtriangle_t *ptri);

extern float	r_time1;
extern float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
extern float	se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;
extern int		r_frustum_indexes[4*6];
extern unsigned int		r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
extern qboolean	r_surfsonstack;
extern cshift_t	cshift_water;
extern qboolean	r_dowarpold, r_viewchanged;

extern mleaf_t	*r_viewleaf, *r_oldviewleaf;

extern vec3_t	r_emins, r_emaxs;
extern mnode_t	*r_pefragtopnode;
extern int		r_clipflags;
//qb: silly!  extern int		r_dlightframecount;
//extern qboolean	r_fov_greater_than_90;
extern byte *pointcolormap; //qb: palettized color

// mankrip - hi-res waterwarp - begin
extern int *intsintable_x ,   *intsintable_y,   *warpcolumn,   *warprow;
extern byte *turbdest,   *turbsrc;
extern float  uwarpscale, vwarpscale;
// mankrip - hi-res waterwarp - end

//qb: fog
extern float fog_density, fog_red, fog_green, fog_blue;
extern float old_density, old_red, old_green, old_blue;
extern float fade_time; //duration of fade
extern float fade_done; //time when fade will be done
void GrabFogmap (void);

void R_BuildLightmaps(void); //qb: ftestain
void GrabAlphamap (void); //qb: color lookup tables
void GrabAlpha50map (void);  //qb:
void GrabAdditivemap (void); //qb:
void GrabColormap (void); //qb:
void GrabFencemap (void); //qb:
void V_BonusFlash_f (void);  //qb: add prototype to appease Cygwin (swc compiling)


void R_StoreEfrags (efrag_t **ppefrag);
void R_TimeRefresh_f (void);
void R_TimeGraph (void);
void R_PrintAliasStats (void);
void R_PrintTimes (void);
void R_PrintDSpeeds (void);
void R_AnimateLight (void);
int R_LightPoint (vec3_t p, int lightpoint);
void R_SetupFrame (void); //qb: things to do once for fisheye
void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1);
void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip);
void R_SplitEntityOnNode2 (mnode_t *node);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
void R_InitSkyBox (void); // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
qboolean R_LoadSkybox (char *name); // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
void Placebo_f (void); //qb
int BestColor (int r, int g, int b, int start, int stop); //qb:

void Fog_FogCommand_f (void); //qb
void R_NoLerpList_f (void); //qb:  from johnfitz
