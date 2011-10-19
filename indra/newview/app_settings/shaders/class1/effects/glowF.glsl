/** 
 * @file glowF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */



uniform sampler2D diffuseMap;
uniform float glowStrength;

vec4 kern = vec4(.25,.5,.8,1.0);
void main()
{

	vec4 col = vec4(0.0, 0.0, 0.0, 0.0);

	col += kern.x * texture2D(diffuseMap, gl_TexCoord[0].xy);	
	col += kern.y * texture2D(diffuseMap, gl_TexCoord[1].xy);
	col += kern.z * texture2D(diffuseMap, gl_TexCoord[2].xy);	
	col += kern.w * texture2D(diffuseMap, gl_TexCoord[3].xy);	
	col += kern.w * texture2D(diffuseMap, gl_TexCoord[0].zw);	
	col += kern.z * texture2D(diffuseMap, gl_TexCoord[1].zw);	
	col += kern.y * texture2D(diffuseMap, gl_TexCoord[2].zw);	
	col += kern.x * texture2D(diffuseMap, gl_TexCoord[3].zw);	
	
	gl_FragColor = vec4(col.rgb * glowStrength, col.a);
}
