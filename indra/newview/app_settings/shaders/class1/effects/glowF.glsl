/** 
 * @file glowF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#version 120

uniform sampler2D diffuseMap;
uniform float glowStrength;

float kern[4] = float[4](.25,.5,.8,1.0); //Initialize the correct (non nVidia cg) way
void main()
{

	vec4 col = vec4(0.0, 0.0, 0.0, 0.0);

	col += kern[0] * texture2D(diffuseMap, gl_TexCoord[0].xy);	
	col += kern[1] * texture2D(diffuseMap, gl_TexCoord[1].xy);
	col += kern[2] * texture2D(diffuseMap, gl_TexCoord[2].xy);	
	col += kern[3] * texture2D(diffuseMap, gl_TexCoord[3].xy);	
	col += kern[3] * texture2D(diffuseMap, gl_TexCoord[0].zw);	
	col += kern[2] * texture2D(diffuseMap, gl_TexCoord[1].zw);	
	col += kern[1] * texture2D(diffuseMap, gl_TexCoord[2].zw);	
	col += kern[0] * texture2D(diffuseMap, gl_TexCoord[3].zw);	
	
	gl_FragColor = vec4(col.rgb * glowStrength, col.a);
}
