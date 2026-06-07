#ifndef KRONOS_VIEWPORT_H
#define KRONOS_VIEWPORT_H

struct KrCamera {
    float distance;
    float azimuth;    // in degrees
    float elevation;  // in degrees
};

void kr_camera_init(KrCamera& camera);
void kr_camera_orbit(KrCamera& camera, float dAz, float dEl);
void kr_camera_zoom(KrCamera& camera, float delta);
void kr_camera_reset(KrCamera& camera);

#endif // KRONOS_VIEWPORT_H
