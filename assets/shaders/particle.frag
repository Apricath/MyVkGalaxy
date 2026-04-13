#version 450

layout(location = 0) in vec2 a_texPos;
layout(location = 1) in vec4 a_color;
layout(location = 2) in flat uint a_type;
layout(location = 3) in flat uint a_displayFlags;

layout(location = 0) out vec4 o_color;

#define DISPLAY_STARS     (1u << 0)
#define DISPLAY_DUST      (1u << 1)
#define DISPLAY_FILAMENTS (1u << 2)
#define DISPLAY_H2        (1u << 3)

void main()
{
	if(a_type == 255u)
		discard;

	vec2 centeredPos = 2.0 * (a_texPos - 0.5);
	float radialFalloff = max(1.0 - length(centeredPos), 0.0);
	vec4 color = a_color;

	if(a_type == 0u)
	{
		if((a_displayFlags & DISPLAY_STARS) == 0u || dot(centeredPos, centeredPos) > 1.0)
			discard;
		color.a *= radialFalloff;
	}
	else if(a_type == 1u)
	{
		if((a_displayFlags & DISPLAY_DUST) == 0u)
			discard;
		color.rgb *= vec3(0.65, 0.65, 1.0);
		color.a *= radialFalloff * 0.8;
	}
	else if(a_type == 2u)
	{
		if((a_displayFlags & DISPLAY_FILAMENTS) == 0u)
			discard;
		color.rgb *= vec3(0.55, 0.62, 1.0);
		color.a *= radialFalloff * radialFalloff * 0.45;
	}
	else if(a_type == 3u)
	{
		if((a_displayFlags & DISPLAY_H2) == 0u)
			discard;
		color.a = radialFalloff;
	}
	else if(a_type == 4u)
	{
		if((a_displayFlags & DISPLAY_H2) == 0u)
			discard;
		color.a = radialFalloff;
	}
	else
	{
		discard;
	}

	o_color = color;
}
