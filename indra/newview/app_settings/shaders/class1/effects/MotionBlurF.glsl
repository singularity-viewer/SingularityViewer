/**
 * @file colorFilterF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */
 
#extension GL_ARB_texture_rectangle : enable
 
#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2DRect tex0;
uniform sampler2DRect tex1;
uniform mat4 inv_proj;
uniform mat4 prev_proj;
uniform vec2 screen_res;
uniform int blur_strength;

VARYING vec2 vary_texcoord0;

#define SAMPLE_COUNT 10

vec4 getPosition(vec2 pos_screen, out vec4 ndc)
{
	float depth = texture2DRect(tex1, pos_screen.xy).r;
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

void main(void) 
{
	vec4 ndc;
	vec4 pos = getPosition(vary_texcoord0,ndc);
	vec4 prev_pos = prev_proj * pos;
	prev_pos/=prev_pos.w;
	prev_pos.w = 1.0;
	vec2 vel = ((ndc.xy-prev_pos.xy) * .5) * screen_res * .01 * blur_strength * 1.0/SAMPLE_COUNT;
	float len = length(vel);
	vel = normalize(vel) * min(len, 50);
	vec3 color = texture2DRect(tex0, vary_texcoord0.st).rgb; 
	vec2 texcoord = vary_texcoord0 + vel;
	for(int i = 1; i < SAMPLE_COUNT; ++i, texcoord += vel)  
	{  
		color += texture2DRect(tex0, texcoord.st).rgb; 
	}
	frag_color = vec4(color / SAMPLE_COUNT, 1.0);
}
