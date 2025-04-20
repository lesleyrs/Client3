#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator.h"
#include "animbase.h"
#include "animframe.h"
#include "defines.h"
#include "jagfile.h"
#include "model.h"
#include "pix2d.h"
#include "pix3d.h"
#include "platform.h"

ModelData _Model = {0};
extern Pix3D _Pix3D;
extern Pix2D _Pix2D;
extern AnimFrameData _AnimFrame;

void model_init_global(void) {
    _Model.face_clipped_x = calloc(4096, sizeof(bool));
    _Model.face_near_clipped = calloc(4096, sizeof(bool));
    _Model.vertex_screen_x = calloc(4096, sizeof(int));
    _Model.vertex_screen_y = calloc(4096, sizeof(int));
    _Model.vertex_screen_z = calloc(4096, sizeof(int));
    _Model.vertex_view_space_x = calloc(4096, sizeof(int));
    _Model.vertex_view_space_y = calloc(4096, sizeof(int));
    _Model.vertex_view_space_z = calloc(4096, sizeof(int));
    _Model.tmp_depth_face_count = calloc(MODEL_MAX_DEPTH, sizeof(int));
    _Model.tmp_depth_faces = calloc(MODEL_MAX_DEPTH, sizeof(int *));
    for (int i = 0; i < MODEL_MAX_DEPTH; i++) {
        _Model.tmp_depth_faces[i] = calloc(512, sizeof(int));
    }
    _Model.tmp_priority_face_count = calloc(12, sizeof(int));
    _Model.tmp_priority_faces = calloc(12, sizeof(int *));
    for (int i = 0; i < 12; i++) {
        _Model.tmp_priority_faces[i] = calloc(2000, sizeof(int));
    }
    _Model.tmp_priority10_face_depth = calloc(2000, sizeof(int));
    _Model.tmp_priority11_face_depth = calloc(2000, sizeof(int));
    _Model.tmp_priority_depth_sum = calloc(12, sizeof(int));
    _Model.clipped_x = calloc(10, sizeof(int));
    _Model.clipped_y = calloc(10, sizeof(int));
    _Model.clipped_color = calloc(10, sizeof(int));
    _Model.picked_bitsets = calloc(1000, sizeof(int));

    _Model.base_x = 0;
    _Model.base_y = 0;
    _Model.base_z = 0;
    _Model.check_hover = false;
    _Model.mouse_x = 0;
    _Model.mouse_y = 0;
    _Model.picked_count = 0;
}

void model_free_global(void) {
    for (int i = 0; i < _Model.metadata_count; i++) {
        free(_Model.metadata[i]);
    }
    free(_Model.metadata);
    free(_Model.face_clipped_x);
    free(_Model.face_near_clipped);
    free(_Model.vertex_screen_x);
    free(_Model.vertex_screen_y);
    free(_Model.vertex_screen_z);
    free(_Model.vertex_view_space_x);
    free(_Model.vertex_view_space_y);
    free(_Model.vertex_view_space_z);
    free(_Model.tmp_depth_face_count);
    for (int i = 0; i < MODEL_MAX_DEPTH; i++) {
        free(_Model.tmp_depth_faces[i]);
    }
    free(_Model.tmp_depth_faces);
    free(_Model.tmp_priority_face_count);
    for (int i = 0; i < 12; i++) {
        free(_Model.tmp_priority_faces[i]);
    }
    free(_Model.tmp_priority_faces);
    free(_Model.tmp_priority10_face_depth);
    free(_Model.tmp_priority11_face_depth);
    free(_Model.tmp_priority_depth_sum);
    free(_Model.clipped_x);
    free(_Model.clipped_y);
    free(_Model.clipped_color);
    free(_Model.picked_bitsets);

    packet_free(_Model.head);
    packet_free(_Model.face1);
    packet_free(_Model.face2);
    packet_free(_Model.face3);
    packet_free(_Model.face4);
    packet_free(_Model.face5);
    packet_free(_Model.point1);
    packet_free(_Model.point2);
    packet_free(_Model.point3);
    packet_free(_Model.point4);
    packet_free(_Model.point5);
    packet_free(_Model.vertex1);
    packet_free(_Model.vertex2);
    packet_free(_Model.axis);
}

void model_free_calculate_normals(Model *m) {
    // TODO: calculate_normals is allocating more memory and setting it to null in apply_lighting
    free(m->face_color_a);
    free(m->face_color_b);
    free(m->face_color_c);
}

void model_free_label_references(Model *m) {
    free(m->label_vertices_index_count);
    free(m->label_faces_index_count);
    for (int i = 0; i < m->label_vertices_count; i++) {
        free(m->label_vertices[i]);
    }
    free(m->label_vertices);
    for (int i = 0; i < m->label_faces_count; i++) {
        free(m->label_faces[i]);
    }
    free(m->label_faces);
    // TODO: not needed?
    // m->label_faces_count = 0;
    // m->label_vertices_count = 0;
    // m->label_faces_index_count = NULL;
    // m->label_vertices_index_count = NULL;
    // NOTE these are in the original place already, exception is npctype/playerentity which create label refs before being put in cache
    // m->label_faces = NULL;
    // m->label_vertices = NULL;
}

void model_free_copy_faces(Model *m, bool copyVertexY, bool copyFaces) {
    if (copyVertexY) {
        free(m->vertices_y);
    }

    if (copyFaces) {
        free(m->face_color_a);
        free(m->face_color_b);
        free(m->face_color_c);
        free(m->face_infos);
        for (int v = 0; v < m->vertex_count; v++) {
            free(m->vertex_normal[v]);
        }
        free(m->vertex_normal);
    }
    free(m);
}

void model_free_share_colored(Model *m, bool shareColors, bool shareAlpha, bool shareVertices) {
    if (!shareVertices) {
        free(m->vertices_x);
        free(m->vertices_y);
        free(m->vertices_z);
    }

    if (!shareColors) {
        free(m->face_colors);
    }

    if (!shareAlpha) {
        free(m->face_alphas);
    }
    free(m);
}

void model_free_share_alpha(Model *m, bool shareAlpha) {
    free(m->vertices_x);
    free(m->vertices_y);
    free(m->vertices_z);

    if (!shareAlpha) {
        free(m->face_alphas);
    }
    free(m);
}

void model_free(Model *model) {
    free(model->vertices_x);
    free(model->vertices_y);
    free(model->vertices_z);
    free(model->face_indices_a);
    free(model->face_indices_b);
    free(model->face_indices_c);
    free(model->textured_p_coordinate);
    free(model->textured_m_coordinate);
    free(model->textured_n_coordinate);

    free(model->vertex_labels);
    free(model->face_infos);
    free(model->face_priorities);
    free(model->face_alphas);
    free(model->face_labels);

    free(model->face_colors);
    free(model->face_color_a);
    free(model->face_color_b);
    free(model->face_color_c);
    // TODO yes or no
    // for (int v = 0; v < model->vertex_count; v++) {
    //  free(model->vertex_normal[v]);
    // }
    free(model->vertex_normal);
    // for (int v = 0; v < model->vertex_count; v++) {
    // 	free(model->vertex_normal_original[v]);
    // }
    free(model->vertex_normal_original);
    free(model);
}

void model_unpack(Jagfile *models) {
    // try {
    _Model.head = jagfile_to_packet(models, "ob_head.dat");
    _Model.face1 = jagfile_to_packet(models, "ob_face1.dat");
    _Model.face2 = jagfile_to_packet(models, "ob_face2.dat");
    _Model.face3 = jagfile_to_packet(models, "ob_face3.dat");
    _Model.face4 = jagfile_to_packet(models, "ob_face4.dat");
    _Model.face5 = jagfile_to_packet(models, "ob_face5.dat");
    _Model.point1 = jagfile_to_packet(models, "ob_point1.dat");
    _Model.point2 = jagfile_to_packet(models, "ob_point2.dat");
    _Model.point3 = jagfile_to_packet(models, "ob_point3.dat");
    _Model.point4 = jagfile_to_packet(models, "ob_point4.dat");
    _Model.point5 = jagfile_to_packet(models, "ob_point5.dat");
    _Model.vertex1 = jagfile_to_packet(models, "ob_vertex1.dat");
    _Model.vertex2 = jagfile_to_packet(models, "ob_vertex2.dat");
    _Model.axis = jagfile_to_packet(models, "ob_axis.dat");

    _Model.head->pos = 0;
    _Model.point1->pos = 0;
    _Model.point2->pos = 0;
    _Model.point3->pos = 0;
    _Model.point4->pos = 0;
    _Model.vertex1->pos = 0;
    _Model.vertex2->pos = 0;
    int count = g2(_Model.head);
    _Model.metadata_count = count + 100;
    _Model.metadata = calloc(_Model.metadata_count, sizeof(Metadata *));

    int vertex_texture_data_offset = 0;
    int label_data_offset = 0;
    int triangle_color_data_offset = 0;
    int triangle_info_data_offset = 0;
    int triangle_priority_data_offset = 0;
    int triangle_alpha_data_offset = 0;
    int triangle_skin_data_offset = 0;

    for (int n = 0; n < count; n++) {
        int id = g2(_Model.head);
        Metadata *meta = _Model.metadata[id] = calloc(1, sizeof(Metadata));
        meta->vertex_count = g2(_Model.head);
        meta->face_count = g2(_Model.head);
        meta->textured_face_count = g1(_Model.head);
        meta->vertex_flags_offset = _Model.point1->pos;
        meta->vertex_x_offset = _Model.point2->pos;
        meta->vertex_y_offset = _Model.point3->pos;
        meta->vertex_z_offset = _Model.point4->pos;
        meta->face_vertices_offset = _Model.vertex1->pos;
        meta->face_orientations_offset = _Model.vertex2->pos;
        int has_info = g1(_Model.head);
        int priority = g1(_Model.head);
        int has_alpha = g1(_Model.head);
        int has_skins = g1(_Model.head);
        int has_labels = g1(_Model.head);
        for (int v = 0; v < meta->vertex_count; v++) {
            const int flags = g1(_Model.point1);
            if ((flags & 0x1) != 0) {
                gsmart(_Model.point2);
            }
            if ((flags & 0x2) != 0) {
                gsmart(_Model.point3);
            }
            if ((flags & 0x4) != 0) {
                gsmart(_Model.point4);
            }
        }
        for (int v = 0; v < meta->face_count; v++) {
            const int type = g1(_Model.vertex2);
            if (type == 1) {
                gsmart(_Model.vertex1);
                gsmart(_Model.vertex1);
            }
            gsmart(_Model.vertex1);
        }
        meta->face_colors_offset = triangle_color_data_offset;
        triangle_color_data_offset += meta->face_count * 2;
        if (has_info == 1) {
            meta->face_infos_offset = triangle_info_data_offset;
            triangle_info_data_offset += meta->face_count;
        } else {
            meta->face_infos_offset = -1;
        }
        if (priority == 255) {
            meta->face_priorities_offset = triangle_priority_data_offset;
            triangle_priority_data_offset += meta->face_count;
        } else {
            meta->face_priorities_offset = -priority - 1;
        }
        if (has_alpha == 1) {
            meta->face_alphas_offset = triangle_alpha_data_offset;
            triangle_alpha_data_offset += meta->face_count;
        } else {
            meta->face_alphas_offset = -1;
        }
        if (has_skins == 1) {
            meta->face_labels_offset = triangle_skin_data_offset;
            triangle_skin_data_offset += meta->face_count;
        } else {
            meta->face_labels_offset = -1;
        }
        if (has_labels == 1) {
            meta->vertex_labels_offset = label_data_offset;
            label_data_offset += meta->vertex_count;
        } else {
            meta->vertex_labels_offset = -1;
        }
        meta->face_texture_axis_offset = vertex_texture_data_offset;
        vertex_texture_data_offset += meta->textured_face_count;
    }
    // } catch (Exception ex) {
    // 	System.out.println("Error loading model index");
    // 	ex.printStackTrace();
    // }
}

