#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "component.h"
#include "datastruct/jstring.h"
#include "datastruct/lrucache.h"
#include "jagfile.h"
#include "model.h"
#include "platform.h"

ComponentData _Component = {0};

void component_free_global(void) {
    for (int i = 0; i < _Component.count; i++) {
        if (_Component.instances[i]) {
            for (int j = 0; j < _Component.instances[i]->scriptCount; j++) {
                free(_Component.instances[i]->scripts[j]);
            }
            free(_Component.instances[i]->scripts);
            if (_Component.instances[i]->iops) {
                for (int j = 0; j < 5; j++) {
                    free(_Component.instances[i]->iops[j]);
                }
                free(_Component.instances[i]->iops);
            }
            free(_Component.instances[i]->childId);
            free(_Component.instances[i]->childX);
            free(_Component.instances[i]->childY);
            free(_Component.instances[i]->scriptComparator);
            free(_Component.instances[i]->scriptOperand);
            if (_Component.instances[i]->actionVerb) {
                free(_Component.instances[i]->actionVerb);
                free(_Component.instances[i]->action);
            }
            if (_Component.instances[i]->activeText) {
                free(_Component.instances[i]->activeText);
            }
            // TODO can't free these due to them being modified in packets
            if (_Component.instances[i]->invSlotObjCount) {
                // for (int j = 0; j < 20; j++) {
                //     if (_Component.instances[i]->invSlotSprite && _Component.instances[i]->invSlotSprite[j]) {
                //         pix24_free(_Component.instances[i]->invSlotSprite[j]);
                //     }
                // }
                free(_Component.instances[i]->invSlotOffsetX);
                free(_Component.instances[i]->invSlotOffsetY);
                free(_Component.instances[i]->invSlotSprite);
                free(_Component.instances[i]->invSlotObjId);
                free(_Component.instances[i]->invSlotObjCount);
            }
            // if (_Component.instances[i]->model) {
            //     model_free(_Component.instances[i]->model);
            // }
            // if (_Component.instances[i]->activeModel) {
            //     model_free(_Component.instances[i]->activeModel);
            // }
            // if (_Component.instances[i]->graphic) {
            //     pix24_free(_Component.instances[i]->graphic);
            // }
            // if (_Component.instances[i]->activeGraphic) {
            //     pix24_free(_Component.instances[i]->activeGraphic);
            // }
        }
        free(_Component.instances[i]);
    }
    free(_Component.instances);
}

