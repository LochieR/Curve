//#type compute
#version 450 core

struct Line
{
	vec4 Position;
	vec4 Color;
	int LineIndex;
	int Padding[3];
};

layout(std430, binding = 0) buffer LineBuffer {
	Line b_Lines[];
};

layout(std430, binding = 1) buffer LineData {
	int b_VertexCounts[2];
};

layout(push_constant) uniform Camera {
	mat4 ViewProjection;
} u_Camera;

layout(local_size_x = 250, local_size_y = 1, local_size_z = 1) in;

float Line1(float x)
{
	return cos(x);
}

float Line2(float x)
{
	return x * x;
}

float LineFunc(float x, int i)
{
	return i == 0 ? Line1(x) : Line2(x);
}

vec4 ProjectionMinMax(mat4 proj)
{
	float minX = 3.402823466e+38;
	float maxX = -3.402823466e+38;
	float minY = 3.402823466e+38;
	float maxY = -3.402823466e+38;

	vec4 corners[4] = vec4[](
		vec4(-1.0, -1.0, 0.0, 1.0),
		vec4(-1.0,  1.0, 0.0, 1.0),
		vec4( 1.0, -1.0, 0.0, 1.0),
		vec4( 1.0,  1.0, 0.0, 1.0)
	);

	for (int i = 0; i < 4; i++)
	{
		vec4 corner = corners[i];
		vec4 worldPos = inverse(proj) * corner;
		worldPos = vec4(worldPos.x / worldPos.w, worldPos.y / worldPos.w, worldPos.z / worldPos.w, worldPos.w / worldPos.w);

		minX = min(minX, worldPos.x);
		maxX = max(maxX, worldPos.x);
		minY = min(minY, worldPos.y);
		maxY = max(maxY, worldPos.y);
	}

	return vec4(minX, maxX, minY, maxY);
}

void main()
{
	int desiredVertexCount = 2000; // per line

	uint globalIndex = gl_WorkGroupID.x * gl_WorkGroupSize.x + gl_LocalInvocationID.x;

	if (globalIndex < 2000)
	{
		uint index = globalIndex;

		vec4 minMax = ProjectionMinMax(u_Camera.ViewProjection);

		float minX = minMax.x - 0.5;
		float maxX = minMax.y + 0.5;

		float range = maxX - minX;
		float step = range / (desiredVertexCount - 1);

		float x = minX;
		for (int i = 0; i != index; x += step, i++);

		int lineCount = 2;

		for (int i = 0; i < lineCount; i++)
		{
			b_Lines[2000 * i + index].Position = vec4(x, LineFunc(x, i), 0.0, 1.0);
			b_Lines[2000 * i + index].Color = vec4(1.0, 1.0, 1.0, 1.0);
			b_Lines[2000 * i + index].LineIndex = i + 1;
		}
	}

	/*b_VertexCounts[0] = 0;

	for (float x = minX; x <= maxX; x += step)
	{
		b_Lines[b_VertexCounts[0]].Position = vec4(x, Line1(x), 0.0, 1.0);
		b_Lines[b_VertexCounts[0]].Color = vec4(1.0, 1.0, 1.0, 1.0);
		b_Lines[b_VertexCounts[0]].LineIndex = 1;

		b_VertexCounts[0]++;
	}

	b_VertexCounts[1] = 0;

	for (float x = minX; x <= maxX; x += step)
	{
		b_Lines[b_VertexCounts[1] + b_VertexCounts[0]].Position = vec4(x, Line2(x), 0.0, 1.0);
		b_Lines[b_VertexCounts[1] + b_VertexCounts[0]].Color = vec4(1.0, 1.0, 1.0, 1.0);
		b_Lines[b_VertexCounts[1] + b_VertexCounts[0]].LineIndex = 2;

		b_VertexCounts[1]++;
	}*/
}
