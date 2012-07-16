/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl_main.c  -- client main loop

#include "quakedef.h"
#include "curl.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

int BestColor (int r, int g, int b, int start, int stop);

extern cvar_t	r_shadowhack; //qbism- engoo shadowhack
extern cvar_t   r_shadowhacksize; //qbism

// these two are not intended to be set directly
cvar_t	cl_name = {"_cl_name", "player", true};
cvar_t	cl_color = {"_cl_color", "22", true}; // edited

cvar_t	cl_vibration = {"cl_vibration","1", true}; // Manoel Kasimier
cvar_t	cl_showfps = {"cl_showfps","0"};	// 2001-11-31 FPS display by QuakeForge/Muff
cvar_t	cl_shownet = {"cl_shownet","0"};	// can be 0, 1, or 2
cvar_t	cl_nolerp = {"cl_nolerp","0"};
cvar_t	cl_maxfps = {"cl_maxfps", "60", true, true};

cvar_t	lookspring = {"lookspring","0", true};
cvar_t	lookstrafe = {"lookstrafe","0", true};
cvar_t	sensitivity = {"sensitivity","11", true}; // edited

cvar_t	m_pitch = {"m_pitch","0.022", true};
cvar_t	m_yaw = {"m_yaw","0.022", true};
cvar_t	m_forward = {"m_forward","1", true};
cvar_t	m_side = {"m_side","0.8", true};
cvar_t	m_look = {"m_look","1", true}; // Manoel Kasimier - m_look
cvar_t	cutscene = {"cutscene", "1"}; // Nehahra

#ifdef WEBDL    //qbism - sometimes works, needs more testing
cvar_t cl_web_download = {"cl_web_download", "1", true}; //qbism - R00k / Baker tute
cvar_t cl_web_download_url = {"cl_web_download_url", "http://qbism.com/_q1maps/", true};
#endif // WEBDL

client_static_t	cls;
client_state_t	cl;

efrag_t			cl_efrags[MAX_EFRAGS];
entity_t		cl_entities[MAX_EDICTS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];
shadow_t		cl_shadows[MAX_SHADOWS];

int				cl_numvisedicts;
entity_t		*cl_visedicts[MAX_VISEDICTS];

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
    int			i;

    if (!sv.active)
        Host_ClearMemory ();

// wipe the entire cl structure
    memset (&cl, 0, sizeof(cl));

    // If disconnect was missing, stop sounds here
    if (cls.state == ca_connected)
        S_StopAllSounds (true);

    SZ_Clear (&cls.message);

// clear other arrays
    memset (cl_entities, 0, sizeof(cl_entities));
    memset (cl_dlights, 0, sizeof(cl_dlights));
    memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
    memset (cl_temp_entities, 0, sizeof(cl_temp_entities));
    memset (cl_beams, 0, sizeof(cl_beams));

    memset (cl_static_entities, 0, sizeof(cl_static_entities));

    // allocate the efrags and chain together into a free list
    cl.free_efrags = cl_efrags;
    for (i=0 ; i<MAX_EFRAGS-1 ; i++)
        cl.free_efrags[i].entnext = &cl.free_efrags[i+1];
    cl.free_efrags[i].entnext = NULL;
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
    int i;
// stop sounds (especially looping!)
    S_StopAllSounds (true);

#ifdef WEBDL    //qbism - sometimes works, needs more testing
// We have to shut down webdownloading first
    if( cls.download.web )  //qbism - R00k / Baker tute
    {
        cls.download.disconnect = true;
        return;
    }
#endif //WEBDL

// ToChriS 1.67 - begin clear effects
//void CL_ClearCshifts (void)
    for (i = 0; i < NUM_CSHIFTS; i++)
        cl.cshifts[i].percent = 0;
// ToChriS 1.67 - end

// if running a local server, shut it down