Model *model_from_id(int id, bool use_allocator) {
    Model *model = rs2_calloc(use_allocator, 1, sizeof(Model));
    if (_Model.metadata) {
        Metadata *meta = _Model.metadata[id];
        if (!meta) {
            rs2_error("Error model:%d not found!\n", id);
        } else {
            model->vertex_count = meta->vertex_count;
            model->face_count = meta->face_count;
            model->textured_face_count = meta->textured_face_count;
            model->vertices_x = rs2_calloc(use_allocator, meta->vertex_count, sizeof(int));
            model->vertices_y = rs2_calloc(use_allocator, meta->vertex_count, sizeof(int));
            model->vertices_z = rs2_calloc(use_allocator, meta->vertex_count, sizeof(int));
            model->face_indices_a = rs2_calloc(use_allocator, meta->face_count, sizeof(int));
            model->face_indices_b = rs2_calloc(use_allocator, meta->face_count, sizeof(int));
            model->face_indices_c = rs2_calloc(use_allocator, meta->face_count, sizeof(int));
            model->textured_p_coordinate = rs2_calloc(use_allocator, meta->textured_face_count, sizeof(int));
            model->textured_m_coordinate = rs2_calloc(use_allocator, meta->textured_face_count, sizeof(int));
            model->textured_n_coordinate = rs2_calloc(use_allocator, meta->textured_face_count, sizeof(int));
            if (meta->vertex_labels_offset >= 0) {
                model->vertex_labels = rs2_calloc(use_allocator, model->vertex_count, sizeof(int));
            }
            if (meta->face_infos_offset >= 0) {
                model->face_infos = rs2_calloc(use_allocator, model->face_count, sizeof(int));
            }
            if (meta->face_priorities_offset >= 0) {
                model->face_priorities = rs2_calloc(use_allocator, model->face_count, sizeof(int));
            } else {
                model->model_priority = -meta->face_priorities_offset - 1;
            }
            if (meta->face_alphas_offset >= 0) {
                model->face_alphas = rs2_calloc(use_allocator, model->face_count, sizeof(int));
            }
            if (meta->face_labels_offset >= 0) {
                model->face_labels = rs2_calloc(use_allocator, model->face_count, sizeof(int));
            }
            model->face_colors = rs2_calloc(use_allocator, model->face_count, sizeof(int));
            _Model.point1->pos = meta->vertex_flags_offset;
            _Model.point2->pos = meta->vertex_x_offset;
            _Model.point3->pos = meta->vertex_y_offset;
            _Model.point4->pos = meta->vertex_z_offset;
            _Model.point5->pos = meta->vertex_labels_offset;
            int dx = 0;
            int dy = 0;
            int dz = 0;
            int a;
            int b;
            int c;
            for (int v = 0; v < meta->vertex_count; v++) {
                const int flags = g1(_Model.point1);
                a = 0;
                if ((flags & 0x1) != 0) {
                    a = gsmart(_Model.point2);
                }
                b = 0;
                if ((flags & 0x2) != 0) {
                    b = gsmart(_Model.point3);
                }
                c = 0;
                if ((flags & 0x4) != 0) {
                    c = gsmart(_Model.point4);
                }
                model->vertices_x[v] = dx + a;
                model->vertices_y[v] = dy + b;
                model->vertices_z[v] = dz + c;
                dx = model->vertices_x[v];
                dy = model->vertices_y[v];
                dz = model->vertices_z[v];
                if (model->vertex_labels) {
                    model->vertex_labels[v] = g1(_Model.point5);
                }
            }
            _Model.face1->pos = meta->face_colors_offset;
            _Model.face2->pos = meta->face_infos_offset;
            _Model.face3->pos = meta->face_priorities_offset;
            _Model.face4->pos = meta->face_alphas_offset;
            _Model.face5->pos = meta->face_labels_offset;
            for (int f = 0; f < model->face_count; f++) {
                model->face_colors[f] = g2(_Model.face1);
                if (model->face_infos) {
                    model->face_infos[f] = g1(_Model.face2);
                }
                if (model->face_priorities) {
                    model->face_priorities[f] = g1(_Model.face3);
                }
                if (model->face_alphas) {
                    model->face_alphas[f] = g1(_Model.face4);
                }
                if (model->face_labels) {
                    model->face_labels[f] = g1(_Model.face5);
                }
            }
            _Model.vertex1->pos = meta->face_vertices_offset;
            _Model.vertex2->pos = meta->face_orientations_offset;
            a = 0;
            b = 0;
            c = 0;
            int last = 0;
            for (int f = 0; f < model->face_count; f++) {
                const int orientation = g1(_Model.vertex2);
                if (orientation == 1) {
                    a = gsmart(_Model.vertex1) + last;
                    b = gsmart(_Model.vertex1) + a;
                    c = gsmart(_Model.vertex1) + b;
                    last = c;
                }
                if (orientation == 2) {
                    b = c;
                    c = gsmart(_Model.vertex1) + last;
                    last = c;
                }
                if (orientation == 3) {
                    a = c;
                    c = gsmart(_Model.vertex1) + last;
                    last = c;
                }
                if (orientation == 4) {
                    int tmp = a;
                    a = b;
                    b = tmp;
                    c = gsmart(_Model.vertex1) + last;
                    last = c;
                }
                model->face_indices_a[f] = a;
                model->face_indices_b[f] = b;
                model->face_indices_c[f] = c;
            }
            _Model.axis->pos = meta->face_texture_axis_offset * 6;
            for (int f = 0; f < model->textured_face_count; f++) {
                model->textured_p_coordinate[f] = g2(_Model.axis);
                model->textured_m_coordinate[f] = g2(_Model.axis);
                model->textured_n_coordinate[f] = g2(_Model.axis);
            }
        }
    }
    return model;
}

static int add_vertex(Model *src, int vertexId, int *vertexX, int *vertexY, int *vertexZ, int *vertexLabel, int *vertexCount) {
    int identical = -1;
    int x = src->vertices_x[vertexId];
    int y = src->vertices_y[vertexId];
    int z = src->vertices_z[vertexId];
    for (int v = 0; v < *vertexCount; v++) {
        if (x == vertexX[v] && y == vertexY[v] && z == vertexZ[v]) {
            identical = v;
            break;
        }
    }
    if (identical == -1) {
        vertexX[*vertexCount] = x;
        vertexY[*vertexCount] = y;
        vertexZ[*vertexCount] = z;
        if (src->vertex_labels) {
            vertexLabel[*vertexCount] = src->vertex_labels[vertexId];
        }
        identical = (*vertexCount)++;
    }
    return identical;
}

