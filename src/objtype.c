#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datastruct/lrucache.h"
#include "defines.h"
#include "jagfile.h"
#include "model.h"
#include "objtype.h"
#include "packet.h"
#include "pix24.h"
#include "pix2d.h"
#include "pix3d.h"
#include "platform.h"

extern Pix3D _Pix3D;
extern Pix2D _Pix2D;

// name taken from rs3
ObjTypeData _ObjType = {0};

static void objtype_decode(ObjType *obj, Packet *dat);

static ObjType *objtype_new(void) {
    ObjType *obj = calloc(1, sizeof(ObjType));
    obj->index = -1;
    return obj;
}

void objtype_unpack(Jagfile *config) {
    _ObjType.membersWorld = true;
    _ObjType.modelCache = lrucache_new(50);
    _ObjType.iconCache = lrucache_new(200);

    _ObjType.dat = jagfile_to_packet(config, "obj.dat");
    Packet *idx = jagfile_to_packet(config, "obj.idx");

    _ObjType.count = g2(idx);
    _ObjType.offsets = calloc(_ObjType.count, sizeof(int));

    int offset = 2;
    for (int id = 0; id < _ObjType.count; id++) {
        _ObjType.offsets[id] = offset;
        offset += g2(idx);
    }

    _ObjType.cache = calloc(10, sizeof(ObjType *));
    for (int id = 0; id < 10; id++) {
        _ObjType.cache[id] = objtype_new();
    }

    packet_free(idx);
}

void objtype_free_global(void) {
    lrucache_free(_ObjType.modelCache);
    lrucache_free(_ObjType.iconCache);
    free(_ObjType.offsets);
    for (int i = 0; i < 10; i++) {
        // TODO this doesn't free everything?
        free(_ObjType.cache[i]);
    }
    free(_ObjType.cache);
    packet_free(_ObjType.dat);
}

ObjType *objtype_get(int id) {
    for (int i = 0; i < 10; i++) {
        if (_ObjType.cache[i]->index == id) {
            return _ObjType.cache[i];
        }
    }

    _ObjType.cachePos = (_ObjType.cachePos + 1) % 10;
    ObjType *obj = _ObjType.cache[_ObjType.cachePos];
    _ObjType.dat->pos = _ObjType.offsets[id];
    obj->index = id;
    objtype_reset(obj);
    objtype_decode(obj, _ObjType.dat);

    if (obj->certtemplate != -1) {
        objtype_to_certificate(obj);
    }

    if (!_ObjType.membersWorld && obj->members) {
        strcpy(obj->name, "Members Object");
        strcpy(obj->desc, "Login to a members' server to use this object.");
        obj->op = NULL;
        obj->iop = NULL;
    }

    return obj;
}

