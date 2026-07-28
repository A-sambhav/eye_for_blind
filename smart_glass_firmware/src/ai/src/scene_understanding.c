#include <string.h>
#include <stdio.h>
#include <math.h>
#include "scene_understanding.h"
#include "object_tracking.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"

#define MIN_TRAVERSABLE_DEPTH 0.3f
#define FREESPACE_CELL_M (0.5f)

static inline float sq(float x) { return x * x; }

static struct {
    scene_config_t config;
    scene_desc_t scene;
    hazard_list_t hazards;
    uint32_t frame_count;
    bool initialized;
} su;

static void estimate_ground_plane(const depth_map_t *depth,
                                   float plane[4])
{
    int samples = 0;
    float sx = 0, sy = 0, sz = 0, sxx = 0, syy = 0, sxy = 0;
    float sxz = 0, syz = 0;

    for (uint16_t y = DEPTH_SMALL_H * 2 / 3; y < DEPTH_SMALL_H; y++) {
        for (uint16_t x = 0; x < DEPTH_SMALL_W; x += 4) {
            float d = depth->map[y][x];
            if (d < 0.3f || d > 10.0f) continue;
            float hfov = 70.0f * 3.14159f / 180.0f;
            float fx = DEPTH_SMALL_W / (2.0f * tanf(hfov / 2.0f));
            float fy = fx;
            float cx = (float)x - DEPTH_SMALL_W / 2.0f;
            float cy = (float)y - DEPTH_SMALL_H / 2.0f;
            float pz = d;
            float px = cx * d / fx;
            float py = -cy * d / fy;

            sx += px; sy += py; sz += pz;
            sxx += px * px; syy += py * py; sxy += px * py;
            sxz += px * pz; syz += py * pz;
            samples++;
        }
    }

    if (samples < 10) {
        plane[0] = 0; plane[1] = 1; plane[2] = 0; plane[3] = 0;
        return;
    }

    float inv_n = 1.0f / samples;
    float mx = sx * inv_n, my = sy * inv_n, mz = sz * inv_n;
    float cxx = sxx - mx * sx, cyy = syy - my * sy;
    float cxy = sxy - mx * sy, cxz = sxz - mx * sz, cyz = syz - my * sz;

    float det = cxx * cyy - cxy * cxy;
    if (fabsf(det) < 1e-10f) {
        plane[0] = 0; plane[1] = 1; plane[2] = 0; plane[3] = 0;
        return;
    }

    plane[0] = (cyy * cxz - cxy * cyz) / det;
    plane[1] = (cxx * cyz - cxy * cxz) / det;
    plane[2] = -1.0f;
    plane[3] = plane[0] * mx + plane[1] * my + plane[2] * mz;
    float norm = sqrtf(sq(plane[0]) + sq(plane[1]) + sq(plane[2]));
    if (norm > 1e-6f) { plane[0] /= norm; plane[1] /= norm; plane[2] /= norm; plane[3] /= norm; }
    if (plane[1] < 0) { plane[0] = -plane[0]; plane[1] = -plane[1]; plane[2] = -plane[2]; plane[3] = -plane[3]; }
}

static void compute_free_space(const depth_map_t *depth,
                                const float plane[4],
                                uint8_t grid[FREE_SPACE_GRID_H][FREE_SPACE_GRID_W])
{
    for (uint16_t y = 0; y < FREE_SPACE_GRID_H; y++) {
        for (uint16_t x = 0; x < FREE_SPACE_GRID_W; x++) {
            uint16_t dx = x * DEPTH_SMALL_W / FREE_SPACE_GRID_W;
            uint16_t dy = y * DEPTH_SMALL_H / FREE_SPACE_GRID_H;

            float d = depth->map[dy][dx];
            if (d >= su.config.hazard_distance_min && d <= 20.0f) {
                float hfov = 70.0f * 3.14159f / 180.0f;
                float fx = FREE_SPACE_GRID_W / (2.0f * tanf(hfov / 2.0f));
                float fy = fx;
                float px = (x - FREE_SPACE_GRID_W / 2) * d / fx;
                float py = -(y - FREE_SPACE_GRID_H / 2) * d / fy;
                float dist_to_plane = fabsf(plane[0] * px + plane[1] * py + plane[2] * d + plane[3]);

                grid[y][x] = (dist_to_plane < 0.3f && d <= 5.0f) ? 255 : 128;
            } else {
                grid[y][x] = (d >= su.config.hazard_distance_min) ? 255 : 0;
            }
        }
    }
}

