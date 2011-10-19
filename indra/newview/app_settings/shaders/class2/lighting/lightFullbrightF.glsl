/** 
 * @file lightFullbrightF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */
 


vec3 fullbrightAtmosTransport(vec3 light);
vec3 fullbrightScaleSoftClip(vec3 light);

void fullbright_lighting()
{
	vec4 color = diffuseLookup(gl_TexCoord[0].xy) * gl_Color;
	
	color.rgb = fullbrightAtmosTransport(color.rgb);
	
	color.rgb = fullbrightScaleSoftClip(color.rgb);

	gl_FragColor = color;
}

