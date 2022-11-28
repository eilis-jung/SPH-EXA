struct Vertex {
    vec4 position;
	vec4 velocity;
	mat4 scale;
	vec4 attr1;  // radius, mass
	vec4 attr2;
	vec4 color;
};

struct Element {
    vec4 position;
	vec4 velocity;
	mat4 scale;
	vec4 attr1;  // radius, mass, is_running
};