Model *model_from_models(Model **models, int count, bool use_allocator) {
    bool copyInfo = false;
    bool copyPriorities = false;
    bool copyAlpha = false;
    bool copyLabels = false;

    Model *new = rs2_calloc(use_allocator, 1, sizeof(Model));
    new->vertex_count = 0;
    new->face_count = 0;
    new->textured_face_count = 0;
    new->model_priority = -1;

    for (int i = 0; i < count; i++) {
        Model *model = models[i];
        if (model) {
            new->vertex_count += model->vertex_count;
            new->face_count += model->face_count;
            new->textured_face_count += model->textured_face_count;
            copyInfo |= model->face_infos != NULL;

            if (!model->face_priorities) {
                if (new->model_priority == -1) {
                    new->model_priority = model->model_priority;
                }

                if (new->model_priority != model->model_priority) {
                    copyPriorities = true;
                }
            } else {
                copyPriorities = true;
            }

            copyAlpha |= model->face_alphas != NULL;
            copyLabels |= model->face_labels != NULL;
        }
    }

    new->vertices_x = rs2_calloc(use_allocator, new->vertex_count, sizeof(int));
    new->vertices_y = rs2_calloc(use_allocator, new->vertex_count, sizeof(int));
    new->vertices_z = rs2_calloc(use_allocator, new->vertex_count, sizeof(int));
    new->vertex_labels = rs2_calloc(use_allocator, new->vertex_count, sizeof(int));
    new->face_indices_a = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    new->face_indices_b = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    new->face_indices_c = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    new->textured_p_coordinate = rs2_calloc(use_allocator, new->textured_face_count, sizeof(int));
    new->textured_m_coordinate = rs2_calloc(use_allocator, new->textured_face_count, sizeof(int));
    new->textured_n_coordinate = rs2_calloc(use_allocator, new->textured_face_count, sizeof(int));

    if (copyInfo) {
        new->face_infos = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    }

    if (copyPriorities) {
        new->face_priorities = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    }

    if (copyAlpha) {
        new->face_alphas = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    }

    if (copyLabels) {
        new->face_labels = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    }

    new->face_colors = rs2_calloc(use_allocator, new->face_count, sizeof(int));
    new->vertex_count = 0;
    new->face_count = 0;
    new->textured_face_count = 0;

    for (int i = 0; i < count; i++) {
        Model *model = models[i];

        if (model) {
            for (int face = 0; face < model->face_count; face++) {
                if (copyInfo && model->face_infos) {
                    new->face_infos[new->face_count] = model->face_infos[face];
                }

                if (copyPriorities) {
                    if (!model->face_priorities) {
                        new->face_priorities[new->face_count] = model->model_priority;
                    } else {
                        new->face_priorities[new->face_count] = model->face_priorities[face];
                    }
                }

                if (copyAlpha && model->face_alphas) {
                    new->face_alphas[new->face_count] = model->face_alphas[face];
                }

                if (copyLabels && model->face_labels) {
                    new->face_labels[new->face_count] = model->face_labels[face];
                }

                new->face_colors[new->face_count] = model->face_colors[face];
                new->face_indices_a[new->face_count] = add_vertex(model, model->face_indices_a[face], new->vertices_x, new->vertices_y, new->vertices_z, new->vertex_labels, &new->vertex_count);
                new->face_indices_b[new->face_count] = add_vertex(model, model->face_indices_b[face], new->vertices_x, new->vertices_y, new->vertices_z, new->vertex_labels, &new->vertex_count);
                new->face_indices_c[new->face_count] = add_vertex(model, model->face_indices_c[face], new->vertices_x, new->vertices_y, new->vertices_z, new->vertex_labels, &new->vertex_count);
                new->face_count++;
            }

            for (int f = 0; f < model->textured_face_count; f++) {
                new->textured_p_coordinate[new->textured_face_count] = add_vertex(model, model->textured_p_coordinate[f], new->vertices_x, new->vertices_y, new->vertices_z, new->vertex_labels, &new->vertex_count);
                new->textured_m_coordinate[new->textured_face_count] = add_vertex(model, model->textured_m_coordinate[f], new->vertices_x, new->vertices_y, new->vertices_z, new->vertex_labels, &new->vertex_count);
                new->textured_n_coordinate[new->textured_face_count] = add_vertex(model, model->textured_n_coordinate[f], new->vertices_x, new->vertices_y, new->vertices_z, new->vertex_labels, &new->vertex_count);
                new->textured_face_count++;
            }
        }
    }
    return new;
}

Model *model_from_models_bounds(Model **models, int count) {
    bool copyInfo = false;
    bool copyPriority = false;
    bool copyAlpha = false;
    bool copyColor = false;

    Model *new = calloc(1, sizeof(Model));
    new->vertex_count = 0;
    new->face_count = 0;
    new->textured_face_count = 0;
    new->model_priority = -1;

    for (int i = 0; i < count; i++) {
        Model *model = models[i];
        if (model) {
            new->vertex_count += model->vertex_count;
            new->face_count += model->face_count;
            new->textured_face_count += model->textured_face_count;

            copyInfo |= model->face_infos != NULL;

            if (model->face_priorities) {
                if (new->model_priority == -1) {
                    new->model_priority = model->model_priority;
                }
                if (new->model_priority != model->model_priority) {
                    copyPriority = true;
                }
            } else {
                copyPriority = true;
            }

            copyAlpha |= model->face_alphas != NULL;
            copyColor |= model->face_colors != NULL;
        }
    }

    new->vertices_x = calloc(new->vertex_count, sizeof(int));
    new->vertices_y = calloc(new->vertex_count, sizeof(int));
    new->vertices_z = calloc(new->vertex_count, sizeof(int));
    new->face_indices_a = calloc(new->face_count, sizeof(int));
    new->face_indices_b = calloc(new->face_count, sizeof(int));
    new->face_indices_c = calloc(new->face_count, sizeof(int));
    new->face_color_a = calloc(new->face_count, sizeof(int));
    new->face_color_b = calloc(new->face_count, sizeof(int));
    new->face_color_c = calloc(new->face_count, sizeof(int));
    new->textured_p_coordinate = calloc(new->textured_face_count, sizeof(int));
    new->textured_m_coordinate = calloc(new->textured_face_count, sizeof(int));
    new->textured_n_coordinate = calloc(new->textured_face_count, sizeof(int));

    if (copyInfo) {
        new->face_infos = calloc(new->face_count, sizeof(int));
    }

    if (copyPriority) {
        new->face_priorities = calloc(new->face_count, sizeof(int));
    }

    if (copyAlpha) {
        new->face_alphas = calloc(new->face_count, sizeof(int));
    }

    if (copyColor) {
        new->face_colors = calloc(new->face_count, sizeof(int));
    }

    new->vertex_count = 0;
    new->face_count = 0;
    new->textured_face_count = 0;

    int i;
    for (i = 0; i < count; i++) {
        Model *model = models[i];
        if (model) {
            int vertexCount = new->vertex_count;

            for (int v = 0; v < model->vertex_count; v++) {
                new->vertices_x[new->vertex_count] = model->vertices_x[v];
                new->vertices_y[new->vertex_count] = model->vertices_y[v];
                new->vertices_z[new->vertex_count] = model->vertices_z[v];
                new->vertex_count++;
            }

            for (int f = 0; f < model->face_count; f++) {
                new->face_indices_a[new->face_count] = model->face_indices_a[f] + vertexCount;
                new->face_indices_b[new->face_count] = model->face_indices_b[f] + vertexCount;
                new->face_indices_c[new->face_count] = model->face_indices_c[f] + vertexCount;
                new->face_color_a[new->face_count] = model->face_color_a[f];
                new->face_color_b[new->face_count] = model->face_color_b[f];
                new->face_color_c[new->face_count] = model->face_color_c[f];

                if (copyInfo && model->face_infos) {
                    new->face_infos[new->face_count] = model->face_infos[f];
                }

                if (copyPriority) {
                    if (!model->face_priorities) {
                        new->face_priorities[new->face_count] = model->model_priority;
                    } else {
                        new->face_priorities[new->face_count] = model->face_priorities[f];
                    }
                }

                if (copyAlpha && model->face_alphas) {
                    new->face_alphas[new->face_count] = model->face_alphas[f];
                }

                if (copyColor && model->face_colors) {
                    new->face_colors[new->face_count] = model->face_colors[f];
                }

                new->face_count++;
            }

            for (int f = 0; f < model->textured_face_count; f++) {
                new->textured_p_coordinate[new->textured_face_count] = model->textured_p_coordinate[f] + vertexCount;
                new->textured_m_coordinate[new->textured_face_count] = model->textured_m_coordinate[f] + vertexCount;
                new->textured_n_coordinate[new->textured_face_count] = model->textured_n_coordinate[f] + vertexCount;
                new->textured_face_count++;
            }
        }
    }

    model_calculate_bounds_cylinder(new);
    return new;
}

Model *model_share_colored(Model *src, bool shareColors, bool shareAlpha, bool shareVertices, bool use_allocator) {
    Model *new = rs2_calloc(use_allocator, 1, sizeof(Model));
    new->vertex_count = src->vertex_count;
    new->face_count = src->face_count;
    new->textured_face_count = src->textured_face_count;

    if (shareVertices) {
        new->vertices_x = src->vertices_x;
        new->vertices_y = src->vertices_y;
        new->vertices_z = src->vertices_z;
    } else {
        new->vertices_x = rs2_malloc(use_allocator, new->vertex_count * sizeof(int));
        new->vertices_y = rs2_malloc(use_allocator, new->vertex_count * sizeof(int));
        new->vertices_z = rs2_malloc(use_allocator, new->vertex_count * sizeof(int));

        for (int v = 0; v < new->vertex_count; v++) {
            new->vertices_x[v] = src->vertices_x[v];
            new->vertices_y[v] = src->vertices_y[v];
            new->vertices_z[v] = src->vertices_z[v];
        }
    }

    if (shareColors) {
        new->face_colors = src->face_colors;
    } else {
        new->face_colors = rs2_malloc(use_allocator, new->face_count * sizeof(int));
        memcpy(new->face_colors, src->face_colors, new->face_count * sizeof(int));
    }

    if (shareAlpha) {
        new->face_alphas = src->face_alphas;
    } else {
        new->face_alphas = rs2_malloc(use_allocator, new->face_count * sizeof(int));
        if (!src->face_alphas) {
            memset(new->face_alphas, 0, new->face_count * sizeof(int));
        } else {
            memcpy(new->face_alphas, src->face_alphas, new->face_count * sizeof(int));
        }
    }

    new->vertex_labels = src->vertex_labels;
    new->face_labels = src->face_labels;
    new->face_infos = src->face_infos;
    new->face_indices_a = src->face_indices_a;
    new->face_indices_b = src->face_indices_b;
    new->face_indices_c = src->face_indices_c;
    new->face_priorities = src->face_priorities;
    new->model_priority = src->model_priority;
    new->textured_p_coordinate = src->textured_p_coordinate;
    new->textured_m_coordinate = src->textured_m_coordinate;
    new->textured_n_coordinate = src->textured_n_coordinate;
    return new;
}

