#version 430 core
out vec4 FragColor;
	
in vec2 TexCoords;
	
uniform sampler2D tex;
	
void main()
{             
    vec3 texCol = texture(tex, TexCoords).rgb;
	const float gamma = 2.2;
    FragColor = vec4(pow(texCol, vec3(1.0 / gamma)), 1.0);
}