static scene_type_t classify_scene(const depth_map_t *depth,
                                    const detection_list_t *dets)
{
    int person_count = 0, vehicle_count = 0, indoor_count = 0;
    for (uint32_t i = 0; i < dets->count; i++) {
        if (dets->boxes[i].class_id == 0) person_count++;
        if (dets->boxes[i].class_id >= 2 && dets->boxes[i].class_id <= 7) vehicle_count++;
        if (dets->boxes[i].class_id >= 56 && dets->boxes[i].class_id <= 70)
            indoor_count++;
    }

    float near_total = 0;
    for (uint16_t y = DEPTH_SMALL_H / 2; y < DEPTH_SMALL_H; y++) {
        for (uint16_t x = 0; x < DEPTH_SMALL_W; x += 8) {
            float d = depth->map[y][x];
            if (d > 0.3f && d < 3.0f) near_total++;
        }
    }
    uint32_t near_samples = (DEPTH_SMALL_H / 2) * (DEPTH_SMALL_W / 8);
    float near_ratio = near_total / near_samples;

    if (vehicle_count >= 2) return kSceneIntersection;
    if (person_count >= 3 && near_ratio > 0.4f) return kSceneOutdoor;
    if (indoor_count >= 2) return kSceneIndoor;
    if (near_ratio < 0.2f && person_count < 2) return kScenePark;
    if (person_count >= 1 && vehicle_count >= 1) return kSceneCrosswalk;
    if (near_ratio > 0.6f && indoor_count > 0) return kSceneCorridor;

    float far_total = 0;
    for (uint16_t y = 0; y < DEPTH_SMALL_H / 3; y++) {
        for (uint16_t x = 0; x < DEPTH_SMALL_W; x += 8) {
            if (depth->map[y][x] > 5.0f) far_total++;
        }
    }
    if (far_total > DEPTH_SMALL_W * DEPTH_SMALL_H / 3 / 8 * 0.7f)
        return kSceneOutdoor;

    return kSceneUnknown;
}

