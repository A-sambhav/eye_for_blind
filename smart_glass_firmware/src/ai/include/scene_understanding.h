#ifndef SCENE_UNDERSTANDING_H
#define SCENE_UNDERSTANDING_H

#include <stdint.h>
#include <stdbool.h>
#include "object_detection.h"

#define SCENE_MAX_OBJECTS 64
#define SCENE_DESC_MAX_LEN 256
#define FREE_SPACE_GRID_W 160
#define FREE_SPACE_GRID_H 120

typedef enum {
    kSceneIndoor,
    kSceneOutdoor,
    kSceneCorridor,
    kSceneIntersection,
    kSceneStaircase,
    kSceneCrosswalk,
    kScenePark,
    kSceneUnknown
} scene_type_t;

typedef enum {
    kHazardObstacle,
    kHazardDropOff,
    kHazardStairDescent,
    kHazardStairAscent,
    kHazardVehicle,
    kHazardPersonClose,
    kHazardOverhead,
    kHazardUnevenGround,
    kHazardDoorway,
    kHazardPole,
    kHazardMovingObject,
    kHazardNone
} hazard_type_t;

typedef struct {
    bounding_box_t box;
    uint8_t scene_relation;
    float distance_to_user;
    bool is_moving;
    bool is_hazard;
    hazard_type_t hazard_type;
    uint8_t hazard_severity;
} scene_object_t;

typedef struct {
    scene_object_t objects[SCENE_MAX_OBJECTS];
    uint32_t count;
    scene_type_t scene_type;
    float ground_plane[4];
    uint8_t free_space_grid[FREE_SPACE_GRID_H][FREE_SPACE_GRID_W];
    uint16_t traversable_width_cm;
    char description[SCENE_DESC_MAX_LEN];
    uint32_t timestamp_us;
} scene_desc_t;

typedef struct {
    hazard_type_t hazards[12];
    uint8_t severity[12];
    float distance[12];
    uint32_t count;
    uint32_t timestamp_us;
} hazard_list_t;

typedef struct {
    float ground_confidence;
    float hazard_distance_min;
    uint8_t free_space_threshold;
    bool enable_description;
    uint8_t max_scene_objects;
} scene_config_t;

typedef enum {
    SCENE_OK = 0,
    SCENE_ERR_INIT,
    SCENE_ERR_NO_OBJECTS,
    SCENE_ERR_GROUND_PLANE
} scene_status_t;

scene_status_t scene_init(const scene_config_t *config);
scene_status_t scene_process(const detection_list_t *detections,
                              const depth_map_t *depth,
                              scene_desc_t **out_scene,
                              hazard_list_t **out_hazards);
scene_status_t scene_get_ground_plane(float *out_a, float *out_b,
                                       float *out_c, float *out_d);
scene_status_t scene_get_free_space(uint8_t *out_grid);
scene_type_t scene_classify_environment(void);
scene_status_t scene_deinit(void);

#endif /* SCENE_UNDERSTANDING_H */