// This makes sure ambient sounds remain silent
    cl.worldmodel = NULL;

    if (cls.demoplayback)
        CL_StopPlayback ();
    else if (cls.state == ca_connected)
    {
        if (cls.demorecording)
            CL_Stop_f ();

        Con_DPrintf ("Sending clc_disconnect\n");
        SZ_Clear (&cls.message);
        MSG_WriteChar(&cls.message, clc_disconnect);
        NET_SendUnreliableMessage (cls.netcon, &cls.message);
        SZ_Clear (&cls.message);
        NET_Close (cls.netcon);

        cls.state = ca_disconnected;
        if (sv.active)
            Host_ShutdownServer(false);
    }

    cls.demoplayback = cls.timedemo = false;
    cls.signon = 0;
    cl.intermission = 0; //qbism DEBUG - works OK? from Baker: So critical.  SCR_UpdateScreen uses this.
    // Manoel Kasimier - begin
    Vibration_Stop (0);
    Vibration_Stop (1);
    // Manoel Kasimier - end
}

void CL_Disconnect_f (void)
{
    CL_Disconnect ();
    if (sv.active)
        Host_ShutdownServer (false);
}


/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection (char *host)
{
    if (cls.state == ca_dedicated)
        return;

    if (cls.demoplayback)
        return;

    CL_Disconnect ();

    cls.netcon = NET_Connect (host);
    if (!cls.netcon)
        Host_Error ("CL_Connect: connect failed\n");
    Con_DPrintf ("CL_EstablishConnection: connected to %s\n", host);

    cls.demonum = -1;			// not in the demo loop now
    cls.state = ca_connected;
    cls.signon = 0;				// need all the signon messages before playing
    MSG_WriteByte (&cls.message, clc_nop); // ProQuake NAT Fix  //qbism thx to Baker
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply (void)
{
    char 	str[100 + MAX_MAPSTRING];

    Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

    switch (cls.signon)
    {
    case 1:
        MSG_WriteChar (&cls.message, clc_stringcmd);
        MSG_WriteString (&cls.message, "prespawn");
        break;

    case 2:
        MSG_WriteChar (&cls.message, clc_stringcmd);
        MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name.string));

        MSG_WriteChar (&cls.message, clc_stringcmd);
        MSG_WriteString (&cls.message, va("color %i %i\n", ((int)cl_color.value)>>4, ((int)cl_color.value)&15));

        MSG_WriteChar (&cls.message, clc_stringcmd);
        sprintf (str, "spawn %s", cls.spawnparms);
        MSG_WriteString (&cls.message, str);
        break;

    case 3:
        MSG_WriteChar (&cls.message, clc_stringcmd);
        MSG_WriteString (&cls.message, "begin");
        Cache_Report ();		// print remaining memory
        break;

    case 4:
        SCR_EndLoadingPlaque ();		// allow normal screen updates
        break;
    }
    Con_DPrintf ("CL_SignonReply %i sent:  %s\n", cls.signon, &cls.message);

}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
    char	str[MAX_QPATH];

    if (cls.demonum == -1)
        return;		// don't play demos

    SCR_BeginLoadingPlaque ();

    if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
    {
        cls.demonum = 0;
        if (!cls.demos[cls.demonum][0])
        {
            Con_Printf ("No demos listed with startdemos\n");
            cls.demonum = -1;
            return;
        }
    }

    sprintf (str,"playdemo %s\n", cls.demos[cls.demonum]);
    Cbuf_InsertText (str);
    cls.demonum++;
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f (void)
{
    entity_t	*ent;
    int			i;

    for (i=0,ent=cl_entities ; i<cl.num_entities ; i++,ent++)
    {
        if (!ent->model)
        {
            //   Con_Printf ("EMPTY\n"); //don't print empties
            continue;
        }
        Con_Printf ("%3i:",i);
        Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n"
                    ,ent->model->name,ent->frame, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2]);
    }
}