Pix24 *objtype_get_icon(int id, int count) {
    Pix24 *icon = (Pix24 *)lrucache_get(_ObjType.iconCache, id);
    if (icon && icon->crop_h != count && icon->crop_h != -1) {
        linkable_unlink(&icon->link.link);
        icon = NULL;
    }

    if (icon) {
        return icon;
    }

    ObjType *obj = objtype_get(id);
    if (!obj->countobj) {
        count = -1;
    }

    if (count > 1) {
        int countobj = -1;
        for (int i = 0; i < 10; i++) {
            if (count >= obj->countco[i] && obj->countco[i] != 0) {
                countobj = obj->countobj[i];
            }
        }

        if (countobj != -1) {
            obj = objtype_get(countobj);
        }
    }

    icon = pix24_new(32, 32, true);

    int _cx = _Pix3D.center_x;
    int _cy = _Pix3D.center_y;
    int *_loff = _Pix3D.line_offset;
    int *_data = _Pix2D.pixels;
    int _w = _Pix2D.width;
    int _h = _Pix2D.height;
    int _l = _Pix2D.left;
    int _r = _Pix2D.right;
    int _t = _Pix2D.top;
    int _b = _Pix2D.bottom;

    _Pix3D.jagged = false;
    pix2d_bind(32, 32, icon->pixels);
    pix2d_fill_rect(0, 0, BLACK, 32, 32);
    pix3d_init2d();

    Model *imodel = objtype_get_interfacemodel(obj, 1, true);
    int sinPitch = _Pix3D.sin_table[obj->xan2d] * obj->zoom2d >> 16;
    int cosPitch = _Pix3D.cos_table[obj->xan2d] * obj->zoom2d >> 16;
    model_draw_simple(imodel, 0, obj->yan2d, obj->zan2d, obj->xan2d, obj->xof2d, sinPitch + imodel->max_y / 2 + obj->yof2d, cosPitch + obj->yof2d);

    for (int x = 31; x >= 0; x--) {
        for (int y = 31; y >= 0; y--) {
            if (icon->pixels[x + y * 32] != 0) {
                continue;
            }

            if (x > 0 && icon->pixels[x + y * 32 - 1] > 1) {
                icon->pixels[x + y * 32] = 1;
            } else if (y > 0 && icon->pixels[x + (y - 1) * 32] > 1) {
                icon->pixels[x + y * 32] = 1;
            } else if (x < 31 && icon->pixels[x + y * 32 + 1] > 1) {
                icon->pixels[x + y * 32] = 1;
            } else if (y < 31 && icon->pixels[x + (y + 1) * 32] > 1) {
                icon->pixels[x + y * 32] = 1;
            }
        }
    }

    for (int x = 31; x >= 0; x--) {
        for (int y = 31; y >= 0; y--) {
            if (icon->pixels[x + y * 32] == 0 && x > 0 && y > 0 && icon->pixels[x + (y - 1) * 32 - 1] > 0) {
                icon->pixels[x + y * 32] = 3153952;
            }
        }
    }

    if (obj->certtemplate != -1) {
        Pix24 *linkedIcon = objtype_get_icon(obj->certlink, 10);
        int w = linkedIcon->crop_w;
        int h = linkedIcon->crop_h;
        linkedIcon->crop_w = 32;
        linkedIcon->crop_h = 32;
        pix24_crop(linkedIcon, 5, 5, 22, 22);
        linkedIcon->crop_w = w;
        linkedIcon->crop_h = h;
    }

    lrucache_put(_ObjType.iconCache, id, &icon->link);
    pix2d_bind(_w, _h, _data);
    pix2d_set_clipping(_b, _r, _t, _l);
    _Pix3D.center_x = _cx;
    _Pix3D.center_y = _cy;
    _Pix3D.line_offset = _loff;
    _Pix3D.jagged = true;
    if (obj->stackable) {
        icon->crop_w = 33;
    } else {
        icon->crop_w = 32;
    }
    icon->crop_h = count;
    return icon;
}

void objtype_reset(ObjType *obj) {
    free(obj->recol_s);
    free(obj->recol_d);
    if (obj->op) {
        for (int i = 0; i < 5; i++) {
            free(obj->op[i]);
        }
        free(obj->op);
    }
    if (obj->iop) {
        for (int i = 0; i < 5; i++) {
            free(obj->iop[i]);
        }
        free(obj->iop);
    }
    if (obj->countobj) {
        free(obj->countobj);
        free(obj->countco);
    }
    obj->model = 0;
    obj->name[0] = '\0';
    obj->desc[0] = '\0';
    obj->recol_s = NULL;
    obj->recol_d = NULL;
    obj->zoom2d = 2000;
    obj->xan2d = 0;
    obj->yan2d = 0;
    obj->zan2d = 0;
    obj->xof2d = 0;
    obj->yof2d = 0;
    obj->code9 = false;
    obj->code10 = -1;
    obj->stackable = false;
    obj->cost = 1;
    obj->members = false;
    obj->op = NULL;
    obj->iop = NULL;
    obj->manwear = -1;
    obj->manwear2 = -1;
    obj->manwearOffsetY = 0;
    obj->womanwear = -1;
    obj->womanwear2 = -1;
    obj->womanwearOffsetY = 0;
    obj->manwear3 = -1;
    obj->womanwear3 = -1;
    obj->manhead = -1;
    obj->manhead2 = -1;
    obj->womanhead = -1;
    obj->womanhead2 = -1;
    obj->countobj = NULL;
    obj->countco = NULL;
    obj->certlink = -1;
    obj->certtemplate = -1;
}