static void detect_hazards(const detection_list_t *dets,
                            const depth_map_t *depth,
                            const tracked_obj_msg_t *tracks,
                            hazard_list_t *hazards,
                            scene_desc_t *scene)
{
    hazards->count = 0;
    hazards->timestamp_us = su.scene.timestamp_us;

    for (uint32_t i = 0; i < dets->count && hazards->count < 12; i++) {
        const bounding_box_t *b = &dets->boxes[i];
        hazard_type_t ht = kHazardNone;
        uint8_t sev = 1;
        float dist = 99;

        float cx = b->x + b->width / 2.0f;
        float cy = b->y + b->height / 2.0f;
        uint16_t sx = (uint16_t)(cx * DEPTH_SMALL_W / DEPTH_FULL_W);
        uint16_t sy = (uint16_t)(cy * DEPTH_SMALL_H / DEPTH_FULL_H);
        if (sx < DEPTH_SMALL_W && sy < DEPTH_SMALL_H)
            dist = depth->map[sy][sx];

        if (b->class_id == 0) {
            ht = kHazardPersonClose;
            sev = dist < 1.5f ? 9 : (dist < 3.0f ? 6 : 3);
            bool moving = false;
            if (tracks) {
                for (uint32_t t = 0; t < tracks->active_count; t++) {
                    if (tracks->tracks[t].is_moving &&
                        fabsf((float)(tracks->tracks[t].last_box.x - b->x)) < 50)
                        { moving = true; break; }
                }
            }
            if (moving) { ht = kHazardMovingObject; sev = 8; }
        } else if (b->class_id >= 2 && b->class_id <= 7) {
            ht = kHazardVehicle;
            sev = dist < 5.0f ? 10 : 7;
        } else if (b->class_id == 56 || b->class_id == 57) {
            ht = kHazardObstacle;
            sev = dist < 1.0f ? 7 : 4;
        } else if (b->class_id == 58) {
            ht = kHazardObstacle;
            sev = dist < 1.0f ? 6 : 3;
        } else if (b->class_id == 60) {
            ht = kHazardObstacle;
            sev = dist < 1.0f ? 5 : 2;
        } else if (b->class_id == 62 || b->class_id == 63) {
            ht = kHazardPole;
            sev = dist < 1.0f ? 6 : 3;
        } else if (b->class_id == 67 || b->class_id == 68 || b->class_id == 69) {
            ht = kHazardDoorway;
            sev = 4;
        } else if (b->class_id == 64 || b->class_id == 65) {
            ht = kHazardOverhead;
            sev = 5;
        }

        if (ht != kHazardNone) {
            hazards->hazards[hazards->count] = ht;
            hazards->severity[hazards->count] = sev;
            hazards->distance[hazards->count] = dist;
            hazards->count++;
            scene->objects[scene->count].is_hazard = true;
            scene->objects[scene->count].hazard_type = ht;
            scene->objects[scene->count].hazard_severity = sev;
        }
    }

    for (uint32_t i = 0; i < hazards->count - 1; i++) {
        for (uint32_t j = i + 1; j < hazards->count; j++) {
            if (hazards->severity[j] > hazards->severity[i]) {
                hazard_type_t th = hazards->hazards[i];
                hazards->hazards[i] = hazards->hazards[j];
                hazards->hazards[j] = th;
                uint8_t ts = hazards->severity[i];
                hazards->severity[i] = hazards->severity[j];
                hazards->severity[j] = ts;
                float td = hazards->distance[i];
                hazards->distance[i] = hazards->distance[j];
                hazards->distance[j] = td;
            }
        }
    }
}

static void generate_description(scene_desc_t *scene,
                                  const hazard_list_t *hazards)
{
    const char *scene_names[] = {
        "indoor", "outdoor", "corridor", "intersection",
        "staircase", "crosswalk", "park", "unknown"
    };
    const char *hazard_names[] = {
        "obstacle", "drop-off", "stairs down", "stairs up",
        "vehicle", "person nearby", "overhead", "uneven ground",
        "doorway", "pole", "moving object", "none"
    };

    int n = snprintf(scene->description, SCENE_DESC_MAX_LEN,
                     "%s scene with %lu objects",
                     scene_names[scene->scene_type],
                     (unsigned long)scene->count);

    if (hazards->count > 0) {
        int remaining = SCENE_DESC_MAX_LEN - n;
        if (remaining > 0) {
            n += snprintf(scene->description + n, (size_t)remaining,
                          ". Hazards: ");
        }
        for (uint32_t i = 0; i < hazards->count && i < 3 && n < SCENE_DESC_MAX_LEN - 20; i++) {
            remaining = SCENE_DESC_MAX_LEN - n;
            n += snprintf(scene->description + n, (size_t)remaining,
                          "%s(sev=%u,d=%.1fm) ",
                          hazard_names[hazards->hazards[i]],
                          hazards->severity[i],
                          hazards->distance[i]);
        }
    }

    if (scene->traversable_width_cm > 0 && n < SCENE_DESC_MAX_LEN - 30) {
        snprintf(scene->description + n, (size_t)(SCENE_DESC_MAX_LEN - n),
                 ". Path width: %ucm", scene->traversable_width_cm);
    }
}

static void compute_traversable_width(
    const uint8_t grid[FREE_SPACE_GRID_H][FREE_SPACE_GRID_W],
    uint16_t *width_cm)
{
    uint16_t max_run = 0;
    uint16_t cy = FREE_SPACE_GRID_H * 3 / 4;
    if (cy >= FREE_SPACE_GRID_H) cy = FREE_SPACE_GRID_H - 1;

    uint16_t run = 0;
    for (uint16_t x = 0; x < FREE_SPACE_GRID_W; x++) {
        if (grid[cy][x] > su.config.free_space_threshold) {
            run++;
            if (run > max_run) max_run = run;
        } else {
            run = 0;
        }
    }

    *width_cm = (uint16_t)(max_run * FREESPACE_CELL_M * 100);
}

