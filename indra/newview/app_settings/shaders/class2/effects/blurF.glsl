/**
 * @file blurf.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */


#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect RenderTexture;
uniform float bloomStrength;

varying vec4 gl_TexCoord[gl_MaxTextureCoords];

float blurWeights[4] = float[4](.05,.1,.2,.3);

void main(void) 
{
	vec3 color = blurWeights[0] * texture2DRect(RenderTexture, gl_TexCoord[0].st).rgb;
	color+= blurWeights[1] * texture2DRect(RenderTexture, gl_TexCoord[1].st).rgb;
	color+= blurWeights[2] * texture2DRect(RenderTexture, gl_TexCoord[2].st).rgb;
	color+= blurWeights[3] * texture2DRect(RenderTexture, gl_TexCoord[3].st).rgb;
	color+= blurWeights[2] * texture2DRect(RenderTexture, gl_TexCoord[4].st).rgb;
	color+= blurWeights[1] * texture2DRect(RenderTexture, gl_TexCoord[5].st).rgb;
	color+= blurWeights[0] * texture2DRect(RenderTexture, gl_TexCoord[6].st).rgb;

	color *= bloomStrength;

	gl_FragColor = vec4(color, 1.0);
}
