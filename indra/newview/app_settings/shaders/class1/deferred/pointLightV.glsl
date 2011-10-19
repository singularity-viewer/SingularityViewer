/** 
 * @file pointLightF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */



varying vec4 vary_light;
varying vec4 vary_fragcoord;

void main()
{
	//transform vertex
	vec4 pos = gl_ModelViewProjectionMatrix * gl_Vertex;
	vary_fragcoord = pos;
		
	vec4 tex = gl_MultiTexCoord0;
	tex.w = 1.0;
	
	vary_light = gl_MultiTexCoord0;
	
	gl_Position = pos;
		
	gl_FrontColor = gl_Color;
}
