#include <string.h>
#include <math.h>
#include "object_tracking.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"

static struct {
    tracker_config_t config;
    tracked_obj_msg_t msg;
    uint32_t frame_count;
    bool initialized;
} trk;

static inline float sq(float x) { return x * x; }

static void kalman_predict(kalman_state_t *k,
                            float dt, float proc_noise)
{
    float F[KALMAN_STATE_DIM][KALMAN_STATE_DIM];
    memset(F, 0, sizeof(F));
    for (int i = 0; i < 4; i++) {
        F[i][i] = 1.0f;
        F[i][i + 4] = dt;
        F[i + 4][i + 4] = 1.0f;
    }

    float new_state[KALMAN_STATE_DIM];
    memset(new_state, 0, sizeof(new_state));
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        for (int j = 0; j < KALMAN_STATE_DIM; j++) {
            new_state[i] += F[i][j] * k->state[j];
        }
    }
    memcpy(k->state, new_state, sizeof(k->state));

    float new_cov[KALMAN_STATE_DIM][KALMAN_STATE_DIM];
    memset(new_cov, 0, sizeof(new_cov));
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        for (int j = 0; j < KALMAN_STATE_DIM; j++) {
            float sum = 0;
            for (int l = 0; l < KALMAN_STATE_DIM; l++)
                sum += F[i][l] * k->covariance[l][j];
            new_cov[i][j] = sum;
        }
    }
    float Q[KALMAN_STATE_DIM][KALMAN_STATE_DIM];
    memset(Q, 0, sizeof(Q));
    for (int i = 0; i < 4; i++) Q[i][i] = proc_noise * dt;
    for (int i = 4; i < KALMAN_STATE_DIM; i++) Q[i][i] = proc_noise * dt * 10;

    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        for (int j = 0; j < KALMAN_STATE_DIM; j++) {
            float sum = 0;
            for (int l = 0; l < KALMAN_STATE_DIM; l++)
                sum += new_cov[i][l] * F[j][l];
            k->covariance[i][j] = sum + Q[i][j];
        }
    }
}

static void kalman_update(kalman_state_t *k,
                           float cx, float cy, float s, float r)
{
    float z[4] = {cx, cy, s, r};
    float H[4][KALMAN_STATE_DIM];
    memset(H, 0, sizeof(H));
    for (int i = 0; i < 4; i++) H[i][i] = 1.0f;

    float y[4];
    for (int i = 0; i < 4; i++) {
        y[i] = z[i] - k->state[i];
    }

    float S[4][4];
    memset(S, 0, sizeof(S));
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int l = 0; l < KALMAN_STATE_DIM; l++)
                sum += H[i][l] * k->covariance[l][j];
            S[i][j] = sum + (i == j ? k->measurement_noise : 0);
        }
    }

    float K[KALMAN_STATE_DIM][4];
    memset(K, 0, sizeof(K));
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int l = 0; l < KALMAN_STATE_DIM; l++)
                sum += k->covariance[i][l] * H[j][l];
            float det = S[0][0] * S[1][1] - S[0][1] * S[1][0];
            if (det < 1e-6f) det = 1e-6f;
            float invS[4][4] = {{0}};
            invS[0][0] = S[1][1] / det;
            invS[0][1] = -S[0][1] / det;
            invS[1][0] = -S[1][0] / det;
            invS[1][1] = S[0][0] / det;
            invS[2][2] = 1.0f / S[2][2];
            invS[3][3] = 1.0f / S[3][3];
            K[i][j] = 0;
            for (int l = 0; l < 4; l++)
                K[i][j] += k->covariance[i][l] * invS[l][j];
        }
    }

    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        float innov = 0;
        for (int j = 0; j < 4; j++) innov += K[i][j] * y[j];
        k->state[i] += innov;
    }

    float I_KH[KALMAN_STATE_DIM][KALMAN_STATE_DIM];
    memset(I_KH, 0, sizeof(I_KH));
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        for (int j = 0; j < KALMAN_STATE_DIM; j++) {
            float sum = 0;
            for (int l = 0; l < 4; l++) sum += K[i][l] * H[l][j];
            I_KH[i][j] = (i == j ? 1.0f : 0) - sum;
        }
    }
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        for (int j = 0; j < KALMAN_STATE_DIM; j++) {
            float sum = 0;
            for (int l = 0; l < KALMAN_STATE_DIM; l++)
                sum += I_KH[i][l] * k->covariance[l][j];
            k->covariance[i][j] = sum;
        }
    }
}