Model *model_copy_faces(Model *src, bool copyVertexY, bool copyFaces, bool use_allocator) {
    Model *new = rs2_calloc(use_allocator, 1, sizeof(Model));
    new->vertex_count = src->vertex_count;
    new->face_count = src->face_count;
    new->textured_face_count = src->textured_face_count;

    if (copyVertexY) {
        new->vertices_y = rs2_malloc(use_allocator, new->vertex_count * sizeof(int));
        memcpy(new->vertices_y, src->vertices_y, new->vertex_count * sizeof(int));
    } else {
        new->vertices_y = src->vertices_y;
    }

    if (copyFaces) {
        new->face_color_a = rs2_malloc(use_allocator, new->face_count * sizeof(int));
        new->face_color_b = rs2_malloc(use_allocator, new->face_count * sizeof(int));
        new->face_color_c = rs2_malloc(use_allocator, new->face_count * sizeof(int));

        for (int f = 0; f < new->face_count; f++) {
            new->face_color_a[f] = src->face_color_a[f];
            new->face_color_b[f] = src->face_color_b[f];
            new->face_color_c[f] = src->face_color_c[f];
        }

        new->face_infos = rs2_malloc(use_allocator, new->face_count * sizeof(int));
        if (!src->face_infos) {
            memset(new->face_infos, 0, new->face_count * sizeof(int));
        } else {
            memcpy(new->face_infos, src->face_infos, new->face_count * sizeof(int));
        }

        new->vertex_normal = rs2_malloc(use_allocator, new->vertex_count * sizeof(VertexNormal *));
        for (int v = 0; v < new->vertex_count; v++) {
            new->vertex_normal[v] = rs2_malloc(use_allocator, sizeof(VertexNormal));
            *new->vertex_normal[v] = *src->vertex_normal[v];
        }

        new->vertex_normal_original = src->vertex_normal_original;
    } else {
        new->face_color_a = src->face_color_a;
        new->face_color_b = src->face_color_b;
        new->face_color_c = src->face_color_c;
        new->face_infos = src->face_infos;
    }

    new->vertices_x = src->vertices_x;
    new->vertices_z = src->vertices_z;
    new->face_colors = src->face_colors;
    new->face_alphas = src->face_alphas;
    new->face_priorities = src->face_priorities;
    new->model_priority = src->model_priority;
    new->face_indices_a = src->face_indices_a;
    new->face_indices_b = src->face_indices_b;
    new->face_indices_c = src->face_indices_c;
    new->textured_p_coordinate = src->textured_p_coordinate;
    new->textured_m_coordinate = src->textured_m_coordinate;
    new->textured_n_coordinate = src->textured_n_coordinate;
    new->max_y = src->max_y;
    new->min_y = src->min_y;
    new->radius = src->radius;
    new->min_depth = src->min_depth;
    new->max_depth = src->max_depth;
    new->min_x = src->min_x;
    new->max_z = src->max_z;
    new->min_z = src->min_z;
    new->max_x = src->max_x;
    return new;
}

Model *model_share_alpha(Model *src, bool shareAlpha) {
    Model *new = calloc(1, sizeof(Model));
    new->vertex_count = src->vertex_count;
    new->face_count = src->face_count;
    new->textured_face_count = src->textured_face_count;

    new->vertices_x = malloc(new->vertex_count * sizeof(int));
    new->vertices_y = malloc(new->vertex_count * sizeof(int));
    new->vertices_z = malloc(new->vertex_count * sizeof(int));

    for (int v = 0; v < new->vertex_count; v++) {
        new->vertices_x[v] = src->vertices_x[v];
        new->vertices_y[v] = src->vertices_y[v];
        new->vertices_z[v] = src->vertices_z[v];
    }

    if (shareAlpha) {
        new->face_alphas = src->face_alphas;
    } else {
        new->face_alphas = malloc(new->face_count * sizeof(int));
        if (!src->face_alphas) {
            memset(new->face_alphas, 0, new->face_count * sizeof(int));
        } else {
            memcpy(new->face_alphas, src->face_alphas, new->face_count * sizeof(int));
        }
    }

    new->face_infos = src->face_infos;
    new->face_colors = src->face_colors;
    new->face_priorities = src->face_priorities;
    new->model_priority = src->model_priority;
    new->label_faces = src->label_faces;
    new->label_vertices = src->label_vertices;
    new->face_indices_a = src->face_indices_a;
    new->face_indices_b = src->face_indices_b;
    new->face_indices_c = src->face_indices_c;
    new->face_color_a = src->face_color_a;
    new->face_color_b = src->face_color_b;
    new->face_color_c = src->face_color_c;
    new->textured_p_coordinate = src->textured_p_coordinate;
    new->textured_m_coordinate = src->textured_m_coordinate;
    new->textured_n_coordinate = src->textured_n_coordinate;

    // manually update counts
    new->label_faces_count = src->label_faces_count;
    new->label_vertices_count = src->label_vertices_count;
    new->label_vertices_index_count = src->label_vertices_index_count;
    new->label_faces_index_count = src->label_faces_index_count;
    return new;
}

void model_draw_simple(Model *m, int pitch, int yaw, int roll, int eyePitch, int eyeX, int eyeY, int eyeZ) {
    int centerX = _Pix3D.center_x;
    int centerY = _Pix3D.center_y;
    int sinPitch = _Pix3D.sin_table[pitch];
    int cosPitch = _Pix3D.cos_table[pitch];
    int sinYaw = _Pix3D.sin_table[yaw];
    int cosYaw = _Pix3D.cos_table[yaw];
    int sinRoll = _Pix3D.sin_table[roll];
    int cosRoll = _Pix3D.cos_table[roll];
    int sinEyePitch = _Pix3D.sin_table[eyePitch];
    int cosEyePitch = _Pix3D.cos_table[eyePitch];
    int midZ = (eyeY * sinEyePitch + eyeZ * cosEyePitch) >> 16;

    for (int v = 0; v < m->vertex_count; v++) {
        int x = m->vertices_x[v];
        int y = m->vertices_y[v];
        int z = m->vertices_z[v];

        int temp;
        if (roll != 0) {
            temp = (y * sinRoll + x * cosRoll) >> 16;
            y = (y * cosRoll - x * sinRoll) >> 16;
            x = temp;
        }

        if (pitch != 0) {
            temp = (y * cosPitch - z * sinPitch) >> 16;
            z = (y * sinPitch + z * cosPitch) >> 16;
            y = temp;
        }

        if (yaw != 0) {
            temp = (z * sinYaw + x * cosYaw) >> 16;
            z = (z * cosYaw - x * sinYaw) >> 16;
            x = temp;
        }

        x += eyeX;
        y += eyeY;
        z += eyeZ;

        temp = (y * cosEyePitch - z * sinEyePitch) >> 16;
        z = (y * sinEyePitch + z * cosEyePitch) >> 16;

        _Model.vertex_screen_z[v] = z - midZ;
        _Model.vertex_screen_x[v] = centerX + (x << 9) / z;
        _Model.vertex_screen_y[v] = centerY + (temp << 9) / z;

        if (m->textured_face_count > 0) {
            _Model.vertex_view_space_x[v] = x;
            _Model.vertex_view_space_y[v] = temp;
            _Model.vertex_view_space_z[v] = z;
        }
    }

    // try {
    model_draw2(m, false, false, 0);
    // } catch (@Pc(223) Exception ex) {
    // }
}

void model_draw(Model *m, int yaw, int sinCameraPitch, int cosCameraPitch, int sinCameraYaw, int cosCameraYaw, int sceneX, int sceneY, int sceneZ, int key) {
    int a = (sceneZ * cosCameraYaw - sceneX * sinCameraYaw) >> 16;
    int b = (sceneY * sinCameraPitch + a * cosCameraPitch) >> 16;
    int c = m->radius * cosCameraPitch >> 16;
    int d = b + c;
    if (d <= 50 || b >= 3500) {
        return;
    }
    int e = (sceneZ * sinCameraYaw + sceneX * cosCameraYaw) >> 16;
    int minScreenX = (e - m->radius) << 9;
    if (minScreenX / d >= _Pix2D.center_x) {
        return;
    }
    int maxScreenX = (e + m->radius) << 9;
    if (maxScreenX / d <= -_Pix2D.center_x) {
        return;
    }
    int f = (sceneY * cosCameraPitch - a * sinCameraPitch) >> 16;
    int g = m->radius * sinCameraPitch >> 16;
    int maxScreenY = (f + g) << 9;
    if (maxScreenY / d <= -_Pix2D.center_y) {
        return;
    }
    int h = g + (m->max_y * cosCameraPitch >> 16);
    int minScreenY = (f - h) << 9;
    if (minScreenY / d >= _Pix2D.center_y) {
        return;
    }
    int i = c + (m->max_y * sinCameraPitch >> 16);
    bool project = false;
    if (b - i <= 50) {
        project = true;
    }
    bool hasInput = false;
    int cx;
    int cy;
    int yawsin;
    if (key > 0 && _Model.check_hover) {
        cx = b - c;
        if (cx <= 50) {
            cx = 50;
        }
        if (e > 0) {
            minScreenX /= d;
            maxScreenX /= cx;
        } else {
            maxScreenX /= d;
            minScreenX /= cx;
        }
        if (f > 0) {
            minScreenY /= d;
            maxScreenY /= cx;
        } else {
            maxScreenY /= d;
            minScreenY /= cx;
        }
        cy = _Model.mouse_x - _Pix3D.center_x;
        yawsin = _Model.mouse_y - _Pix3D.center_y;
        if (cy > minScreenX && cy < maxScreenX && yawsin > minScreenY && yawsin < maxScreenY) {
            if (m->pickable) {
                _Model.picked_bitsets[_Model.picked_count++] = key;
            } else {
                hasInput = true;
            }
        }
    }
    cx = _Pix3D.center_x;
    cy = _Pix3D.center_y;
    yawsin = 0;
    int yawcos = 0;
    if (yaw != 0) {
        yawsin = _Pix3D.sin_table[yaw];
        yawcos = _Pix3D.cos_table[yaw];
    }
    for (int v = 0; v < m->vertex_count; v++) {
        int x = m->vertices_x[v];
        int y = m->vertices_y[v];
        int z = m->vertices_z[v];
        int temp;
        if (yaw != 0) {
            temp = (z * yawsin + x * yawcos) >> 16;
            z = (z * yawcos - x * yawsin) >> 16;
            x = temp;
        }
        x += sceneX;
        y += sceneY;
        z += sceneZ;
        temp = (z * sinCameraYaw + x * cosCameraYaw) >> 16;
        z = (z * cosCameraYaw - x * sinCameraYaw) >> 16;
        x = temp;
        temp = (y * cosCameraPitch - z * sinCameraPitch) >> 16;
        z = (y * sinCameraPitch + z * cosCameraPitch) >> 16;

        _Model.vertex_screen_z[v] = z - b;
        if (z >= 50) {
            _Model.vertex_screen_x[v] = cx + (x << 9) / z;
            _Model.vertex_screen_y[v] = cy + (temp << 9) / z;
        } else {
            _Model.vertex_screen_x[v] = -5000;
            project = true;
        }
        if (project || m->textured_face_count > 0) {
            _Model.vertex_view_space_x[v] = x;
            _Model.vertex_view_space_y[v] = temp;
            _Model.vertex_view_space_z[v] = z;
        }
    }
    // try {
    model_draw2(m, project, hasInput, key);
    // } catch ( Exception ignored) {
}