static void objtype_decode(ObjType *obj, Packet *dat) {
    while (true) {
        int code = g1(dat);
        if (code == 0) {
            break;
        }

        if (code == 1) {
            obj->model = g2(dat);
        } else if (code == 2) {
            char *str = gjstr(dat);
            strcpy(obj->name, str);
            free(str);
        } else if (code == 3) {
            char *str = gjstr(dat);
            strcpy(obj->desc, str);
            free(str);
        } else if (code == 4) {
            obj->zoom2d = g2(dat);
        } else if (code == 5) {
            obj->xan2d = g2(dat);
        } else if (code == 6) {
            obj->yan2d = g2(dat);
        } else if (code == 7) {
            obj->xof2d = g2(dat);
            if (obj->xof2d > 32767) {
                obj->xof2d -= 65536;
            }
        } else if (code == 8) {
            obj->yof2d = g2(dat);
            if (obj->yof2d > 32767) {
                obj->yof2d -= 65536;
            }
        } else if (code == 9) {
            // animHasAlpha from code10?
            obj->code9 = true;
        } else if (code == 10) {
            // seq?
            obj->code10 = g2(dat);
        } else if (code == 11) {
            obj->stackable = true;
        } else if (code == 12) {
            obj->cost = g4(dat);
        } else if (code == 16) {
            obj->members = true;
        } else if (code == 23) {
            obj->manwear = g2(dat);
            obj->manwearOffsetY = g1b(dat);
        } else if (code == 24) {
            obj->manwear2 = g2(dat);
        } else if (code == 25) {
            obj->womanwear = g2(dat);
            obj->womanwearOffsetY = g1b(dat);
        } else if (code == 26) {
            obj->womanwear2 = g2(dat);
        } else if (code >= 30 && code < 35) {
            if (!obj->op) {
                obj->op = calloc(5, sizeof(char *));
            }

            obj->op[code - 30] = gjstr(dat);
            if (platform_strcasecmp(obj->op[code - 30], "hidden") == 0) {
                obj->op[code - 30] = NULL;
            }
        } else if (code >= 35 && code < 40) {
            if (!obj->iop) {
                obj->iop = calloc(5, sizeof(char *));
            }

            obj->iop[code - 35] = gjstr(dat);
        } else if (code == 40) {
            int count = g1(dat);
            obj->recol_count = count;
            obj->recol_s = calloc(count, sizeof(int));
            obj->recol_d = calloc(count, sizeof(int));

            for (int i = 0; i < count; i++) {
                obj->recol_s[i] = g2(dat);
                obj->recol_d[i] = g2(dat);
            }
        } else if (code == 78) {
            obj->manwear3 = g2(dat);
        } else if (code == 79) {
            obj->womanwear3 = g2(dat);
        } else if (code == 90) {
            obj->manhead = g2(dat);
        } else if (code == 91) {
            obj->womanhead = g2(dat);
        } else if (code == 92) {
            obj->manhead2 = g2(dat);
        } else if (code == 93) {
            obj->womanhead2 = g2(dat);
        } else if (code == 95) {
            obj->zan2d = g2(dat);
        } else if (code == 97) {
            obj->certlink = g2(dat);
        } else if (code == 98) {
            obj->certtemplate = g2(dat);
        } else if (code >= 100 && code < 110) {
            if (!obj->countobj) {
                obj->countobj = calloc(10, sizeof(int));
                obj->countco = calloc(10, sizeof(int));
            }

            obj->countobj[code - 100] = g2(dat);
            obj->countco[code - 100] = g2(dat);
        } else {
            rs2_error("Error unrecognised obj config code: %d\n", code);
        }
    }
}

