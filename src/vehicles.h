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

/* Compact per-car geometry blueprints. Dimensions are normalized against the
   selected vehicle's real gameplay length/width and expanded into PS2-friendly
   polygon surfaces at runtime (no large external model files). */
typedef struct {
    float roofHeight;
    float cabinRear;
    float cabinFront;
    float cabinWidth;
    float hoodHeight;
    float beltHeight;
    float rearDeckHeight;
    float frontNose;
    float rearNose;
    float wheelScale;
    unsigned char glassTint;
    unsigned char trimTone;
} VehicleShape;

static const VehicleShape VEHICLE_SHAPES[VEHICLE_COUNT] = {
    {1.25f,.29f,.70f,.58f,.47f,.73f,.68f,.07f,.05f,1.00f,78,28}, /* Samand LX */
    {1.17f,.22f,.70f,.60f,.44f,.71f,.72f,.10f,.08f, .96f,72,30}, /* 206 */
    {1.20f,.23f,.71f,.61f,.46f,.72f,.73f,.11f,.08f, .98f,76,29}, /* 207 */
    {1.29f,.30f,.69f,.57f,.48f,.74f,.70f,.06f,.05f,1.02f,82,27}, /* Soren */
    {1.34f,.31f,.70f,.59f,.49f,.75f,.72f,.05f,.05f,1.04f,85,25}, /* Dena */
    {1.24f,.29f,.69f,.58f,.47f,.73f,.71f,.06f,.05f,1.00f,78,28}, /* Pars */
    {1.22f,.28f,.69f,.57f,.46f,.72f,.70f,.07f,.06f,1.00f,74,30}, /* 405 */
    {1.08f,.22f,.70f,.61f,.42f,.68f,.68f,.12f,.10f, .92f,68,34}, /* Pride */
    {1.33f,.30f,.70f,.58f,.48f,.75f,.72f,.06f,.05f,1.03f,86,25}, /* Shahin */
    {1.21f,.43f,.78f,.58f,.45f,.78f,.66f,.12f,.07f,1.05f,70,32}  /* Nissan Junior */
};

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
