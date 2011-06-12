/** 
 * @file lightFullbrightF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */
 
#version 120


uniform sampler2D diffuseMap;

void fullbright_lighting()
{
	gl_FragColor = texture2D(diffuseMap, gl_TexCoord[0].xy);
}

