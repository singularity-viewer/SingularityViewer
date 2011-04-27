/**
 * @file simpleF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect RenderTexture;

void main(void) 
{
	vec3 color = vec3(texture2DRect(RenderTexture, gl_TexCoord[0].st));
	gl_FragColor = vec4(1.0 - color, 1.0);
}
