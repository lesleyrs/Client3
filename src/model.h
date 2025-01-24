#pragma once

#include <stdbool.h>

#include "jagfile.h"
#include "packet.h"

typedef struct {
    int vertex_count;
    int face_count;
    int textured_face_count;
    int vertex_flags_offset;
    int vertex_x_offset;
    int vertex_y_offset;
    int vertex_z_offset;
    int vertex_labels_offset;
    int face_vertices_offset;
    int face_orientations_offset;
    int face_colors_offset;
    int face_infos_offset;
    int face_priorities_offset;
    int face_alphas_offset;
    int face_labels_offset;
    int face_texture_axis_offset;
} Metadata;

typedef struct {
    int x;
    int y;
    int z;
    int w;
} VertexNormal;

typedef struct {
    DoublyLinkable link;
    int vertex_count;
    int *vertex_x;
    int *vertex_y;
    int *vertex_z;
    int face_count;
    int *face_vertex_a;
    int *face_vertex_b;
    int *face_vertex_c;
    int *face_color_a;
    int *face_color_b;
    int *face_color_c;
    int *face_info;
    int *face_priority;
    int *face_alpha;
    int *face_color;
    int priority;
    int textured_face_count;
    int *textured_vertex_a;
    int *textured_vertex_b;
    int *textured_vertex_c;

    int min_x;
    int max_x;
    int min_z;
    int max_z;
    int radius;
    int max_y;
    int min_y;
    int max_depth;
    int min_depth;
    int *vertex_label;
    int *face_label;
    int **label_vertices;
    int **label_faces;
    VertexNormal **vertex_normal;
    VertexNormal **vertex_normal_original;
    int obj_raise;
    bool pickable;

    int label_vertices_count;
    int label_faces_count;
    int *label_vertices_index_count;
    int *label_faces_index_count;
} Model;

typedef struct {
    int metadata_count;
    Metadata **metadata;
    Packet *head;
    Packet *face1;
    Packet *face2;
    Packet *face3;
    Packet *face4;
    Packet *face5;
    Packet *point1;
    Packet *point2;
    Packet *point3;
    Packet *point4;
    Packet *point5;
    Packet *vertex1;
    Packet *vertex2;
    Packet *axis;
    bool *face_clipped_x;
    bool *face_near_clipped;
    int *vertex_screen_x;
    int *vertex_screen_y;
    int *vertex_screen_z;
    int *vertex_view_space_x;
    int *vertex_view_space_y;
    int *vertex_view_space_z;
    int *tmp_depth_face_count;
    int **tmp_depth_faces;
    int *tmp_priority_face_count;
    int **tmp_priority_faces;
    int *tmp_priority10_face_depth;
    int *tmp_priority11_face_depth;
    int *tmp_priority_depth_sum;
    int *clipped_x;
    int *clipped_y;
    int *clipped_color;
    int base_x;
    int base_y;
    int base_z;
    bool check_hover;
    int mouse_x;
    int mouse_y;
    int picked_count;
    int *picked_bitsets;
} ModelData;

void model_free_global(void);
void model_init_global(void);
void model_free_calculate_normals(Model *m);
void model_free_label_references(Model *m);
void model_free_copy_faces(Model *m, bool copyVertexY, bool copyFaces);
void model_free_share_colored(Model *m, bool shareColors, bool shareAlpha, bool shareVertices);
void model_free_share_alpha(Model *m, bool shareAlpha);
void model_free(Model *model);
void model_unpack(Jagfile *models);
Model *model_from_id(int id, bool use_allocator);
Model *model_from_models(Model **models, int count, bool use_allocator);
Model *model_from_models_bounds(Model **models, int count);
Model *model_share_colored(Model *src, bool shareColors, bool shareAlpha, bool shareVertices, bool use_allocator);
Model *model_copy_faces(Model *src, bool copyVertexY, bool copyFaces, bool use_allocator);
Model *model_share_alpha(Model *src, bool shareAlpha);
void model_calculate_normals(Model *m, int light_ambient, int light_attenuation, int lightsrc_x, int lightsrc_y, int lightsrc_z, bool apply_lighting, bool use_allocator);
int model_mul_color_lightness(int hsl, int scalar, int face_info);
void model_apply_lighting(Model *m, int light_ambient, int light_attenuation, int lightsrc_x, int lightsrc_y, int lightsrc_z);
void model_calculate_bounds_cylinder(Model *m);
void model_calculate_bounds_aabb(Model *m);
void model_calculate_bounds_y(Model *m);
bool model_point_within_triangle(int x, int y, int ya, int yb, int yc, int xa, int xb, int xc);
void model_draw_simple(Model *m, int pitch, int yaw, int roll, int eyePitch, int eyeX, int eyeY, int eyeZ);
void model_draw(Model *m, int yaw, int sinCameraPitch, int cosCameraPitch, int sinCameraYaw, int cosCameraYaw, int sceneX, int sceneY, int sceneZ, int key);
void model_draw2(Model *m, bool projected, bool hasInput, int bitset);
void model_draw_face(Model *m, int index);
void model_draw_near_clipped_face(Model *m, int index);
void model_create_label_references(Model *m, bool use_allocator);
void model_apply_transform(Model *m, int id);
void model_apply_transforms(Model *m, int id, int id2, int *walkmerge);
void model_apply_transform2(Model *m, int x, int y, int z, int *labels, int labels_count, int type);
void model_rotate_y90(Model *m);
void model_rotate_x(Model *m, int angle);
void model_translate(Model *m, int y, int x, int z);
void model_recolor(Model *m, int src, int dst);
void model_rotate_y180(Model *m);
void model_scale(Model *m, int x, int y, int z);
