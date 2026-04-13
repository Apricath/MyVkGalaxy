#version 450

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;

layout(location = 0) out vec4 o_color;

layout(binding = 0) uniform sampler2D u_fontAtlas;

void main()
{
	o_color = i_color * texture(u_fontAtlas, i_uv);
}