/*
===============
SetPal

Debugging tool, just flashes the screen
===============
*/
void SetPal (int i)
{
#if 0
    static int old;
    byte	pal[768];
    int		c;

    if (i == old)
        return;
    old = i;

    if (i==0)
        VID_SetPalette (host_basepal);
    else if (i==1)
    {
        for (c=0 ; c<768 ; c+=3)
        {
            pal[c] = 0;
            pal[c+1] = 255;
            pal[c+2] = 0;
        }
        VID_SetPalette (pal);
    }
    else
    {
        for (c=0 ; c<768 ; c+=3)
        {
            pal[c] = 0;
            pal[c+1] = 0;
            pal[c+2] = 255;
        }
        VID_SetPalette (pal);
    }
#endif
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight (int key)
{
    int		i;
    dlight_t	*dl;

// first look for an exact key match
    if (key)
    {
        dl = cl_dlights;
        for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
        {
            if (dl->key == key)
            {
                memset (dl, 0, sizeof(*dl));
                dl->key = key;
                dl->color = palmapnofb[60>>3][50>>3][12>>3];  //qbism dyncol - default  HACK
                return dl;
            }
        }
    }

// then look for anything else
    dl = cl_dlights;
    for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
    {
        if (dl->die < cl.time)
        {
            memset (dl, 0, sizeof(*dl));
            dl->key = key;
            dl->color = palmapnofb[60>>3][50>>3][12>>3];  //qbism dyncol - default
            return dl;
        }
    }

    dl = &cl_dlights[0];
    memset (dl, 0, sizeof(*dl));
    dl->key = key;
    dl->color = palmapnofb[60>>3][50>>3][12>>3];  //qbism dyncol - default
    return dl;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
    int			i;
    dlight_t	*dl;
    float		time;

    time = cl.time - cl.oldtime;

    dl = cl_dlights;
    for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
    {
        if (dl->die < cl.time || !dl->radius)
            continue;

        dl->radius -= time*dl->decay;
        if (dl->radius < 0)
            dl->radius = 0;
    }

    /*    shd = cl_shadows;
        for (i=0 ; i<MAX_SHADOWS ; i++, dl++)
        {
            if (shd->die < cl.time || !shd->radius)
                continue;

            shd->radius -= time*1;
            if (shd->radius < 0)
                shd->radius = 0;
        }
        */
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float	CL_LerpPoint (void)
{
    float	f, frac;

    f = cl.mtime[0] - cl.mtime[1];

    if (!f || cl_nolerp.value || cls.timedemo || sv.active)
    {
        cl.time = cl.mtime[0];
        return 1;
    }

    if (f > 0.1)
    {
        // dropped packet, or start of demo
        cl.mtime[1] = cl.mtime[0] - 0.1;
        f = 0.1;
    }
    frac = (cl.time - cl.mtime[1]) / f;
//Con_Printf ("frac: %f\n",frac);
    if (frac < 0)
    {
        if (frac < -0.01)
        {
            SetPal(1);
            cl.time = cl.mtime[1];
//				Con_Printf ("low frac\n");
        }
        frac = 0;
    }
    else if (frac > 1)
    {
        if (frac > 1.01)
        {
            SetPal(2);
            cl.time = cl.mtime[0];
//				Con_Printf ("high frac\n");
        }
        frac = 1;
    }
    else
        SetPal(0);

    return frac;
}


/*
===============
CL_RelinkEntities
===============
*/
void CL_RelinkEntities (void)
{
    entity_t	*ent;
    int			i, j;
    float		frac, f, d;
    vec3_t		delta;
    float		bobjrotate;
    vec3_t		oldorg;
    dlight_t	*dl;
    int			shadpacity;
    float		shadorigin;


// determine partial update time
    frac = CL_LerpPoint ();

    cl_numvisedicts = 0;

//
// interpolate player info
//
    for (i=0 ; i<3 ; i++)
        cl.velocity[i] = cl.mvelocity[1][i] +
                         frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

    if (cls.demoplayback)
    {
        // interpolate the angles
        for (j=0 ; j<3 ; j++)
        {
            d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
            if (d > 180)
                d -= 360;
            else if (d < -180)
                d += 360;
            cl.viewangles[j] = cl.mviewangles[1][j] + frac*d;
        }
    }

    bobjrotate = anglemod(100*cl.time);

// start on the entity after the world
    for (i=1,ent=cl_entities+1 ; i<cl.num_entities ; i++,ent++)
    {
        if (!ent->model)
        {
            // empty slot
            if (ent->forcelink)
                R_RemoveEfrags (ent);	// just became empty
            continue;
        }


        if (r_shadowhack.value && !(ent->effects & EF_ADDITIVE) && !(ent->effects & EF_NODRAW)) //qbism- engoo
        {
            // We find our shadow size here

            //if (ent->model){
            ent->shadowsize = (ent->model->maxs[0]);
            if ((ent->shadowsize > 128) || (ent->shadowsize < 8) || ent->model->dontshadow) ent->shadowsize = 0;
            //}

            // Ok we have a shadow ready. allocate it and do it!!!!!
            else
            {
                ent->shadowsize *= r_shadowhacksize.value; //qbism was 3.9
                shadorigin = ent->model->maxs[2] - (ent->model->mins[2] *0.2); //qbism  was 1.25 - move darklight higher
                //qbism- no better with bleed-through walls... R_AddStain(ent->origin, -60, ent->shadowsize, 3); //qbism a different way to shadowhack, needs it's own layer of stains.


                if (ent->alpha > 0)
                    shadpacity = 28 * ent->alpha; //qbism- made shadow darker, was 32
                else
                    shadpacity = 28; //qbism was 32

                dl = CL_AllocDlight (i);
                VectorCopy (ent->origin,  dl->origin);
                dl->minlight = shadpacity;
                dl->origin[2] -= shadorigin;
                dl->radius = ent->shadowsize;
                dl->die = cl.time + 0.001;
                dl->dark = 1; //qbism- make it a light.... darklight.
            }
        }

// if the object wasn't included in the last packet, remove it
        if (ent->msgtime != cl.mtime[0])
        {
            ent->model = NULL;
            // Manoel Kasimier - model interpolation - begin
            ent->reset_frame_interpolation = true; // Manoel Kasimier
            ent->translate_start_time = 0;
            ent->rotate_start_time    = 0;
            // Manoel Kasimier - model interpolation - end
            continue;
        }

        VectorCopy (ent->origin, oldorg);

        if (ent->forcelink)
        {
            // the entity was not updated in the last message
            // so move to the final spot
            VectorCopy (ent->msg_origins[0], ent->origin);
            VectorCopy (ent->msg_angles[0], ent->angles);
        }
        else
        {
            // if the delta is large, assume a teleport and don't lerp
            f = frac;
            for (j=0 ; j<3 ; j++)
            {
                delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
                if (delta[j] > 100 || delta[j] < -100)
                    f = 1;		// assume a teleportation, not a motion
            }

            // Manoel Kasimier - model interpolation - begin
            // interpolation should be reset in the event of a large delta
            if (f >= 1)  //qbism was == 1
            {
                ent->translate_start_time = 0;
                ent->rotate_start_time    = 0;
            }
            // Manoel Kasimier - model interpolation - end

            // interpolate the origin and angles
            for (j=0 ; j<3 ; j++)
            {
                ent->origin[j] = ent->msg_origins[1][j] + f*delta[j];

                d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
                if (d > 180)
                    d -= 360;
                else if (d < -180)
                    d += 360;
                ent->angles[j] = ent->msg_angles[1][j] + f*d;
            }

        }

        if (ent->effects) // Manoel Kasimier
        {
            // Manoel Kasimier
            if (ent->effects & EF_BRIGHTFIELD)
                R_EntityParticles (ent);
            else if (ent->effects & EF_DARKFIELD)
                R_DarkFieldParticles (ent);

            if (ent->effects & EF_MUZZLEFLASH)
            {
                vec3_t		fv, rv, uv;

                dl = CL_AllocDlight (i);
                VectorCopy (ent->origin,  dl->origin);
                dl->origin[2] += 16;
                AngleVectors (ent->angles, fv, rv, uv);

                VectorMA (dl->origin, 18, fv, dl->origin);
                dl->radius = 200 + (rand()&31);
                dl->minlight = 32;
                dl->die = cl.time + 0.1;
                dl->color = palmapnofb[32>>3][32>>3][40>>3]; //qbism dyncol  HACK
            }
            if (ent->effects & EF_BRIGHTLIGHT)
            {
                dl = CL_AllocDlight (i);
                VectorCopy (ent->origin,  dl->origin);
                dl->origin[2] += 16;
                dl->radius = 400 + (rand()&31);
                dl->die = cl.time + 0.001;
                dl->color = palmapnofb[60>>3][50>>3][10>>3]; //qbism dyncol
            }
            if (ent->effects & EF_DIMLIGHT)
            {
                dl = CL_AllocDlight (i);
                VectorCopy (ent->origin,  dl->origin);
                dl->radius = 200 + (rand()&31);
                dl->die = cl.time + 0.001;
                dl->color = palmapnofb[30>>3][20>>3][10>>3]; //qbism dyncol
            }

        } // Manoel Kasimier
        if (!(ent->effects & 0xFF800000))  //qbism based on DP model flags
            ent->effects |= ent->model->flags;

        if (ent->effects) // Manoel Kasimier
        {
            // Manoel Kasimier
// rotate binary objects locally
            if (ent->effects & EF_ROTATE)
            {
//		ent->effects |= EF_REFLECTIVE; //qbism- test effect on rotating items
                ent->angles[1] = bobjrotate;
            }
            if (ent->effects & EF_GIB)
                R_RocketTrail (oldorg, ent->origin, 2);
            else if (ent->effects & EF_ZOMGIB)
                R_RocketTrail (oldorg, ent->origin, 4);
            else if (ent->effects & EF_TRACER)
                R_RocketTrail (oldorg, ent->origin, 3);
            else if (ent->effects & EF_TRACER2)
                R_RocketTrail (oldorg, ent->origin, 5);
            else if (ent->effects & EF_ROCKET)
            {
                R_RocketTrail (oldorg, ent->origin, 0);
                dl = CL_AllocDlight (i);
                VectorCopy (ent->origin, dl->origin);
                dl->radius = 200;
                dl->die = cl.time + 0.01;
            }
            else if (ent->effects & EF_GRENADE)
                R_RocketTrail (oldorg, ent->origin, 1);
            else if (ent->effects & EF_TRACER3)
                R_RocketTrail (oldorg, ent->origin, 6);
        } // Manoel Kasimier
        // Tomaz - QC Glow
        if (ent->glow_size)
            if (!(ent->effects & (EF_MUZZLEFLASH)))//qbism - |EF_BRIGHTLIGHT|EF_DIMLIGHT))) // Manoel Kasimier
            {
                dl = CL_AllocDlight (i);
                VectorCopy (ent->origin, dl->origin);
                dl->radius = abs(ent->glow_size); // Manoel Kasimier - edited
                dl->dark = (ent->glow_size < 0); // Manoel Kasimier
                dl->die = cl.time + 0.001;
                dl->color = palmapnofb[ent->glow_red >>3] [ent->glow_green >>3] [ent->glow_blue >>3]; //qbism dyncol
            }

        ent->forcelink = false;

        // Manoel Kasimier - model interpolation - begin
        // hack to make interpolation look better in demos
        if (cls.demoplayback)
            if (i != cl.viewentity && ent!=&cl.viewent && ent->model->type == mod_alias)
            {
                //	if (ent->translate_start_time)
                VectorCopy (ent->msg_origins[0], ent->origin);
                //	if (ent->rotate_start_time)
                VectorCopy (ent->msg_angles[0], ent->angles);
            }
        // Manoel Kasimier - model interpolation - end

        if (i == cl.viewentity && !chase_active.value)
            continue;

        if ( ent->effects & EF_NODRAW )
            continue;

        if (cl_numvisedicts < MAX_VISEDICTS)
        {
            cl_visedicts[cl_numvisedicts] = ent;
            cl_numvisedicts++;
        }
    }
}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int CL_ReadFromServer (void)
{
    int		ret;

    cl.oldtime = cl.time;
    cl.time += host_frametime;

    do
    {
        ret = CL_GetMessage ();
        if (ret == -1)
            Host_Error ("CL_ReadFromServer: lost server connection");
        if (!ret)
            break;

        cl.last_received_message = realtime;
        CL_ParseServerMessage ();
    }
    while (ret && cls.state == ca_connected);

    if (cl_shownet.value)
        Con_Printf ("\n");

    CL_RelinkEntities ();
//	CL_UpdateTEnts (); // Manoel Kasimier - moved to R_RenderView_

//
// bring the links up to date
//
    return 0;
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
    usercmd_t		cmd;

    if (cls.state != ca_connected)
        return;

    if (cls.signon == SIGNONS)
    {
        // get basic movement from keyboard
        CL_BaseMove (&cmd);

        // allow mice or other external controllers to add to the move
        IN_Move (&cmd);

        // send the unreliable message
        CL_SendMove (&cmd);

    }

    if (cls.demoplayback)
    {
        SZ_Clear (&cls.message);
        return;
    }

// send the reliable message
    if (!cls.message.cursize)
        return;		// no message at all

    if (!NET_CanSendMessage (cls.netcon))
    {
        Con_DPrintf ("CL_WriteToServer: can't send\n");
        return;
    }

    if (NET_SendMessage (cls.netcon, &cls.message) == -1)
        Host_Error ("CL_WriteToServer: lost server connection");

    SZ_Clear (&cls.message);
}

/*
=============
CL_Viewpos_f

Display client's position and mangle
=============
*/
void CL_Viewpos_f (void)
{
    // Player position
    Con_Printf ("Viewpos: (%i %i %i) %i %i %i\n",
                (int)cl_entities[cl.viewentity].origin[0],
                (int)cl_entities[cl.viewentity].origin[1],
                (int)cl_entities[cl.viewentity].origin[2],
                (int)cl.viewangles[PITCH],
                (int)cl.viewangles[YAW],
                (int)cl.viewangles[ROLL]);

    if (sv_player && sv_player->v.movetype == MOVETYPE_NOCLIP && Cmd_Argc() == 4)
    {
        // Major hack ...
        sv_player->v.origin[0] = atoi (Cmd_Argv(1));
        sv_player->v.origin[1] = atoi (Cmd_Argv(2));
        sv_player->v.origin[2] = atoi (Cmd_Argv(3));
    }
}

/*
=============
CL_Mapname_f
=============
*/
void CL_Mapname_f (void)
{
    char name[MAX_QPATH];

    name[0] = '\0';

    if (cl.worldmodel && cl.worldmodel->name)
        COM_StripExtension (cl.worldmodel->name + 5, name);

    Con_Printf ("\"mapname\" is \"%s\"\n", name);
}

/*
=============
CL_StaticEnts_f
=============
*/
void CL_StaticEnts_f (void)
{
    Con_Printf ("%d static entities\n", cl.num_statics);
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{
    SZ_Alloc (&cls.message, 8192);

    CL_InitInput ();
    CL_InitTEnts ();

//
// register our commands
//
    Cvar_RegisterVariable (&cl_name);
    Cvar_RegisterVariable (&cl_color);
    Cvar_RegisterVariable (&cl_upspeed);
    Cvar_RegisterVariable (&cl_forwardspeed);
    Cvar_RegisterVariable (&cl_backspeed);
    Cvar_RegisterVariable (&cl_sidespeed);
    Cvar_RegisterVariable (&cl_movespeedkey);
    Cvar_RegisterVariable (&cl_yawspeed);
    Cvar_RegisterVariable (&cl_pitchspeed);
    Cvar_RegisterVariable (&cl_anglespeedkey);
    Cvar_RegisterVariable (&cl_vibration); // Manoel Kasimier
    Cvar_RegisterVariable (&cl_showfps);	// 2001-11-31 FPS display by QuakeForge/Muff
    Cvar_RegisterVariable (&cl_shownet);
    Cvar_RegisterVariable (&cl_nolerp);
    Cvar_RegisterVariable (&lookspring);
    Cvar_RegisterVariable (&lookstrafe);
    Cvar_RegisterVariable (&sensitivity);
    Cvar_RegisterVariable (&cl_maxfps); //qbism from qrack

    Cvar_RegisterVariable (&m_pitch);
    Cvar_RegisterVariable (&m_yaw);
    Cvar_RegisterVariable (&m_forward);
    Cvar_RegisterVariable (&m_side);
    Cvar_RegisterVariable (&m_look); // Manoel Kasimier - m_look
    Cvar_RegisterVariable (&cutscene); // Nehahra

    #ifdef WEBDL    //qbism - sometimes works, needs more testing
    Cvar_RegisterVariable (&cl_web_download);  //qbism - R00k / Baker tute
    Cvar_RegisterVariable (&cl_web_download_url);
    #endif

    Cmd_AddCommand ("entities", CL_PrintEntities_f);
    Cmd_AddCommand ("disconnect", CL_Disconnect_f);
    Cmd_AddCommand ("record", CL_Record_f);
    Cmd_AddCommand ("stop", CL_Stop_f);
    Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
    Cmd_AddCommand ("timedemo", CL_TimeDemo_f);
    Cmd_AddCommand ("viewpos", CL_Viewpos_f);
    Cmd_AddCommand ("mapname", CL_Mapname_f);
    Cmd_AddCommand ("staticents", CL_StaticEnts_f);
}

