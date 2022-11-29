struct Vertex {
    vec4 position;
	vec4 color;
	vec2 texCoord;
};

struct Element {
	vec4 root;
    vec4 position;
	vec4 scale;
	vec4 velocity;
	mat4 modelMat;
};