void component_unpack(Jagfile *jag, Jagfile *media, PixFont **fonts) {
    _Component.imageCache = lrucache_new(50000);
    _Component.modelCache = lrucache_new(50000);

    Packet *dat = jagfile_to_packet(jag, "data");
    int layer = -1;

    _Component.count = g2(dat);
    _Component.instances = calloc(_Component.count, sizeof(Component *));

    while (dat->pos < dat->length) {
        int id = g2(dat);
        if (id == 65535) {
            layer = g2(dat);
            id = g2(dat);
        }

        Component *com = _Component.instances[id] = calloc(1, sizeof(Component));
        com->id = id;
        com->layer = layer;
        com->type = g1(dat);
        com->buttonType = g1(dat);
        com->clientCode = g2(dat);
        com->width = g2(dat);
        com->height = g2(dat);
        com->overLayer = g1(dat);
        if (com->overLayer == 0) {
            com->overLayer = -1;
        } else {
            com->overLayer = ((com->overLayer - 1) << 8) + g1(dat);
        }

        com->comparatorCount = g1(dat);
        if (com->comparatorCount > 0) {
            com->scriptComparator = calloc(com->comparatorCount, sizeof(int));
            com->scriptOperand = calloc(com->comparatorCount, sizeof(int));

            for (int i = 0; i < com->comparatorCount; i++) {
                com->scriptComparator[i] = g1(dat);
                com->scriptOperand[i] = g2(dat);
            }
        }

        com->scriptCount = g1(dat);
        if (com->scriptCount > 0) {
            com->scripts = calloc(com->scriptCount, sizeof(int *));

            for (int i = 0; i < com->scriptCount; i++) {
                int opcodeCount = g2(dat);
                com->scripts[i] = calloc(opcodeCount, sizeof(int));

                for (int j = 0; j < opcodeCount; j++) {
                    com->scripts[i][j] = g2(dat);
                }
            }
        }

        if (com->type == TYPE_LAYER) {
            com->scroll = g2(dat);
            com->hide = g1(dat) == 1;

            com->childCount = g1(dat);
            com->childId = calloc(com->childCount, sizeof(int));
            com->childX = calloc(com->childCount, sizeof(int));
            com->childY = calloc(com->childCount, sizeof(int));

            for (int i = 0; i < com->childCount; i++) {
                com->childId[i] = g2(dat);
                com->childX[i] = g2b(dat);
                com->childY[i] = g2b(dat);
            }
        }

        if (com->type == TYPE_UNUSED) {
            com->unusedShort1 = g2(dat);
            com->unusedBoolean1 = g1(dat) == 1;
        }

        if (com->type == TYPE_INV) {
            com->invSlotObjId = calloc(com->width * com->height, sizeof(int));
            com->invSlotObjCount = calloc(com->width * com->height, sizeof(int));

            com->draggable = g1(dat) == 1;
            com->interactable = g1(dat) == 1;
            com->usable = g1(dat) == 1;
            com->marginX = g1(dat);
            com->marginY = g1(dat);

            com->invSlotOffsetX = calloc(20, sizeof(int));
            com->invSlotOffsetY = calloc(20, sizeof(int));
            com->invSlotSprite = calloc(20, sizeof(Pix24 *));

            for (int i = 0; i < 20; i++) {
                if (g1(dat) == 1) {
                    com->invSlotOffsetX[i] = g2b(dat);
                    com->invSlotOffsetY[i] = g2b(dat);

                    char *sprite = gjstr(dat);
                    if (media && strlen(sprite) > 0) {
                        int sprite_index = (int)(strrchr(sprite, ',') - sprite);
                        char *sprite_name = substring(sprite, 0, sprite_index);
                        char *sprite_id = substring(sprite, sprite_index + 1, strlen(sprite));
                        com->invSlotSprite[i] = component_get_image(media, sprite_name, atoi(sprite_id));
                        free(sprite_name);
                        free(sprite_id);
                    }
                    free(sprite);
                }
            }

            com->iops = calloc(5, sizeof(char *));
            for (int i = 0; i < 5; i++) {
                com->iops[i] = gjstr(dat);

                if (strlen(com->iops[i]) == 0) {
                    free(com->iops[i]);
                    com->iops[i] = NULL;
                }
            }
        }

        if (com->type == TYPE_RECT) {
            com->fill = g1(dat) == 1;
        }

        if (com->type == TYPE_TEXT || com->type == TYPE_UNUSED) {
            com->center = g1(dat) == 1;
            int fontId = g1(dat);
            if (fonts) {
                com->font = fonts[fontId];
            }
            com->shadowed = g1(dat) == 1;
        }

        if (com->type == TYPE_TEXT) {
            char *text = gjstr(dat);
            strcpy(com->text, text);
            free(text);
            com->activeText = gjstr(dat);
        }

        if (com->type == TYPE_UNUSED || com->type == TYPE_RECT || com->type == TYPE_TEXT) {
            com->colour = g4(dat);
        }

        if (com->type == TYPE_RECT || com->type == TYPE_TEXT) {
            com->activeColour = g4(dat);
            com->overColour = g4(dat);
        }

        if (com->type == TYPE_GRAPHIC) {
            char *sprite = gjstr(dat);
            if (media && strlen(sprite) > 0) {
                int sprite_index = (int)(strrchr(sprite, ',') - sprite);
                char *sprite_name = substring(sprite, 0, sprite_index);
                char *sprite_id = substring(sprite, sprite_index + 1, strlen(sprite));
                com->graphic = component_get_image(media, sprite_name, atoi(sprite_id));
                free(sprite_name);
                free(sprite_id);
            }
            free(sprite);

            sprite = gjstr(dat);
            if (media && strlen(sprite) > 0) {
                int sprite_index = (int)(strrchr(sprite, ',') - sprite);
                char *sprite_name = substring(sprite, 0, sprite_index);
                char *sprite_id = substring(sprite, sprite_index + 1, strlen(sprite));
                com->activeGraphic = component_get_image(media, sprite_name, atoi(sprite_id));
                free(sprite_name);
                free(sprite_id);
            }
            free(sprite);
        }

        if (com->type == TYPE_MODEL) {
            int tmp = g1(dat);
            if (tmp != 0) {
                com->model = component_get_model(((tmp - 1) << 8) + g1(dat));
            }

            tmp = g1(dat);
            if (tmp != 0) {
                com->activeModel = component_get_model(((tmp - 1) << 8) + g1(dat));
            }

            tmp = g1(dat);
            if (tmp == 0) {
                com->anim = -1;
            } else {
                com->anim = ((tmp - 1) << 8) + g1(dat);
            }

            tmp = g1(dat);
            if (tmp == 0) {
                com->activeAnim = -1;
            } else {
                com->activeAnim = ((tmp - 1) << 8) + g1(dat);
            }

            com->zoom = g2(dat);
            com->xan = g2(dat);
            com->yan = g2(dat);
        }

        if (com->type == TYPE_INV_TEXT) {
            com->invSlotObjId = calloc(com->width * com->height, sizeof(int));
            com->invSlotObjCount = calloc(com->width * com->height, sizeof(int));

            com->center = g1(dat) == 1;
            int fontId = g1(dat);
            if (fonts) {
                com->font = fonts[fontId];
            }
            com->shadowed = g1(dat) == 1;
            com->colour = g4(dat);
            com->marginX = g2b(dat);
            com->marginY = g2b(dat);
            com->interactable = g1(dat) == 1;

            com->iops = calloc(5, sizeof(char *));
            for (int i = 0; i < 5; i++) {
                com->iops[i] = gjstr(dat);

                if (strlen(com->iops[i]) == 0) {
                    free(com->iops[i]);
                    com->iops[i] = NULL;
                }
            }
        }

        if (com->buttonType == BUTTON_TARGET || com->type == TYPE_INV) {
            com->actionVerb = gjstr(dat);
            com->action = gjstr(dat);
            com->actionTarget = g2(dat);
        }

        if (com->buttonType == BUTTON_OK || com->buttonType == BUTTON_TOGGLE || com->buttonType == BUTTON_SELECT || com->buttonType == BUTTON_CONTINUE) {
            char *option = gjstr(dat);
            strcpy(com->option, option);

            if (strlen(com->option) == 0) {
                if (com->buttonType == BUTTON_OK) {
                    strcpy(com->option, "Ok");
                } else if (com->buttonType == BUTTON_TOGGLE) {
                    strcpy(com->option, "Select");
                } else if (com->buttonType == BUTTON_SELECT) {
                    strcpy(com->option, "Select");
                } else if (com->buttonType == BUTTON_CONTINUE) {
                    strcpy(com->option, "Continue");
                }
            }
            free(option);
        }
    }

    packet_free(dat);
    lrucache_free(_Component.imageCache);
    lrucache_free(_Component.modelCache);
}