void objtype_to_certificate(ObjType *obj) {
    ObjType *template = objtype_get(obj->certtemplate);
    obj->model = template->model;
    obj->zoom2d = template->zoom2d;
    obj->xan2d = template->xan2d;
    obj->yan2d = template->yan2d;
    obj->zan2d = template->zan2d;
    obj->xof2d = template->xof2d;
    obj->yof2d = template->yof2d;
    obj->recol_s = template->recol_s;
    obj->recol_d = template->recol_d;

    ObjType *link = objtype_get(obj->certlink);
    strcpy(obj->name, link->name);
    obj->members = link->members;
    obj->cost = link->cost;

    const char *article = "a";
    char c = link->name[0];
    if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') {
        article = "an";
    }
    sprintf(obj->desc, "Swap this note at any bank for %s %s.", article, link->name);

    obj->stackable = true;
}

Model *objtype_get_interfacemodel(ObjType *obj, int count, bool use_allocator) {
    if (obj->countobj && count > 1) {
        int id = -1;
        for (int i = 0; i < 10; i++) {
            if (count >= obj->countco[i] && obj->countco[i] != 0) {
                id = obj->countobj[i];
            }
        }

        if (id != -1) {
            return objtype_get_interfacemodel(objtype_get(id), 1, use_allocator);
        }
    }

    Model *model = (Model *)lrucache_get(_ObjType.modelCache, obj->index);
    if (model) {
        return model;
    }

    model = model_from_id(obj->model, use_allocator);
    if (obj->recol_s) {
        for (int i = 0; i < obj->recol_count; i++) {
            model_recolor(model, obj->recol_s[i], obj->recol_d[i]);
        }
    }

    model_calculate_normals(model, 64, 768, -50, -10, -50, true, use_allocator);
    model->pickable = true;
    lrucache_put(_ObjType.modelCache, obj->index, &model->link);
    return model;
}

Model *objtype_get_wornmodel(ObjType *obj, int gender) {
    int id1 = obj->manwear;
    if (gender == 1) {
        id1 = obj->womanwear;
    }

    if (id1 == -1) {
        return NULL;
    }

    int id2 = obj->manwear2;
    int id3 = obj->manwear3;
    if (gender == 1) {
        id2 = obj->womanwear2;
        id3 = obj->womanwear3;
    }

    Model *model = model_from_id(id1, false);
    if (id2 != -1) {
        Model *model2 = model_from_id(id2, false);

        if (id3 == -1) {
            Model *models[] = {model, model2};
            model = model_from_models(models, 2, false);
            model_free(models[0]);
        } else {
            Model *model3 = model_from_id(id3, false);
            Model *models[] = {model, model2, model3};
            model = model_from_models(models, 3, false);
            model_free(models[0]);
            model_free(model3);
        }
        model_free(model2);
    }

    if (gender == 0 && obj->manwearOffsetY != 0) {
        model_translate(model, obj->manwearOffsetY, 0, 0);
    }

    if (gender == 1 && obj->womanwearOffsetY != 0) {
        model_translate(model, obj->womanwearOffsetY, 0, 0);
    }

    if (obj->recol_s) {
        for (int i = 0; i < obj->recol_count; i++) {
            model_recolor(model, obj->recol_s[i], obj->recol_d[i]);
        }
    }

    return model;
}

Model *objtype_get_headmodel(ObjType *obj, int gender) {
    int head1 = obj->manhead;
    if (gender == 1) {
        head1 = obj->womanhead;
    }

    if (head1 == -1) {
        return NULL;
    }

    int head2 = obj->manhead2;
    if (gender == 1) {
        head2 = obj->womanhead2;
    }

    Model *model = model_from_id(head1, false);
    if (head2 != -1) {
        Model *model2 = model_from_id(head2, false);
        Model *models[] = {model, model2};
        model = model_from_models(models, 2, false);
    }

    if (obj->recol_s) {
        for (int i = 0; i < obj->recol_count; i++) {
            model_recolor(model, obj->recol_s[i], obj->recol_d[i]);
        }
    }

    return model;
}