void model_draw2(Model *m, bool projected, bool hasInput, int bitset) {
    // NOTE: added < MODEL_MAX_DEPTH checks for model 714 and for optional smaller depth buffer
    for (int i = 0; i < m->max_depth && i < MODEL_MAX_DEPTH; i++) {
        _Model.tmp_depth_face_count[i] = 0;
    }
    for (int f = 0; f < m->face_count; f++) {
        if (!m->face_infos || m->face_infos[f] != -1) {
            int a = m->face_indices_a[f];
            int b = m->face_indices_b[f];
            int c = m->face_indices_c[f];

            int xa = _Model.vertex_screen_x[a];
            int xb = _Model.vertex_screen_x[b];
            int xc = _Model.vertex_screen_x[c];
            if (projected && (xa == -5000 || xb == -5000 || xc == -5000)) {
                _Model.face_near_clipped[f] = true;
                int depth_average = (_Model.vertex_screen_z[a] + _Model.vertex_screen_z[b] + _Model.vertex_screen_z[c]) / 3 + m->min_depth;
                if (depth_average < MODEL_MAX_DEPTH) {
                    _Model.tmp_depth_faces[depth_average][_Model.tmp_depth_face_count[depth_average]++] = f;
                }
            } else {
                if (hasInput && model_point_within_triangle(_Model.mouse_x, _Model.mouse_y, _Model.vertex_screen_y[a], _Model.vertex_screen_y[b], _Model.vertex_screen_y[c], xa, xb, xc)) {
                    _Model.picked_bitsets[_Model.picked_count++] = bitset;
                    hasInput = false;
                }

                if ((xa - xb) * (_Model.vertex_screen_y[c] - _Model.vertex_screen_y[b]) - (_Model.vertex_screen_y[a] - _Model.vertex_screen_y[b]) * (xc - xb) > 0) {
                    _Model.face_near_clipped[f] = false;
                    _Model.face_clipped_x[f] = xa >= 0 || xb >= 0 || xc >= 0 || xa <= _Pix2D.bound_x || xb <= _Pix2D.bound_x || xc <= _Pix2D.bound_x;
                    int depth_average = (_Model.vertex_screen_z[a] + _Model.vertex_screen_z[b] + _Model.vertex_screen_z[c]) / 3 + m->min_depth;
                    if (depth_average < MODEL_MAX_DEPTH) {
                        _Model.tmp_depth_faces[depth_average][_Model.tmp_depth_face_count[depth_average]++] = f;
                    }
                }
            }
        }
    }
    if (!m->face_priorities) {
        for (int depth = m->max_depth - 1; depth >= 0 && depth < MODEL_MAX_DEPTH; depth--) {
            int count = _Model.tmp_depth_face_count[depth];
            if (count > 0) {
                int *faces = _Model.tmp_depth_faces[depth];
                for (int f = 0; f < count; f++) {
                    model_draw_face(m, faces[f]);
                }
            }
        }
        return;
    }
    for (int priority = 0; priority < 12; priority++) {
        _Model.tmp_priority_face_count[priority] = 0;
        _Model.tmp_priority_depth_sum[priority] = 0;
    }
    for (int depth = m->max_depth - 1; depth >= 0 && depth < MODEL_MAX_DEPTH; depth--) {
        const int face_count = _Model.tmp_depth_face_count[depth];
        if (face_count > 0) {
            int *faces = _Model.tmp_depth_faces[depth];
            for (int i = 0; i < face_count; i++) {
                int priority_depth = faces[i];
                int depth_average = m->face_priorities[priority_depth];
                int priority_face_count = _Model.tmp_priority_face_count[depth_average]++;
                _Model.tmp_priority_faces[depth_average][priority_face_count] = priority_depth;
                if (depth_average < 10) {
                    _Model.tmp_priority_depth_sum[depth_average] += depth;
                } else if (depth_average == 10) {
                    _Model.tmp_priority10_face_depth[priority_face_count] = depth;
                } else {
                    _Model.tmp_priority11_face_depth[priority_face_count] = depth;
                }
            }
        }
    }
    int averagePriorityDepthSum1_2 = 0;
    if (_Model.tmp_priority_face_count[1] > 0 || _Model.tmp_priority_face_count[2] > 0) {
        averagePriorityDepthSum1_2 = (_Model.tmp_priority_depth_sum[1] + _Model.tmp_priority_depth_sum[2]) / (_Model.tmp_priority_face_count[1] + _Model.tmp_priority_face_count[2]);
    }
    int averagePriorityDepthSum3_4 = 0;
    if (_Model.tmp_priority_face_count[3] > 0 || _Model.tmp_priority_face_count[4] > 0) {
        averagePriorityDepthSum3_4 = (_Model.tmp_priority_depth_sum[3] + _Model.tmp_priority_depth_sum[4]) / (_Model.tmp_priority_face_count[3] + _Model.tmp_priority_face_count[4]);
    }
    int averagePriorityDepthSum6_8 = 0;
    if (_Model.tmp_priority_face_count[6] > 0 || _Model.tmp_priority_face_count[8] > 0) {
        averagePriorityDepthSum6_8 = (_Model.tmp_priority_depth_sum[6] + _Model.tmp_priority_depth_sum[8]) / (_Model.tmp_priority_face_count[6] + _Model.tmp_priority_face_count[8]);
    }

    int priority_face = 0;
    int priority_face_count = _Model.tmp_priority_face_count[10];
    int *faces = _Model.tmp_priority_faces[10];
    int *priorities = _Model.tmp_priority10_face_depth;
    if (priority_face_count == 0) {
        priority_face_count = _Model.tmp_priority_face_count[11];
        faces = _Model.tmp_priority_faces[11];
        priorities = _Model.tmp_priority11_face_depth;
    }
    int priority_depth;
    if (priority_face_count > 0) {
        priority_depth = priorities[0];
    } else {
        priority_depth = -1000;
    }
    for (int p = 0; p < 10; p++) {
        while (p == 0 && priority_depth > averagePriorityDepthSum1_2) {
            model_draw_face(m, faces[priority_face++]);
            if (priority_face == priority_face_count && faces != _Model.tmp_priority_faces[11]) {
                priority_face = 0;
                priority_face_count = _Model.tmp_priority_face_count[11];
                faces = _Model.tmp_priority_faces[11];
                priorities = _Model.tmp_priority11_face_depth;
            }
            if (priority_face < priority_face_count) {
                priority_depth = priorities[priority_face];
            } else {
                priority_depth = -1000;
            }
        }
        while (p == 3 && priority_depth > averagePriorityDepthSum3_4) {
            model_draw_face(m, faces[priority_face++]);
            if (priority_face == priority_face_count && faces != _Model.tmp_priority_faces[11]) {
                priority_face = 0;
                priority_face_count = _Model.tmp_priority_face_count[11];
                faces = _Model.tmp_priority_faces[11];
                priorities = _Model.tmp_priority11_face_depth;
            }
            if (priority_face < priority_face_count) {
                priority_depth = priorities[priority_face];
            } else {
                priority_depth = -1000;
            }
        }
        while (p == 5 && priority_depth > averagePriorityDepthSum6_8) {
            model_draw_face(m, faces[priority_face++]);
            if (priority_face == priority_face_count && faces != _Model.tmp_priority_faces[11]) {
                priority_face = 0;
                priority_face_count = _Model.tmp_priority_face_count[11];
                faces = _Model.tmp_priority_faces[11];
                priorities = _Model.tmp_priority11_face_depth;
            }
            if (priority_face < priority_face_count) {
                priority_depth = priorities[priority_face];
            } else {
                priority_depth = -1000;
            }
        }
        int n = _Model.tmp_priority_face_count[p];
        int *tris = _Model.tmp_priority_faces[p];
        for (int f = 0; f < n; f++) {
            model_draw_face(m, tris[f]);
        }
    }
    while (priority_depth != -1000) {
        model_draw_face(m, faces[priority_face++]);
        if (priority_face == priority_face_count && faces != _Model.tmp_priority_faces[11]) {
            priority_face = 0;
            faces = _Model.tmp_priority_faces[11];
            priority_face_count = _Model.tmp_priority_face_count[11];
            priorities = _Model.tmp_priority11_face_depth;
        }
        if (priority_face < priority_face_count) {
            priority_depth = priorities[priority_face];
        } else {
            priority_depth = -1000;
        }
    }
}

