/* The game camera, for drawing at world positions: over a fighter, along a hitbox, at the ledge.
 *
 *   pp_camera cam;
 *   if (pp_camera_read(host, &cam)) {
 *       float x = host->rdf32(fp + PP_FT_POS), y = host->rdf32(fp + PP_FT_POS + 4);
 *       float sx, sy, ppu;
 *       if (pp_project(&cam, x, y + 18, 0, &sx, &sy, &ppu))
 *           host->hud_label(sx, sy, pp_rgba(PP_RGB_TEXT, 1), 14, 1, "Tech!");
 *   }
 *
 * Read the camera once per frame, then project as many points as you like. Positions come out in
 * the HUD's 640 x 480 space; ppu is how many HUD pixels one world unit is at that depth (a hitbox's
 * radius times ppu is its radius on screen).
 */
#ifndef PASCALPATCH_CAMERA_H
#define PASCALPATCH_CAMERA_H
#include <math.h>
#include "melee.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PP_GAME_CAMERA    0x80452C68u  /* Camera game_camera; +0 its HSD_GObj */
#define PP_GOBJ_HSD_OBJ   0x28u        /* HSD_GObj.hsd_obj: for the camera gobj, its HSD_CObj */
#define PP_COBJ_VIEWPORT  0x0Cu        /* f32 left, right, top, bottom */
#define PP_COBJ_NEAR      0x38u
#define PP_COBJ_FOV       0x40u        /* f32 vertical field of view, degrees */
#define PP_COBJ_ASPECT    0x44u
#define PP_COBJ_PROJ      0x50u        /* u8 projection: 1 perspective */
#define PP_COBJ_VIEW      0x54u        /* 3 x 4 view matrix */

typedef struct {
    float view[12];
    float cot, aspect, near_z, left, right, top, bottom;
} pp_camera;

/* 0 when there is no game camera (menus) or it does not read sensibly. */
static inline int pp_camera_read(const pp_host *h, pp_camera *c) {
    uint32_t gobj = h->rd32(PP_GAME_CAMERA), cobj;
    float fov;
    int i;
    if (!pp_is_ptr(gobj)) return 0;
    cobj = h->rd32(gobj + PP_GOBJ_HSD_OBJ);
    if (!pp_is_ptr(cobj) || h->rd8(cobj + PP_COBJ_PROJ) != 1) return 0;
    for (i = 0; i < 12; ++i) c->view[i] = h->rdf32(cobj + PP_COBJ_VIEW + 4 * (uint32_t)i);
    fov = h->rdf32(cobj + PP_COBJ_FOV);
    c->aspect = h->rdf32(cobj + PP_COBJ_ASPECT);
    c->near_z = h->rdf32(cobj + PP_COBJ_NEAR);
    c->left = h->rdf32(cobj + PP_COBJ_VIEWPORT);
    c->right = h->rdf32(cobj + PP_COBJ_VIEWPORT + 4);
    c->top = h->rdf32(cobj + PP_COBJ_VIEWPORT + 8);
    c->bottom = h->rdf32(cobj + PP_COBJ_VIEWPORT + 12);
    if (!(fov > 1 && fov < 179) || !(c->aspect > 0.1f) || c->right <= c->left || c->bottom <= c->top) return 0;
    c->cot = 1.0f / tanf(fov * 3.14159265f / 360.0f);
    return 1;
}

/* A world point in HUD space (GX's C_MTXPerspective). 0 when it is behind the camera.
 * px_per_unit may be NULL. */
static inline int pp_project(const pp_camera *c, float x, float y, float z, float *sx, float *sy, float *px_per_unit) {
    const float *m = c->view;
    float vx = m[0] * x + m[1] * y + m[2] * z + m[3];
    float vy = m[4] * x + m[5] * y + m[6] * z + m[7];
    float vz = m[8] * x + m[9] * y + m[10] * z + m[11];
    float w;
    if (vz > -c->near_z) return 0;
    w = -vz;
    *sx = c->left + (c->cot / c->aspect * vx / w + 1) * 0.5f * (c->right - c->left);
    *sy = c->top + (1 - c->cot * vy / w) * 0.5f * (c->bottom - c->top);
    if (px_per_unit) *px_per_unit = c->cot / w * 0.5f * (c->bottom - c->top);
    return isfinite(*sx) && isfinite(*sy);
}

#ifdef __cplusplus
}
#endif
#endif
