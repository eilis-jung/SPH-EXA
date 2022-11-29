struct Vertex {
    vec4 position;
	// mat4 modelMat;
	vec4 color;
};

struct Element {
    vec3 translation;
	vec3 scale;
	vec3 rotation;
	vec3 velocity;
	// vec4 mati0;
	// vec4 mati1;
	// vec4 mati2;
	// vec4 mati3;
	mat4 modelMat;
};