void model_draw_face(Model *m, int index) {
    if (_Model.face_near_clipped[index]) {
        model_draw_near_clipped_face(m, index);
        return;
    }
    int a = m->face_indices_a[index];
    int b = m->face_indices_b[index];
    int c = m->face_indices_c[index];
    _Pix3D.clipX = _Model.face_clipped_x[index];
    if (!m->face_alphas) {
        _Pix3D.alpha = 0;
    } else {
        _Pix3D.alpha = m->face_alphas[index];
    }
    int type;
    if (!m->face_infos) {
        type = 0;
    } else {
        type = m->face_infos[index] & 0x3;
    }
    if (type == 0) {
        gouraudTriangle(_Model.vertex_screen_x[a], _Model.vertex_screen_x[b], _Model.vertex_screen_x[c], _Model.vertex_screen_y[a], _Model.vertex_screen_y[b], _Model.vertex_screen_y[c], m->face_color_a[index], m->face_color_b[index], m->face_color_c[index]);
    } else if (type == 1) {
        flatTriangle(_Model.vertex_screen_x[a], _Model.vertex_screen_x[b], _Model.vertex_screen_x[c], _Model.vertex_screen_y[a], _Model.vertex_screen_y[b], _Model.vertex_screen_y[c], _Pix3D.palette[m->face_color_a[index]]);
    } else {
        int t;
        int tA;
        int tB;
        int tC;
        if (type == 2) {
            t = m->face_infos[index] >> 2;
            tA = m->textured_p_coordinate[t];
            tB = m->textured_m_coordinate[t];
            tC = m->textured_n_coordinate[t];
            textureTriangle(_Model.vertex_screen_x[a], _Model.vertex_screen_x[b], _Model.vertex_screen_x[c], _Model.vertex_screen_y[a], _Model.vertex_screen_y[b], _Model.vertex_screen_y[c], m->face_color_a[index], m->face_color_b[index], m->face_color_c[index], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
        } else if (type == 3) {
            t = m->face_infos[index] >> 2;
            tA = m->textured_p_coordinate[t];
            tB = m->textured_m_coordinate[t];
            tC = m->textured_n_coordinate[t];
            textureTriangle(_Model.vertex_screen_x[a], _Model.vertex_screen_x[b], _Model.vertex_screen_x[c], _Model.vertex_screen_y[a], _Model.vertex_screen_y[b], _Model.vertex_screen_y[c], m->face_color_a[index], m->face_color_a[index], m->face_color_a[index], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
        }
    }
}

void model_draw_near_clipped_face(Model *m, int index) {
    int centerX = _Pix3D.center_x;
    int centerY = _Pix3D.center_y;
    int n = 0;
    int a = m->face_indices_a[index];
    int b = m->face_indices_b[index];
    int c = m->face_indices_c[index];
    int zA = _Model.vertex_screen_z[a];
    int zB = _Model.vertex_screen_z[b];
    int zC = _Model.vertex_screen_z[c];
    int xA;
    int xB;
    int xC;
    int yA;
    if (zA >= 50) {
        _Model.clipped_x[0] = _Model.vertex_screen_x[a];
        _Model.clipped_y[0] = _Model.vertex_screen_y[a];
        n++;
        _Model.clipped_color[0] = m->face_color_a[index];
    } else {
        xA = _Model.vertex_screen_x[a];
        xB = _Model.vertex_screen_y[a];
        xC = m->face_color_a[index];
        if (zC >= 50) {
            yA = (50 - zA) * _Pix3D.reciprical16[zC - zA];
            _Model.clipped_x[0] = centerX + ((xA + ((_Model.vertex_screen_x[c] - xA) * yA >> 16)) << 9) / 50;
            _Model.clipped_y[0] = centerY + ((xB + ((_Model.vertex_screen_y[c] - xB) * yA >> 16)) << 9) / 50;
            n++;
            _Model.clipped_color[0] = xC + ((m->face_color_c[index] - xC) * yA >> 16);
        }
        if (zB >= 50) {
            yA = (50 - zA) * _Pix3D.reciprical16[zB - zA];
            _Model.clipped_x[n] = centerX + ((xA + ((_Model.vertex_screen_x[b] - xA) * yA >> 16)) << 9) / 50;
            _Model.clipped_y[n] = centerY + ((xB + ((_Model.vertex_screen_y[b] - xB) * yA >> 16)) << 9) / 50;
            _Model.clipped_color[n++] = xC + ((m->face_color_b[index] - xC) * yA >> 16);
        }
    }
    if (zB >= 50) {
        _Model.clipped_x[n] = _Model.vertex_screen_x[b];
        _Model.clipped_y[n] = _Model.vertex_screen_y[b];
        _Model.clipped_color[n++] = m->face_color_b[index];
    } else {
        xA = _Model.vertex_screen_x[b];
        xB = _Model.vertex_screen_y[b];
        xC = m->face_color_b[index];
        if (zA >= 50) {
            yA = (50 - zB) * _Pix3D.reciprical16[zA - zB];
            _Model.clipped_x[n] = centerX + ((xA + ((_Model.vertex_screen_x[a] - xA) * yA >> 16)) << 9) / 50;
            _Model.clipped_y[n] = centerY + ((xB + ((_Model.vertex_screen_y[a] - xB) * yA >> 16)) << 9) / 50;
            _Model.clipped_color[n++] = xC + ((m->face_color_a[index] - xC) * yA >> 16);
        }
        if (zC >= 50) {
            yA = (50 - zB) * _Pix3D.reciprical16[zC - zB];
            _Model.clipped_x[n] = centerX + ((xA + ((_Model.vertex_screen_x[c] - xA) * yA >> 16)) << 9) / 50;
            _Model.clipped_y[n] = centerY + ((xB + ((_Model.vertex_screen_y[c] - xB) * yA >> 16)) << 9) / 50;
            _Model.clipped_color[n++] = xC + ((m->face_color_c[index] - xC) * yA >> 16);
        }
    }
    if (zC >= 50) {
        _Model.clipped_x[n] = _Model.vertex_screen_x[c];
        _Model.clipped_y[n] = _Model.vertex_screen_y[c];
        _Model.clipped_color[n++] = m->face_color_c[index];
    } else {
        xA = _Model.vertex_screen_x[c];
        xB = _Model.vertex_screen_y[c];
        xC = m->face_color_c[index];
        if (zB >= 50) {
            yA = (50 - zC) * _Pix3D.reciprical16[zB - zC];
            _Model.clipped_x[n] = centerX + ((xA + ((_Model.vertex_screen_x[b] - xA) * yA >> 16)) << 9) / 50;
            _Model.clipped_y[n] = centerY + ((xB + ((_Model.vertex_screen_y[b] - xB) * yA >> 16)) << 9) / 50;
            _Model.clipped_color[n++] = xC + ((m->face_color_b[index] - xC) * yA >> 16);
        }
        if (zA >= 50) {
            yA = (50 - zC) * _Pix3D.reciprical16[zA - zC];
            _Model.clipped_x[n] = centerX + ((xA + ((_Model.vertex_screen_x[a] - xA) * yA >> 16)) << 9) / 50;
            _Model.clipped_y[n] = centerY + ((xB + ((_Model.vertex_screen_y[a] - xB) * yA >> 16)) << 9) / 50;
            _Model.clipped_color[n++] = xC + ((m->face_color_a[index] - xC) * yA >> 16);
        }
    }
    xA = _Model.clipped_x[0];
    xB = _Model.clipped_x[1];
    xC = _Model.clipped_x[2];
    yA = _Model.clipped_y[0];
    int yB = _Model.clipped_y[1];
    int yC = _Model.clipped_y[2];
    if ((xA - xB) * (yC - yB) - (yA - yB) * (xC - xB) <= 0) {
        return;
    }
    _Pix3D.clipX = false;
    int type;
    int t;
    int tA;
    int tB;
    int tC;
    if (n == 3) {
        if (xA < 0 || xB < 0 || xC < 0 || xA > _Pix2D.bound_x || xB > _Pix2D.bound_x || xC > _Pix2D.bound_x) {
            _Pix3D.clipX = true;
        }
        if (!m->face_infos) {
            type = 0;
        } else {
            type = m->face_infos[index] & 0x3;
        }
        if (type == 0) {
            gouraudTriangle(xA, xB, xC, yA, yB, yC, _Model.clipped_color[0], _Model.clipped_color[1], _Model.clipped_color[2]);
        } else if (type == 1) {
            flatTriangle(xA, xB, xC, yA, yB, yC, _Pix3D.palette[m->face_color_a[index]]);
        } else if (type == 2) {
            t = m->face_infos[index] >> 2;
            tA = m->textured_p_coordinate[t];
            tB = m->textured_m_coordinate[t];
            tC = m->textured_n_coordinate[t];
            textureTriangle(xA, xB, xC, yA, yB, yC, _Model.clipped_color[0], _Model.clipped_color[1], _Model.clipped_color[2], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
        } else if (type == 3) {
            t = m->face_infos[index] >> 2;
            tA = m->textured_p_coordinate[t];
            tB = m->textured_m_coordinate[t];
            tC = m->textured_n_coordinate[t];
            textureTriangle(xA, xB, xC, yA, yB, yC, m->face_color_a[index], m->face_color_a[index], m->face_color_a[index], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
        }
    }
    if (n != 4) {
        return;
    }
    if (xA < 0 || xB < 0 || xC < 0 || xA > _Pix2D.bound_x || xB > _Pix2D.bound_x || xC > _Pix2D.bound_x || _Model.clipped_x[3] < 0 || _Model.clipped_x[3] > _Pix2D.bound_x) {
        _Pix3D.clipX = true;
    }
    if (!m->face_infos) {
        type = 0;
    } else {
        type = m->face_infos[index] & 0x3;
    }
    if (type == 0) {
        gouraudTriangle(xA, xB, xC, yA, yB, yC, _Model.clipped_color[0], _Model.clipped_color[1], _Model.clipped_color[2]);
        gouraudTriangle(xA, xC, _Model.clipped_x[3], yA, yC, _Model.clipped_y[3], _Model.clipped_color[0], _Model.clipped_color[2], _Model.clipped_color[3]);
        return;
    }
    if (type == 1) {
        t = _Pix3D.palette[m->face_color_a[index]];
        flatTriangle(xA, xB, xC, yA, yB, yC, t);
        flatTriangle(xA, xC, _Model.clipped_x[3], yA, yC, _Model.clipped_y[3], t);
        return;
    }
    if (type == 2) {
        t = m->face_infos[index] >> 2;
        tA = m->textured_p_coordinate[t];
        tB = m->textured_m_coordinate[t];
        tC = m->textured_n_coordinate[t];
        textureTriangle(xA, xB, xC, yA, yB, yC, _Model.clipped_color[0], _Model.clipped_color[1], _Model.clipped_color[2], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
        textureTriangle(xA, xC, _Model.clipped_x[3], yA, yC, _Model.clipped_y[3], _Model.clipped_color[0], _Model.clipped_color[2], _Model.clipped_color[3], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
        return;
    }
    if (type != 3) {
        return;
    }
    t = m->face_infos[index] >> 2;
    tA = m->textured_p_coordinate[t];
    tB = m->textured_m_coordinate[t];
    tC = m->textured_n_coordinate[t];
    textureTriangle(xA, xB, xC, yA, yB, yC, m->face_color_a[index], m->face_color_a[index], m->face_color_a[index], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
    textureTriangle(xA, xC, _Model.clipped_x[3], yA, yC, _Model.clipped_y[3], m->face_color_a[index], m->face_color_a[index], m->face_color_a[index], _Model.vertex_view_space_x[tA], _Model.vertex_view_space_y[tA], _Model.vertex_view_space_z[tA], _Model.vertex_view_space_x[tB], _Model.vertex_view_space_x[tC], _Model.vertex_view_space_y[tB], _Model.vertex_view_space_y[tC], _Model.vertex_view_space_z[tB], _Model.vertex_view_space_z[tC], m->face_colors[index]);
}

bool model_point_within_triangle(int x, int y, int ya, int yb, int yc, int xa, int xb, int xc) {
    if (y < ya && y < yb && y < yc) {
        return false;
    } else if (y > ya && y > yb && y > yc) {
        return false;
    } else if (x < xa && x < xb && x < xc) {
        return false;
    } else {
        return x <= xa || x <= xb || x <= xc;
    }
}

void model_calculate_normals(Model *m, int light_ambient, int light_attenuation, int lightsrc_x, int lightsrc_y, int lightsrc_z, bool apply_lighting, bool use_allocator) {
    const int lightMagnitude = (int)sqrt(lightsrc_x * lightsrc_x + lightsrc_y * lightsrc_y + lightsrc_z * lightsrc_z);
    const int attenuation = light_attenuation * lightMagnitude >> 8;

    if (!m->face_color_a) {
        m->face_color_a = rs2_calloc(use_allocator, m->face_count, sizeof(int));
        m->face_color_b = rs2_calloc(use_allocator, m->face_count, sizeof(int));
        m->face_color_c = rs2_calloc(use_allocator, m->face_count, sizeof(int));
    }

    if (!m->vertex_normal) {
        m->vertex_normal = rs2_calloc(use_allocator, m->vertex_count, sizeof(VertexNormal *));
        for (int v = 0; v < m->vertex_count; v++) {
            m->vertex_normal[v] = rs2_calloc(use_allocator, 1, sizeof(VertexNormal));
        }
    }

    for (int f = 0; f < m->face_count; f++) {
        int a = m->face_indices_a[f];
        int b = m->face_indices_b[f];
        int c = m->face_indices_c[f];

        int dx_ab = m->vertices_x[b] - m->vertices_x[a];
        int dy_ab = m->vertices_y[b] - m->vertices_y[a];
        int dz_ab = m->vertices_z[b] - m->vertices_z[a];
        int dx_ac = m->vertices_x[c] - m->vertices_x[a];
        int dy_ac = m->vertices_y[c] - m->vertices_y[a];
        int dz_ac = m->vertices_z[c] - m->vertices_z[a];
        int nx = dy_ab * dz_ac - dy_ac * dz_ab;
        int ny = dz_ab * dx_ac - dz_ac * dx_ab;
        int nz = dx_ab * dy_ac - dx_ac * dy_ab;
        while (nx > 8192 || ny > 8192 || nz > 8192 || nx < -8192 || ny < -8192 || nz < -8192) {
            nx >>= 0x1;
            ny >>= 0x1;
            nz >>= 0x1;
        }
        int length = (int)sqrt((double)(nx * nx + ny * ny + nz * nz));
        if (length <= 0) {
            length = 1;
        }
        nx = nx * 256 / length;
        ny = ny * 256 / length;
        nz = nz * 256 / length;
        if (!m->face_infos || (m->face_infos[f] & 0x1) == 0) {
            VertexNormal *n = m->vertex_normal[a];
            n->x += nx;
            n->y += ny;
            n->z += nz;
            n->w++;

            n = m->vertex_normal[b];
            n->x += nx;
            n->y += ny;
            n->z += nz;
            n->w++;

            n = m->vertex_normal[c];
            n->x += nx;
            n->y += ny;
            n->z += nz;
            n->w++;
        } else {
            int lightness = light_ambient + (lightsrc_x * nx + lightsrc_y * ny + lightsrc_z * nz) / (attenuation + attenuation / 2);
            m->face_color_a[f] = model_mul_color_lightness(m->face_colors[f], lightness, m->face_infos[f]);
        }
    }
    if (apply_lighting) {
        model_apply_lighting(m, light_ambient, attenuation, lightsrc_x, lightsrc_y, lightsrc_z);
    } else {
        m->vertex_normal_original = rs2_malloc(use_allocator, m->vertex_count * sizeof(VertexNormal *));
        for (int v = 0; v < m->vertex_count; v++) {
            m->vertex_normal_original[v] = rs2_malloc(use_allocator, sizeof(VertexNormal));
            *m->vertex_normal_original[v] = *m->vertex_normal[v];
        }
    }
    if (apply_lighting) {
        model_calculate_bounds_cylinder(m);
    } else {
        model_calculate_bounds_aabb(m);
    }
}

void model_calculate_bounds_cylinder(Model *m) {
    m->max_y = 0;
    m->radius = 0;
    m->min_y = 0;
    for (int i = 0; i < m->vertex_count; i++) {
        int x = m->vertices_x[i];
        int y = m->vertices_y[i];
        int z = m->vertices_z[i];
        if (-y > m->max_y) {
            m->max_y = -y;
        }
        if (y > m->min_y) {
            m->min_y = y;
        }
        int radius_sqr = x * x + z * z;
        if (radius_sqr > m->radius) {
            m->radius = radius_sqr;
        }
    }
    m->radius = (int)(sqrt((double)m->radius) + 0.99);
    m->min_depth = (int)(sqrt((double)(m->radius * m->radius + m->max_y * m->max_y)) + 0.99);
    m->max_depth = m->min_depth + (int)(sqrt((double)(m->radius * m->radius + m->min_y * m->min_y)) + 0.99);
}

void model_create_label_references(Model *m, bool use_allocator) {
    if (m->vertex_labels) {
        m->label_vertices_index_count = rs2_calloc(use_allocator, 256, sizeof(int));

        int count = 0;
        for (int v = 0; v < m->vertex_count; v++) {
            int label = m->vertex_labels[v];
            int countDebug = m->label_vertices_index_count[label]++;
            (void)countDebug;

            if (label > count) {
                count = label;
            }
        }

        m->label_vertices_count = count + 1;
        m->label_vertices = rs2_calloc(use_allocator, count + 1, sizeof(int *));
        for (int label = 0; label <= count; label++) {
            m->label_vertices[label] = rs2_calloc(use_allocator, m->label_vertices_index_count[label], sizeof(int));
            m->label_vertices_index_count[label] = 0;
        }

        int v = 0;
        while (v < m->vertex_count) {
            int label = m->vertex_labels[v];
            m->label_vertices[label][m->label_vertices_index_count[label]++] = v++;
        }

        m->vertex_labels = NULL;
    }

    if (m->face_labels) {
        m->label_faces_index_count = rs2_calloc(use_allocator, 256, sizeof(int));

        int count = 0;
        for (int f = 0; f < m->face_count; f++) {
            int label = m->face_labels[f];
            int countDebug = m->label_faces_index_count[label]++;
            (void)countDebug;
            if (label > count) {
                count = label;
            }
        }

        m->label_faces_count = count + 1;
        m->label_faces = rs2_calloc(use_allocator, count + 1, sizeof(int *));
        for (int label = 0; label <= count; label++) {
            m->label_faces[label] = rs2_calloc(use_allocator, m->label_faces_index_count[label], sizeof(int));
            m->label_faces_index_count[label] = 0;
        }

        int face = 0;
        while (face < m->face_count) {
            int label = m->face_labels[face];
            m->label_faces[label][m->label_faces_index_count[label]++] = face++;
        }

        m->face_labels = NULL;
    }
}

void model_apply_transform(Model *m, int id) {
    if (m->label_vertices && id != -1) {
        AnimFrame *frame = _AnimFrame.instances[id];
        AnimBase *base = frame->base;

        _Model.base_x = 0;
        _Model.base_y = 0;
        _Model.base_z = 0;

        for (int i = 0; i < frame->length; i++) {
            int group = frame->groups[i];
            model_apply_transform2(m, frame->x[i], frame->y[i], frame->z[i], base->labels[group], base->labels_count[group], base->types[group]);
        }
    }
}

void model_apply_transforms(Model *m, int id, int id2, int *walkmerge) {
    if (id == -1) {
        return;
    }

    if (!walkmerge || id2 == -1) {
        model_apply_transform(m, id);
    } else {
        AnimFrame *frame = _AnimFrame.instances[id];
        AnimFrame *frame2 = _AnimFrame.instances[id2];
        AnimBase *base = frame->base;

        _Model.base_x = 0;
        _Model.base_y = 0;
        _Model.base_z = 0;

        int length = 0;
        int merge = walkmerge[length++];

        for (int i = 0; i < frame->length; i++) {
            int group = frame->groups[i];
            while (group > merge) {
                merge = walkmerge[length++];
            }

            if (group != merge || base->types[group] == OP_BASE) {
                model_apply_transform2(m, frame->x[i], frame->y[i], frame->z[i], base->labels[group], base->labels_count[group], base->types[group]);
            }
        }

        _Model.base_x = 0;
        _Model.base_y = 0;
        _Model.base_z = 0;

        length = 0;
        merge = walkmerge[length++];

        for (int i = 0; i < frame2->length; i++) {
            int group = frame2->groups[i];
            while (group > merge) {
                merge = walkmerge[length++];
            }

            if (group == merge || base->types[group] == OP_BASE) {
                model_apply_transform2(m, frame2->x[i], frame2->y[i], frame2->z[i], base->labels[group], base->labels_count[group], base->types[group]);
            }
        }
    }
}

void model_apply_transform2(Model *m, int x, int y, int z, int *labels, int labels_count, int type) {
    if (type == OP_BASE) {
        int count = 0;
        _Model.base_x = 0;
        _Model.base_y = 0;
        _Model.base_z = 0;

        for (int g = 0; g < labels_count; g++) {
            int label = labels[g];
            if (label < m->label_vertices_count) {
                int *vertices = m->label_vertices[label];
                for (int i = 0; i < m->label_vertices_index_count[label]; i++) {
                    int v = vertices[i];
                    _Model.base_x += m->vertices_x[v];
                    _Model.base_y += m->vertices_y[v];
                    _Model.base_z += m->vertices_z[v];
                    count++;
                }
            }
        }

        if (count > 0) {
            _Model.base_x = _Model.base_x / count + x;
            _Model.base_y = _Model.base_y / count + y;
            _Model.base_z = _Model.base_z / count + z;
        } else {
            _Model.base_x = x;
            _Model.base_y = y;
            _Model.base_z = z;
        }
    } else if (type == OP_TRANSLATE) {
        for (int g = 0; g < labels_count; g++) {
            int label = labels[g];
            if (label >= m->label_vertices_count) {
                continue;
            }

            int *vertices = m->label_vertices[label];
            for (int i = 0; i < m->label_vertices_index_count[label]; i++) {
                int v = vertices[i];
                m->vertices_x[v] += x;
                m->vertices_y[v] += y;
                m->vertices_z[v] += z;
            }
        }
    } else if (type == OP_ROTATE) {
        for (int g = 0; g < labels_count; g++) {
            int label = labels[g];
            if (label >= m->label_vertices_count) {
                continue;
            }

            int *vertices = m->label_vertices[label];
            for (int i = 0; i < m->label_vertices_index_count[label]; i++) {
                int v = vertices[i];
                m->vertices_x[v] -= _Model.base_x;
                m->vertices_y[v] -= _Model.base_y;
                m->vertices_z[v] -= _Model.base_z;

                int pitch = (x & 0xff) * 8;
                int yaw = (y & 0xff) * 8;
                int roll = (z & 0xff) * 8;

                int sin;
                int cos;

                if (roll != 0) {
                    sin = _Pix3D.sin_table[roll];
                    cos = _Pix3D.cos_table[roll];
                    int x_ = (m->vertices_y[v] * sin + m->vertices_x[v] * cos) >> 16;
                    m->vertices_y[v] = (m->vertices_y[v] * cos - m->vertices_x[v] * sin) >> 16;
                    m->vertices_x[v] = x_;
                }

                if (pitch != 0) {
                    sin = _Pix3D.sin_table[pitch];
                    cos = _Pix3D.cos_table[pitch];
                    int y_ = (m->vertices_y[v] * cos - m->vertices_z[v] * sin) >> 16;
                    m->vertices_z[v] = (m->vertices_y[v] * sin + m->vertices_z[v] * cos) >> 16;
                    m->vertices_y[v] = y_;
                }

                if (yaw != 0) {
                    sin = _Pix3D.sin_table[yaw];
                    cos = _Pix3D.cos_table[yaw];
                    int x_ = (m->vertices_z[v] * sin + m->vertices_x[v] * cos) >> 16;
                    m->vertices_z[v] = (m->vertices_z[v] * cos - m->vertices_x[v] * sin) >> 16;
                    m->vertices_x[v] = x_;
                }

                m->vertices_x[v] += _Model.base_x;
                m->vertices_y[v] += _Model.base_y;
                m->vertices_z[v] += _Model.base_z;
            }
        }
    } else if (type == OP_SCALE) {
        for (int g = 0; g < labels_count; g++) {
            int label = labels[g];
            if (label >= m->label_vertices_count) {
                continue;
            }

            int *vertices = m->label_vertices[label];
            for (int i = 0; i < m->label_vertices_index_count[label]; i++) {
                int v = vertices[i];

                m->vertices_x[v] -= _Model.base_x;
                m->vertices_y[v] -= _Model.base_y;
                m->vertices_z[v] -= _Model.base_z;

                m->vertices_x[v] = m->vertices_x[v] * x / 128;
                m->vertices_y[v] = m->vertices_y[v] * y / 128;
                m->vertices_z[v] = m->vertices_z[v] * z / 128;

                m->vertices_x[v] += _Model.base_x;
                m->vertices_y[v] += _Model.base_y;
                m->vertices_z[v] += _Model.base_z;
            }
        }
    } else if (type == OP_ALPHA && (m->label_faces && m->face_alphas)) {
        for (int g = 0; g < labels_count; g++) {
            int label = labels[g];
            if (label >= m->label_faces_count) {
                continue;
            }

            int *triangles = m->label_faces[label];
            for (int i = 0; i < m->label_faces_index_count[label]; i++) {
                int t = triangles[i];

                m->face_alphas[t] += x * 8;
                if (m->face_alphas[t] < 0) {
                    m->face_alphas[t] = 0;
                }

                if (m->face_alphas[t] > 255) {
                    m->face_alphas[t] = 255;
                }
            }
        }
    }
}

void model_rotate_y90(Model *m) {
    for (int v = 0; v < m->vertex_count; v++) {
        int tmp = m->vertices_x[v];
        m->vertices_x[v] = m->vertices_z[v];
        m->vertices_z[v] = -tmp;
    }
}

void model_rotate_x(Model *m, int angle) {
    int sin = _Pix3D.sin_table[angle];
    int cos = _Pix3D.cos_table[angle];
    for (int v = 0; v < m->vertex_count; v++) {
        int tmp = (m->vertices_y[v] * cos - m->vertices_z[v] * sin) >> 16;
        m->vertices_z[v] = (m->vertices_y[v] * sin + m->vertices_z[v] * cos) >> 16;
        m->vertices_y[v] = tmp;
    }
}

void model_translate(Model *m, int y, int x, int z) {
    for (int v = 0; v < m->vertex_count; v++) {
        m->vertices_x[v] += x;
        m->vertices_y[v] += y;
        m->vertices_z[v] += z;
    }
}

void model_recolor(Model *m, int src, int dst) {
    for (int f = 0; f < m->face_count; f++) {
        if (m->face_colors[f] == src) {
            m->face_colors[f] = dst;
        }
    }
}

void model_rotate_y180(Model *m) {
    for (int v = 0; v < m->vertex_count; v++) {
        m->vertices_z[v] = -m->vertices_z[v];
    }

    for (int f = 0; f < m->face_count; f++) {
        int temp = m->face_indices_a[f];
        m->face_indices_a[f] = m->face_indices_c[f];
        m->face_indices_c[f] = temp;
    }
}

void model_scale(Model *m, int x, int y, int z) {
    for (int v = 0; v < m->vertex_count; v++) {
        m->vertices_x[v] = m->vertices_x[v] * x / 128;
        m->vertices_y[v] = m->vertices_y[v] * y / 128;
        m->vertices_z[v] = m->vertices_z[v] * z / 128;
    }
}

void model_calculate_bounds_y(Model *m) {
    m->max_y = 0;
    m->min_y = 0;

    for (int v = 0; v < m->vertex_count; v++) {
        int y = m->vertices_y[v];
        if (-y > m->max_y) {
            m->max_y = -y;
        }
        if (y > m->min_y) {
            m->min_y = y;
        }
    }

    m->min_depth = (int)(sqrt(m->radius * m->radius + m->max_y * m->max_y) + 0.99);
    m->max_depth = m->min_depth + (int)(sqrt(m->radius * m->radius + m->min_y * m->min_y) + 0.99);
}

void model_calculate_bounds_aabb(Model *m) {
    m->max_y = 0;
    m->radius = 0;
    m->min_y = 0;
    m->min_x = 999999;
    m->max_x = -999999;
    m->max_z = -99999;
    m->min_z = 99999;
    for (int v = 0; v < m->vertex_count; v++) {
        int x = m->vertices_x[v];
        int y = m->vertices_y[v];
        int z = m->vertices_z[v];
        if (x < m->min_x) {
            m->min_x = x;
        }
        if (x > m->max_x) {
            m->max_x = x;
        }
        if (z < m->min_z) {
            m->min_z = z;
        }
        if (z > m->max_z) {
            m->max_z = z;
        }
        if (-y > m->max_y) {
            m->max_y = -y;
        }
        if (y > m->min_y) {
            m->min_y = y;
        }
        int radius_sqr = x * x + z * z;
        if (radius_sqr > m->radius) {
            m->radius = radius_sqr;
        }
    }
    m->radius = (int)sqrt((double)m->radius);
    m->min_depth = (int)sqrt((double)(m->radius * m->radius + m->max_y * m->max_y));
    m->max_depth = m->min_depth + (int)sqrt((double)(m->radius * m->radius + m->min_y * m->min_y));
}

int model_mul_color_lightness(int hsl, int scalar, int face_infos) {
    if ((face_infos & 0x2) == 2) {
        if (scalar < 0) {
            scalar = 0;
        } else if (scalar > 127) {
            scalar = 127;
        }
        return 127 - scalar;
    }
    scalar = scalar * (hsl & 0x7f) >> 7;
    if (scalar < 2) {
        scalar = 2;
    } else if (scalar > 126) {
        scalar = 126;
    }
    return (hsl & 0xff80) + scalar;
}

void model_apply_lighting(Model *m, int light_ambient, int light_attenuation, int lightsrc_x, int lightsrc_y, int lightsrc_z) {
    for (int f = 0; f < m->face_count; f++) {
        int a = m->face_indices_a[f];
        int b = m->face_indices_b[f];
        int c = m->face_indices_c[f];
        if (!m->face_infos) {
            int color = m->face_colors[f];
            VertexNormal *n = m->vertex_normal[a];
            int lightness = light_ambient + (lightsrc_x * n->x + lightsrc_y * n->y + lightsrc_z * n->z) / (light_attenuation * n->w);
            m->face_color_a[f] = model_mul_color_lightness(color, lightness, 0);
            n = m->vertex_normal[b];
            lightness = light_ambient + (lightsrc_x * n->x + lightsrc_y * n->y + lightsrc_z * n->z) / (light_attenuation * n->w);
            m->face_color_b[f] = model_mul_color_lightness(color, lightness, 0);
            n = m->vertex_normal[c];
            lightness = light_ambient + (lightsrc_x * n->x + lightsrc_y * n->y + lightsrc_z * n->z) / (light_attenuation * n->w);
            m->face_color_c[f] = model_mul_color_lightness(color, lightness, 0);
        } else if ((m->face_infos[f] & 0x1) == 0) {
            int color = m->face_colors[f];
            int info = m->face_infos[f];
            VertexNormal *n = m->vertex_normal[a];
            int lightness = light_ambient + (lightsrc_x * n->x + lightsrc_y * n->y + lightsrc_z * n->z) / (light_attenuation * n->w);
            m->face_color_a[f] = model_mul_color_lightness(color, lightness, info);
            n = m->vertex_normal[b];
            lightness = light_ambient + (lightsrc_x * n->x + lightsrc_y * n->y + lightsrc_z * n->z) / (light_attenuation * n->w);
            m->face_color_b[f] = model_mul_color_lightness(color, lightness, info);
            n = m->vertex_normal[c];
            lightness = light_ambient + (lightsrc_x * n->x + lightsrc_y * n->y + lightsrc_z * n->z) / (light_attenuation * n->w);
            m->face_color_c[f] = model_mul_color_lightness(color, lightness, info);
        }
    }
    // TODO somehow free vertex_normal before it's null
    // for (int v = 0; v < m->vertex_count; v++) {
    // 	free(m->vertex_normal[v]);
    // }
    // free(m->vertex_normal);
    m->vertex_normal = NULL;
    m->vertex_normal_original = NULL;
    m->vertex_labels = NULL;
    m->face_labels = NULL;
    if (m->face_infos) {
        for (int f = 0; f < m->face_count; f++) {
            if ((m->face_infos[f] & 0x2) == 2) {
                return;
            }
        }
    }
    m->face_colors = NULL;
}
