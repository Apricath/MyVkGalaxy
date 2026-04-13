#version 450

layout(location = 0) in vec2 i_pos;
layout(location = 1) in vec2 i_uv;
layout(location = 2) in vec4 i_color;

layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec4 o_color;

layout(push_constant) uniform PushConstants
{
	vec2 u_scale;
	vec2 u_translate;
};

void main()
{
	o_uv = i_uv;
	o_color = i_color;
	gl_Position = vec4(i_pos * u_scale + u_translate, 0.0, 1.0);
}
