#ifndef IRAN_ROAD_VEHICLES_H
#define IRAN_ROAD_VEHICLES_H

/* Approximate gameplay values, not real-world performance claims. */
typedef struct {
    const char *name;
    float baseTopSpeed;
    float acceleration;
    float length;
    float width;
    unsigned char r,g,b;
} VehicleSpec;

#define VEHICLE_COUNT 10
#define ENGINE_UPGRADE_STAGES 5

static const VehicleSpec VEHICLES[VEHICLE_COUNT] = {
    {"Samand LX",       185.0f, 0.72f, 4.45f, 1.78f, 185,185,180},
    {"Peugeot 206",     175.0f, 0.82f, 3.83f, 1.65f, 210,210,205},
    {"Peugeot 207",     190.0f, 0.86f, 3.87f, 1.68f, 225,225,220},
    {"Samand Soren",    195.0f, 0.80f, 4.53f, 1.76f, 165,170,175},
    {"Dena Plus",       205.0f, 0.88f, 4.55f, 1.72f, 25,30,38},
    {"Peugeot Pars",    185.0f, 0.76f, 4.49f, 1.70f, 200,200,195},
    {"Peugeot 405",     180.0f, 0.74f, 4.40f, 1.71f, 150,155,160},
    {"Pride",           145.0f, 0.58f, 3.93f, 1.60f, 220,220,215},
    {"Shahin",          200.0f, 0.84f, 4.45f, 1.76f, 40,55,75},
    {"Nissan Junior",   125.0f, 0.48f, 4.70f, 1.70f, 35,95,175}
};

static const unsigned char PAINTS[8][3] = {
    {210,210,205},{185,25,25},{20,25,30},{25,65,125},
    {210,175,35},{20,115,75},{130,130,135},{235,235,225}
};

static float vehicle_top_speed(int vehicle, int engine_stage) {
    float result = VEHICLES[vehicle].baseTopSpeed + (float)engine_stage * 28.0f;
    return result > 350.0f ? 350.0f : result;
}

static float vehicle_acceleration(int vehicle, int engine_stage) {
    return VEHICLES[vehicle].acceleration + (float)engine_stage * 0.12f;
}

#endif
