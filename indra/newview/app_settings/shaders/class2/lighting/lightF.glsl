/** 
 * @file lightF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */
 


vec3 atmosLighting(vec3 light);
vec3 scaleSoftClip(vec3 light);

void default_lighting() 
{
	vec4 color = diffuseLookup(gl_TexCoord[0].xy) * gl_Color;
	
	color.rgb = atmosLighting(color.rgb);

	color.rgb = scaleSoftClip(color.rgb);

	gl_FragColor = color;
}

