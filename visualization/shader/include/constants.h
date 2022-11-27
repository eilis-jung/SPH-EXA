// #define m_max_point_light_count 15
// #define m_max_point_light_geom_vertices 90 // 90 = 2 * 3 * m_max_point_light_count
// #define m_mesh_per_drawcall_max_instance_count 64
// #define m_mesh_vertex_blending_max_joint_count 1024
// #define CHAOS_LAYOUT_MAJOR row_major
#define PI 3.1415926f
// layout(CHAOS_LAYOUT_MAJOR) buffer;
// layout(CHAOS_LAYOUT_MAJOR) uniform;
// boundry of the space
const float width      = 10.f; // Y
const float length     = 10.f; // X
const float height     = 10.f; // Z
const vec4  startPoint = vec4(0.f, 0.f, 0.f, 0.f);

const float radius_snow = 0.05f;
const float radius_ice  = 0.025f;
const float gridSize    = 2.f * radius_snow;

const float neighborRadius = 1.5f * gridSize;

const float youngs_modulus_snow    = 25000.f;
const float youngs_modulus_ice     = 35000.f;
const float cohesive_strength_snow = 625.f;  // 625
const float cohesive_strength_ice  = 3750.f; // 3750

const float Kq    = 0.00005f;
const float FminW = 0.12275f;
const float FmaxW = 10000.f;

const float damping         = 1.f;
const float boundry_damping = 0.5f;

const vec3 gravity = vec3(0.f, 0.f, 0.f);

const float Kf              = 50.f;
const float angle_of_repose = 38.f / 180.f * PI;
const float dt              = 0.0017f;