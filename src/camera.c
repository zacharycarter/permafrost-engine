/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2017-2018 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include "camera.h"
#include "render/public/render.h"
#include "config.h"
#include "collision.h"

#include <SDL.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

struct camera {
    float    speed;
    float    sensitivity;

    vec3_t   pos;
    vec3_t   front;
    vec3_t   up;

    float    pitch;
    float    yaw;

    uint32_t prev_frame_ts;

    /* When 'bounded' is true, the camera position must 
     * always be within the 'bounds' box */
    bool            bounded;
    struct bound_box bounds;
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*****************************************************************************/
/* GLOBAL VARIABLES                                                          */
/*****************************************************************************/

const unsigned g_sizeof_camera = sizeof(struct camera);

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static bool camera_pos_in_bounds(const struct camera *cam)
{
    /* X is increasing to the left in our coordinate system */
    return (cam->pos.x <= cam->bounds.x && cam->pos.x >= cam->bounds.x - cam->bounds.w)
        && (cam->pos.z >= cam->bounds.z && cam->pos.z <= cam->bounds.z + cam->bounds.h);
}

static void camera_move_within_bounds(struct camera *cam)
{
    /* X is increasing to the left in our coordinate system */
    cam->pos.x = MIN(cam->pos.x, cam->bounds.x);
    cam->pos.x = MAX(cam->pos.x, cam->bounds.x - cam->bounds.w);

    cam->pos.z = MAX(cam->pos.z, cam->bounds.z);
    cam->pos.z = MIN(cam->pos.z, cam->bounds.z + cam->bounds.h);
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

struct camera *Camera_New(void)
{
    struct camera *ret = calloc(1, sizeof(struct camera));
    if(!ret)
        return NULL;

    return ret;
}

void Camera_Free(struct camera *cam)
{
    free(cam);
}

void Camera_SetPos(struct camera *cam, vec3_t pos)
{
    cam->pos = pos; 

    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

void Camera_SetPitchAndYaw(struct camera *cam, float pitch, float yaw)
{
    cam->pitch = pitch;
    cam->yaw = yaw;

    vec3_t front;
    front.x = cos(DEG_TO_RAD(cam->yaw)) * cos(DEG_TO_RAD(cam->pitch));
    front.y = sin(DEG_TO_RAD(cam->pitch));
    front.z = sin(DEG_TO_RAD(cam->yaw)) * cos(DEG_TO_RAD(cam->pitch)) * -1;
    PFM_Vec3_Normal(&front, &cam->front);

    /* Find a vector that is orthogonal to 'front' in the XZ plane */
    vec3_t xz = (vec3_t){cam->front.z, 0.0f, -cam->front.x};
    PFM_Vec3_Cross(&cam->front, &xz, &cam->up);
    PFM_Vec3_Normal(&cam->up, &cam->up);
}

void Camera_SetSpeed(struct camera *cam, float speed)
{
    cam->speed = speed;
}

void Camera_SetSens(struct camera *cam, float sensitivity)
{
    cam->sensitivity = sensitivity;
}

float Camera_GetYaw(const struct camera *cam)
{
    return cam->yaw;
}

float Camera_GetPitch(const struct camera *cam)
{
    return cam->pitch;
}

float Camera_GetHeight(const struct camera *cam)
{
    return cam->pos.y;
}

vec3_t Camera_GetPos(const struct camera *cam)
{
    return cam->pos;
}

void Camera_MoveLeftTick(struct camera *cam)
{
    uint32_t tdelta;
    vec3_t   vdelta, right;
    
    if(!cam->prev_frame_ts)
        cam->prev_frame_ts = SDL_GetTicks();

    uint32_t curr = SDL_GetTicks();
    tdelta = curr - cam->prev_frame_ts;

    PFM_Vec3_Cross(&cam->front, &cam->up, &right);
    PFM_Vec3_Normal(&right, &right);

    PFM_Vec3_Scale(&right, tdelta * cam->speed, &vdelta);
    PFM_Vec3_Add(&cam->pos, &vdelta, &cam->pos);

    if(cam->bounded) camera_move_within_bounds(cam);
    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

void Camera_MoveRightTick(struct camera *cam)
{
    uint32_t tdelta;
    vec3_t   vdelta, right;
    
    if(!cam->prev_frame_ts)
        cam->prev_frame_ts = SDL_GetTicks();

    uint32_t curr = SDL_GetTicks();
    tdelta = curr - cam->prev_frame_ts;

    PFM_Vec3_Cross(&cam->front, &cam->up, &right);
    PFM_Vec3_Normal(&right, &right);

    PFM_Vec3_Scale(&right, tdelta * cam->speed, &vdelta);
    PFM_Vec3_Sub(&cam->pos, &vdelta, &cam->pos);

    if(cam->bounded) camera_move_within_bounds(cam);
    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

void Camera_MoveFrontTick(struct camera *cam)
{
    uint32_t tdelta;
    vec3_t   vdelta;
    
    if(!cam->prev_frame_ts)
        cam->prev_frame_ts = SDL_GetTicks();

    uint32_t curr = SDL_GetTicks();
    tdelta = curr - cam->prev_frame_ts;

    PFM_Vec3_Scale(&cam->front, tdelta * cam->speed, &vdelta);
    PFM_Vec3_Add(&cam->pos, &vdelta, &cam->pos);

    if(cam->bounded) camera_move_within_bounds(cam);
    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

void Camera_MoveBackTick(struct camera *cam)
{
    uint32_t tdelta;
    vec3_t   vdelta;
    
    if(!cam->prev_frame_ts)
        cam->prev_frame_ts = SDL_GetTicks();

    uint32_t curr = SDL_GetTicks();
    tdelta = curr - cam->prev_frame_ts;

    PFM_Vec3_Scale(&cam->front, tdelta * cam->speed, &vdelta);
    PFM_Vec3_Sub(&cam->pos, &vdelta, &cam->pos);

    if(cam->bounded) camera_move_within_bounds(cam);
    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

void Camera_MoveDirectionTick(struct camera *cam, vec3_t dir)
{
    uint32_t tdelta;
    vec3_t   vdelta;

    if(!cam->prev_frame_ts)
        cam->prev_frame_ts = SDL_GetTicks();

    float mag = sqrt(pow(dir.x,2) + pow(dir.y,2) + pow(dir.z,2));
    if(mag == 0.0f)
        return;

    PFM_Vec3_Normal(&dir, &dir);

    uint32_t curr = SDL_GetTicks();
    tdelta = curr - cam->prev_frame_ts;

    PFM_Vec3_Scale(&dir, tdelta * cam->speed, &vdelta);
    PFM_Vec3_Add(&cam->pos, &vdelta, &cam->pos);

    if(cam->bounded) camera_move_within_bounds(cam);
    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

void Camera_ChangeDirection(struct camera *cam, int dx, int dy)
{
    float sdx = dx * cam->sensitivity; 
    float sdy = dy * cam->sensitivity;

    cam->yaw   += sdx;
    cam->pitch -= sdy;

    cam->pitch = cam->pitch <  89.0f ? cam->pitch :  89.0f;
    cam->pitch = cam->pitch > -89.0f ? cam->pitch : -89.0f;

    vec3_t front;         
    front.x = cos(DEG_TO_RAD(cam->yaw)) * cos(DEG_TO_RAD(cam->pitch));
    front.y = sin(DEG_TO_RAD(cam->pitch));
    front.z = sin(DEG_TO_RAD(cam->yaw)) * cos(DEG_TO_RAD(cam->pitch)) * -1;
    PFM_Vec3_Normal(&front, &cam->front);

    /* Find a vector that is orthogonal to 'front' in the XZ plane */
    vec3_t xz = (vec3_t){cam->front.z, 0.0f, -cam->front.x};
    PFM_Vec3_Cross(&cam->front, &xz, &cam->up);
    PFM_Vec3_Normal(&cam->up, &cam->up);
}

void Camera_TickFinishPerspective(struct camera *cam)
{
    mat4x4_t view, proj;

    /* Set the view matrix for the vertex shader */
    vec3_t target;
    PFM_Vec3_Add(&cam->pos, &cam->front, &target);
    PFM_Mat4x4_MakeLookAt(&cam->pos, &target, &cam->up, &view);

    R_GL_SetViewMatAndPos(&view, &cam->pos);
    
    /* Set the projection matrix for the vertex shader */
    GLint viewport[4]; 
    glGetIntegerv(GL_VIEWPORT, viewport);
    PFM_Mat4x4_MakePerspective(DEG_TO_RAD(45.0f), ((GLfloat)viewport[2])/viewport[3], CAM_Z_NEAR_DIST, CONFIG_DRAWDIST, &proj);

    R_GL_SetProj(&proj);

    /* Update our last timestamp */
    cam->prev_frame_ts = SDL_GetTicks();
}

void Camera_TickFinishOrthographic(struct camera *cam, vec2_t bot_left, vec2_t top_right)
{
    mat4x4_t view, proj;

    /* Set the view matrix for the vertex shader */
    vec3_t target;
    PFM_Vec3_Add(&cam->pos, &cam->front, &target);
    PFM_Mat4x4_MakeLookAt(&cam->pos, &target, &cam->up, &view);

    R_GL_SetViewMatAndPos(&view, &cam->pos);
    
    /* Set the projection matrix for the vertex shader */
    PFM_Mat4x4_MakeOrthographic(bot_left.raw[0], top_right.raw[0], bot_left.raw[1], top_right.raw[1], CAM_Z_NEAR_DIST, CONFIG_DRAWDIST, &proj);
    R_GL_SetProj(&proj);

    /* Update our last timestamp */
    cam->prev_frame_ts = SDL_GetTicks();
}

void Camera_RestrictPosWithBox(struct camera *cam, struct bound_box box)
{
    cam->bounded = true;
    cam->bounds = box;

    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

void Camera_UnrestrictPos(struct camera *cam)
{
    cam->bounded = false;

    assert(!cam->bounded || camera_pos_in_bounds(cam));
}

bool Camera_PosIsRestricted(const struct camera *cam)
{
    return cam->bounded;
}

void Camera_MakeViewMat(const struct camera *cam, mat4x4_t *out)
{
    vec3_t target;
    PFM_Vec3_Add((vec3_t*)&cam->pos, (vec3_t*)&cam->front, &target);
    PFM_Mat4x4_MakeLookAt((vec3_t*)&cam->pos, &target, (vec3_t*)&cam->up, out);
}

void Camera_MakeProjMat(const struct camera *cam, mat4x4_t *out)
{
    GLint viewport[4]; 
    glGetIntegerv(GL_VIEWPORT, viewport);
    PFM_Mat4x4_MakePerspective(CAM_FOV_RAD, ((GLfloat)viewport[2])/viewport[3], 0.1f, CONFIG_DRAWDIST, out);
}

/* Useful information about frustrums here:
 * http://cgvr.informatik.uni-bremen.de/teaching/cg_literatur/lighthouse3d_view_frustum_culling/index.html
 * Note that our engine's coordinate system is left-handed.
 */
void Camera_MakeFrustum(const struct camera *cam, struct frustum *out)
{
    GLint viewport[4]; 
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float aspect_ratio = ((float)viewport[2])/viewport[3];

    const float near_dist = CAM_Z_NEAR_DIST;
    const float far_dist = CONFIG_DRAWDIST;

    const float near_height = 2 * tan(CAM_FOV_RAD/2.0f) * near_dist;
    const float near_width = near_height * aspect_ratio;

    const float far_height = 2 * tan(CAM_FOV_RAD/2.0f) * far_dist;
    const float far_width = far_height * aspect_ratio;

    vec3_t tmp;
    vec3_t cam_right;
    PFM_Vec3_Cross((vec3_t*)&cam->up, (vec3_t*)&cam->front, &cam_right);
    PFM_Vec3_Normal(&cam_right, &cam_right);

    vec3_t nc = cam->pos;
    PFM_Vec3_Scale((vec3_t*)&cam->front, near_dist, &tmp);
    PFM_Vec3_Add(&nc, &tmp, &nc);

    vec3_t fc = cam->pos;
    PFM_Vec3_Scale((vec3_t*)&cam->front, far_dist, &tmp);
    PFM_Vec3_Add(&nc, &tmp, &fc);

    vec3_t up_half_hfar;
    PFM_Vec3_Scale((vec3_t*)&cam->up, far_height/2.0f, &up_half_hfar);

    vec3_t right_half_wfar;
    PFM_Vec3_Scale(&cam_right, far_width/2.0f, &right_half_wfar);

    vec3_t up_half_hnear;
    PFM_Vec3_Scale((vec3_t*)&cam->up, near_height/2.0f, &up_half_hnear);

    vec3_t right_half_wnear;
    PFM_Vec3_Scale(&cam_right, near_width/2.0f, &right_half_wnear);


    /* Far Top Left corner */
    PFM_Vec3_Add(&fc, &up_half_hfar, &tmp);
    PFM_Vec3_Sub(&tmp, &right_half_wfar, &out->ftl);

    /* Far Top Right corner */
    PFM_Vec3_Add(&fc, &up_half_hfar, &tmp);
    PFM_Vec3_Add(&tmp, &right_half_wfar, &out->ftr);

    /* Far Bottom Left corner */
    PFM_Vec3_Sub(&fc, &up_half_hfar, &tmp);
    PFM_Vec3_Sub(&tmp, &right_half_wfar, &out->fbl);

    /* Far Bottom Right corner */
    PFM_Vec3_Sub(&fc, &up_half_hfar, &tmp);
    PFM_Vec3_Add(&tmp, &right_half_wfar, &out->fbr);

    /* Near Top Left corner */
    PFM_Vec3_Add(&nc, &up_half_hnear, &tmp);
    PFM_Vec3_Sub(&tmp, &right_half_wnear, &out->ntl);

    /* Near Top Right corner */
    PFM_Vec3_Add(&nc, &up_half_hnear, &tmp);
    PFM_Vec3_Add(&tmp, &right_half_wnear, &out->ntr);

    /* Near Bottom Left corner */
    PFM_Vec3_Sub(&nc, &up_half_hnear, &tmp); 
    PFM_Vec3_Sub(&tmp, &right_half_wnear, &out->nbl);

    /* Near Bottom right corner */
    PFM_Vec3_Sub(&nc, &up_half_hnear, &tmp);
    PFM_Vec3_Add(&tmp, &right_half_wnear, &out->nbr);


    /* Near plane */
    out->near.point = nc;
    out->near.normal = cam->front;

    /* Far plane */
    vec3_t negative_dir;
    PFM_Vec3_Scale((vec3_t*)&cam->front, -1.0f, &negative_dir);

    out->far.point = fc;
    out->far.normal = negative_dir;

    /* Right plane */
    vec3_t p_to_near_right_edge;
    PFM_Vec3_Scale(&cam_right, near_width / 2.0f, &tmp);
    PFM_Vec3_Add(&nc, &tmp, &tmp);
    PFM_Vec3_Sub(&tmp, (vec3_t*)&cam->pos, &p_to_near_right_edge);
    PFM_Vec3_Normal(&p_to_near_right_edge, &p_to_near_right_edge);

    out->right.point = cam->pos;
    PFM_Vec3_Cross(&p_to_near_right_edge, (vec3_t*)&cam->up, &out->right.normal);

    /* Left plane */
    vec3_t p_to_near_left_edge;
    PFM_Vec3_Scale(&cam_right, near_width / 2.0f, &tmp);
    PFM_Vec3_Sub(&nc, &tmp, &tmp);
    PFM_Vec3_Sub(&tmp, (vec3_t*)&cam->pos, &p_to_near_left_edge);
    PFM_Vec3_Normal(&p_to_near_left_edge, &p_to_near_left_edge);

    out->left.point = cam->pos;
    PFM_Vec3_Cross((vec3_t*)&cam->up, &p_to_near_left_edge, &out->left.normal);

    /* Top plane */
    vec3_t p_to_near_top_edge;
    PFM_Vec3_Scale((vec3_t*)&cam->up, near_height / 2.0f, &tmp);
    PFM_Vec3_Add(&nc, &tmp, &tmp);
    PFM_Vec3_Sub(&tmp, (vec3_t*)&cam->pos, &p_to_near_top_edge);
    PFM_Vec3_Normal(&p_to_near_top_edge, &p_to_near_top_edge);

    out->top.point = cam->pos;
    PFM_Vec3_Cross(&cam_right, &p_to_near_top_edge, &out->top.normal);

    /* Bot plane */
    vec3_t p_to_near_bot_edge;
    PFM_Vec3_Scale((vec3_t*)&cam->up, near_height / 2.0f, &tmp);
    PFM_Vec3_Sub(&nc, &tmp, &tmp);
    PFM_Vec3_Sub(&tmp, (vec3_t*)&cam->pos, &p_to_near_bot_edge);
    PFM_Vec3_Normal(&p_to_near_bot_edge, &p_to_near_bot_edge);

    out->bot.point = cam->pos;
    PFM_Vec3_Cross(&p_to_near_bot_edge, &cam_right, &out->bot.normal);
}