Pix24 *component_get_image(Jagfile *media, char *sprite, int spriteId) {
    int64_t uid = (jstring_hash_code(sprite) << 8) + (int64_t)spriteId;
    Pix24 *image = (Pix24 *)lrucache_get(_Component.imageCache, uid);
    if (image) {
        return image;
    }

    // try {
    image = pix24_from_archive(media, sprite, spriteId);
    lrucache_put(_Component.imageCache, uid, &image->link);
    // } catch (Exception ignored) {
    // 	return null;
    // }

    return image;
}

Model *component_get_model(int id) {
    Model *m = (Model *)lrucache_get(_Component.modelCache, id);
    if (m) {
        return m;
    }

    m = model_from_id(id, false);
    lrucache_put(_Component.modelCache, id, &m->link);
    return m;
}

Model *component_get_model2(Component *com, int primaryFrame, int secondaryFrame, bool active, bool *_free) {
    Model *m = com->model;
    if (active) {
        m = com->activeModel;
    }

    if (!m) {
        return NULL;
    }

    if (primaryFrame == -1 && secondaryFrame == -1 && !m->face_colors) {
        return m;
    }

    *_free = true;
    Model *tmp = model_share_colored(m, true, true, false, false);
    if (primaryFrame != -1 || secondaryFrame != -1) {
        model_create_label_references(tmp, false);
    }

    if (primaryFrame != -1) {
        model_apply_transform(tmp, primaryFrame);
    }

    if (secondaryFrame != -1) {
        model_apply_transform(tmp, secondaryFrame);
    }

    model_calculate_normals(tmp, 64, 768, -50, -10, -50, true, false);
    return tmp;
}
