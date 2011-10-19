/**
 * @file colorFilterF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */


#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect RenderTexture;
uniform float brightness;
uniform float contrast;
uniform vec3  contrastBase;
uniform float saturation;
uniform vec3  lumWeights;

uniform float gamma;

void main(void) 
{
	vec3 color = vec3(texture2DRect(RenderTexture, gl_TexCoord[0].st));

	/// Apply gamma
	color = pow(color, vec3(1.0/gamma));
	
	/// Modulate brightness
	color *= brightness;

	/// Modulate contrast
	color = mix(contrastBase, color, contrast);

	/// Modulate saturation
	color = mix(vec3(dot(color, lumWeights)), color, saturation);

	gl_FragColor = vec4(color, 1.0);
}
