
#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect RenderTexture;
uniform int horizontalPass;

uniform float offset[2] = float[2]( 1.3846153846, 3.2307692308 );
uniform float weight[3] = float[3]( 0.2270270270, 0.3162162162, 0.0702702703 );
 
void main(void)
{
	vec4 color = texture2DRect(RenderTexture, gl_TexCoord[0].st)*weight[0];
	
	
	if(horizontalPass == 1)
	{
		color += weight[1] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x+offset[0],gl_TexCoord[0].y));
		color += weight[1] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x-offset[0],gl_TexCoord[0].y));
		color += weight[2] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x+offset[1],gl_TexCoord[0].y));
		color += weight[2] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x-offset[1],gl_TexCoord[0].y));
		
	}
	else
	{
		color += weight[1] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x,gl_TexCoord[0].y+offset[0]));
		color += weight[1] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x,gl_TexCoord[0].y-offset[0]));
		color += weight[2] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x,gl_TexCoord[0].y+offset[1]));
		color += weight[2] * texture2DRect(RenderTexture, vec2(gl_TexCoord[0].x,gl_TexCoord[0].y-offset[1]));
	}
	gl_FragColor = color;
}