static void state_to_bbox(const float *state, bounding_box_t *bbox)
{
    float cx = state[0], cy = state[1], s = state[2], r = state[3];
    float w = sqrtf(s / r);
    float h = s / w;
    bbox->x = (uint16_t)(cx - w / 2);
    bbox->y = (uint16_t)(cy - h / 2);
    bbox->width = (uint16_t)w;
    bbox->height = (uint16_t)h;
}

static float compute_iou(const bounding_box_t *a, const bounding_box_t *b)
{
    uint16_t ax1 = a->x, ay1 = a->y;
    uint16_t ax2 = a->x + a->width, ay2 = a->y + a->height;
    uint16_t bx1 = b->x, by1 = b->y;
    uint16_t bx2 = b->x + b->width, by2 = b->y + b->height;

    uint16_t ix1 = ax1 > bx1 ? ax1 : bx1;
    uint16_t iy1 = ay1 > by1 ? ay1 : by1;
    uint16_t ix2 = ax2 < bx2 ? ax2 : bx2;
    uint16_t iy2 = ay2 < by2 ? ay2 : by2;

    if (ix2 <= ix1 || iy2 <= iy1) return 0;

    uint32_t inter = (ix2 - ix1) * (iy2 - iy1);
    uint32_t area_a = a->width * a->height;
    uint32_t area_b = b->width * b->height;
    return (float)inter / (area_a + area_b - inter);
}

static void predict_tracks(void)
{
    for (uint32_t i = 0; i < trk.msg.active_count; i++) {
        tracked_object_t *t = &trk.msg.tracks[i];
        if (!t->active) continue;

        float s = t->last_box.width * t->last_box.height;
        float r = t->last_box.width / (float)t->last_box.height;
        t->kalman.state[0] = t->last_box.x + t->last_box.width / 2.0f;
        t->kalman.state[1] = t->last_box.y + t->last_box.height / 2.0f;
        t->kalman.state[2] = s;
        t->kalman.state[3] = r;

        kalman_predict(&t->kalman, 1.0f, trk.config.process_noise);
        state_to_bbox(t->kalman.state, &t->predicted_box);

        t->velocity_mps[0] = t->kalman.state[4];
        t->velocity_mps[1] = t->kalman.state[5];
        t->velocity_mps[2] = 0;
        float speed = sqrtf(sq(t->velocity_mps[0]) + sq(t->velocity_mps[1]));
        t->is_moving = speed > trk.config.velocity_threshold_mps;
    }
}

static int find_best_match(const bounding_box_t *det,
                            uint32_t *assigned,
                            float *best_iou_out)
{
    int best_track = -1;
    float best_iou = trk.config.iou_threshold;
    for (uint32_t j = 0; j < trk.msg.active_count; j++) {
        if (assigned[j]) continue;
        if (!trk.msg.tracks[j].active) continue;
        float iou = compute_iou(det, &trk.msg.tracks[j].predicted_box);
        if (iou > best_iou) {
            best_iou = iou;
            best_track = (int)j;
        }
    }
    *best_iou_out = best_iou;
    return best_track;
}

