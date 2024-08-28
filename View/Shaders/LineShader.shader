#type vertex
#version 450 core

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in int a_LineIndex;
layout(location = 3) in int a_Pad1;
layout(location = 4) in int a_Pad2;
layout(location = 5) in int a_Pad3;

layout(location = 0) out vec4 v_Color;
layout(location = 1) out flat int v_LineIndex;

layout(push_constant) uniform Camera
{
	mat4 ViewProjection;
} u_Camera;

void main()
{
	int x = a_Pad1 + a_Pad2 + a_Pad3;
	v_Color = a_Color;
	v_LineIndex = a_LineIndex;
	gl_Position = u_Camera.ViewProjection * a_Position;
}

#type fragment
#version 450 core

layout(location = 0) in vec4 v_Color;
layout(location = 1) in flat int v_LineIndex;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_LineIndex;

void main()
{
	o_Color = v_Color;
	o_LineIndex = v_LineIndex;
}
