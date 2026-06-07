#include "viewport.h"

void kr_camera_init(KrCamera& camera) {
    camera.distance = 5.0f;
    camera.azimuth = 45.0f;
    camera.elevation = 30.0f;
}

void kr_camera_orbit(KrCamera& camera, float dAz, float dEl) {
    camera.azimuth += dAz;
    camera.elevation += dEl;
    if (camera.elevation > 89.0f) camera.elevation = 89.0f;
    if (camera.elevation < -89.0f) camera.elevation = -89.0f;
}

void kr_camera_zoom(KrCamera& camera, float delta) {
    camera.distance -= delta;
    if (camera.distance < 0.5f) camera.distance = 0.5f;
    if (camera.distance > 100.0f) camera.distance = 100.0f;
}

void kr_camera_reset(KrCamera& camera) {
    kr_camera_init(camera);
}