static void assign_detections(const detection_list_t *dets)
{
    uint32_t assigned_det[MAX_TRACKS] = {0};

    for (uint32_t i = 0; i < trk.msg.active_count; i++)
        trk.msg.tracks[i].stale_count++;

    for (uint32_t i = 0; i < dets->count; i++) {
        float best_iou;
        int idx = find_best_match(&dets->boxes[i], assigned_det, &best_iou);
        if (idx >= 0) {
            assigned_det[idx] = 1;
            tracked_object_t *t = &trk.msg.tracks[idx];
            t->last_box = dets->boxes[i];
            t->age++;
            t->hit_count++;
            t->stale_count = 0;

            float cx = dets->boxes[i].x + dets->boxes[i].width / 2.0f;
            float cy = dets->boxes[i].y + dets->boxes[i].height / 2.0f;
            float s = dets->boxes[i].width * dets->boxes[i].height;
            float r = dets->boxes[i].width / (float)dets->boxes[i].height;
            kalman_update(&t->kalman, cx, cy, s, r);
            state_to_bbox(t->kalman.state, &t->last_box);
        } else if (trk.msg.active_count < trk.config.max_tracks) {
            uint32_t n = trk.msg.active_count++;
            tracked_object_t *t = &trk.msg.tracks[n];
            memset(t, 0, sizeof(*t));
            t->track_id = trk.msg.next_id++;
            t->last_box = dets->boxes[i];

            float cx = dets->boxes[i].x + dets->boxes[i].width / 2.0f;
            float cy = dets->boxes[i].y + dets->boxes[i].height / 2.0f;
            float s = dets->boxes[i].width * dets->boxes[i].height;
            float r = dets->boxes[i].width / (float)dets->boxes[i].height;
            t->kalman.state[0] = cx;
            t->kalman.state[1] = cy;
            t->kalman.state[2] = s;
            t->kalman.state[3] = r;
            for (int k = 0; k < KALMAN_STATE_DIM; k++)
                t->kalman.covariance[k][k] = 10.0f;
            t->kalman.measurement_noise = trk.config.measurement_noise;
            t->kalman.process_noise = trk.config.process_noise;

            t->active = true;
            t->age = 1;
            t->hit_count = 1;
        }
    }

    uint32_t write = 0;
    for (uint32_t read = 0; read < trk.msg.active_count; read++) {
        if (trk.msg.tracks[read].stale_count > trk.config.max_stale_frames) {
            continue;
        }
        if (write != read)
            trk.msg.tracks[write] = trk.msg.tracks[read];
        write++;
    }
    trk.msg.active_count = write;
}

tracker_status_t tracker_init(const tracker_config_t *config)
{
    if (config == NULL) return TRACKER_ERR_INIT;
    memset(&trk, 0, sizeof(trk));
    trk.config = *config;
    if (trk.config.iou_threshold <= 0) trk.config.iou_threshold = 0.3f;
    if (trk.config.max_stale_frames == 0) trk.config.max_stale_frames = TRACK_STALE_FRAMES;
    if (trk.config.max_tracks == 0 || trk.config.max_tracks > MAX_TRACKS)
        trk.config.max_tracks = MAX_TRACKS;
    if (trk.config.velocity_threshold_mps <= 0) trk.config.velocity_threshold_mps = 0.5f;
    if (trk.config.process_noise <= 0) trk.config.process_noise = 0.01f;
    if (trk.config.measurement_noise <= 0) trk.config.measurement_noise = 1.0f;

    trk.msg.next_id = 1;
    trk.initialized = true;
    log_info("tracker", "Init iou=%.2f stale=%lu max=%lu vel=%.1f pn=%.4f mn=%.1f",
             trk.config.iou_threshold, (unsigned long)trk.config.max_stale_frames,
             (unsigned long)trk.config.max_tracks,
             trk.config.velocity_threshold_mps,
             trk.config.process_noise, trk.config.measurement_noise);
    return TRACKER_OK;
}

tracker_status_t tracker_process(const detection_list_t *detections,
                                  tracked_obj_msg_t **out_tracks)
{
    if (!trk.initialized || detections == NULL) return TRACKER_ERR_INIT;

    trk.frame_count++;
    trk.msg.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;

    predict_tracks();
    assign_detections(detections);

    if (out_tracks) *out_tracks = &trk.msg;
    return TRACKER_OK;
}

tracker_status_t tracker_get_track_by_id(uint32_t track_id,
                                          tracked_object_t *out)
{
    if (!trk.initialized || out == NULL) return TRACKER_ERR_INIT;
    for (uint32_t i = 0; i < trk.msg.active_count; i++) {
        if (trk.msg.tracks[i].track_id == track_id && trk.msg.tracks[i].active) {
            *out = trk.msg.tracks[i];
            return TRACKER_OK;
        }
    }
    return TRACKER_ERR_ASSOCIATION;
}

tracker_status_t tracker_get_active_count(uint32_t *out_count)
{
    if (!trk.initialized || out_count == NULL) return TRACKER_ERR_INIT;
    *out_count = trk.msg.active_count;
    return TRACKER_OK;
}

tracker_status_t tracker_reset(void)
{
    memset(&trk.msg, 0, sizeof(trk.msg));
    trk.msg.next_id = 1;
    trk.frame_count = 0;
    return TRACKER_OK;
}

tracker_status_t tracker_deinit(void)
{
    trk.initialized = false;
    return TRACKER_OK;
}