scene_status_t scene_init(const scene_config_t *config)
{
    if (config == NULL) return SCENE_ERR_INIT;
    memset(&su, 0, sizeof(su));
    su.config = *config;
    if (su.config.hazard_distance_min <= 0) su.config.hazard_distance_min = 0.5f;
    if (su.config.free_space_threshold == 0) su.config.free_space_threshold = 128;
    if (su.config.max_scene_objects == 0 || su.config.max_scene_objects > SCENE_MAX_OBJECTS)
        su.config.max_scene_objects = SCENE_MAX_OBJECTS;

    su.initialized = true;
    log_info("scene", "Init hazard_min=%.1f freespace_thr=%u max_obj=%u desc=%d",
             su.config.hazard_distance_min, su.config.free_space_threshold,
             su.config.max_scene_objects, su.config.enable_description);
    return SCENE_OK;
}

scene_status_t scene_process(const detection_list_t *detections,
                              const depth_map_t *depth,
                              scene_desc_t **out_scene,
                              hazard_list_t **out_hazards)
{
    if (!su.initialized || detections == NULL || depth == NULL)
        return SCENE_ERR_INIT;

    su.frame_count++;

    memset(&su.scene, 0, sizeof(su.scene));
    memset(&su.hazards, 0, sizeof(su.hazards));

    su.scene.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;

    estimate_ground_plane(depth, su.scene.ground_plane);
    compute_free_space(depth, su.scene.ground_plane, su.scene.free_space_grid);

    su.scene.scene_type = classify_scene(depth, detections);
    compute_traversable_width(su.scene.free_space_grid,
                               &su.scene.traversable_width_cm);

    uint32_t obj_count = detections->count < su.config.max_scene_objects ?
                          detections->count : su.config.max_scene_objects;
    for (uint32_t i = 0; i < obj_count; i++) {
        scene_object_t *so = &su.scene.objects[su.scene.count];
        so->box = detections->boxes[i];
        so->distance_to_user = 0;
        uint16_t cx = detections->boxes[i].x + detections->boxes[i].width / 2;
        uint16_t cy = detections->boxes[i].y + detections->boxes[i].height / 2;
        uint16_t sx = (uint16_t)(cx * DEPTH_SMALL_W / DEPTH_FULL_W);
        uint16_t sy = (uint16_t)(cy * DEPTH_SMALL_H / DEPTH_FULL_H);
        if (sx < DEPTH_SMALL_W && sy < DEPTH_SMALL_H)
            so->distance_to_user = depth->map[sy][sx];
        su.scene.count++;
    }

    detect_hazards(detections, depth, NULL, &su.hazards, &su.scene);

    if (su.config.enable_description)
        generate_description(&su.scene, &su.hazards);

    message_bus_publish(MSG_HAZARD_LIST, &su.hazards, sizeof(su.hazards), 4);

    if (out_scene) *out_scene = &su.scene;
    if (out_hazards) *out_hazards = &su.hazards;

    return su.scene.count > 0 ? SCENE_OK : SCENE_ERR_NO_OBJECTS;
}

scene_status_t scene_get_ground_plane(float *out_a, float *out_b,
                                       float *out_c, float *out_d)
{
    if (!su.initialized) return SCENE_ERR_INIT;
    if (out_a) *out_a = su.scene.ground_plane[0];
    if (out_b) *out_b = su.scene.ground_plane[1];
    if (out_c) *out_c = su.scene.ground_plane[2];
    if (out_d) *out_d = su.scene.ground_plane[3];
    return SCENE_OK;
}

scene_status_t scene_get_free_space(uint8_t *out_grid)
{
    if (!su.initialized || out_grid == NULL) return SCENE_ERR_INIT;
    memcpy(out_grid, su.scene.free_space_grid, sizeof(su.scene.free_space_grid));
    return SCENE_OK;
}

scene_type_t scene_classify_environment(void)
{
    return su.initialized ? su.scene.scene_type : kSceneUnknown;
}

scene_status_t scene_deinit(void)
{
    su.initialized = false;
    return SCENE_OK;
}
