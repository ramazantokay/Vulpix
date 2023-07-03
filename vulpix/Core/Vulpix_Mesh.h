#ifndef VULPIX_MESH_H
#define VULPIX_MESH_H

#include "Buffer.h"
#include "VulpixAS.h"
#include "../Common.h"

class VulpixMesh
{
public:
	uint32_t m_vertexCount;
	uint32_t m_faceCount;
	
	Buffer m_position;
	Buffer m_attribute;
	Buffer m_index;
	Buffer m_faces;
	Buffer m_material;

	VulpixAccelerationStructure m_BLAS;

};

#endif