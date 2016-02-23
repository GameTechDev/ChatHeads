//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

#include "CPUT.h"
#include "CPUTSprite.h"
#include "CPUTMesh.h"
#include "CPUTMaterial.h"
#include "CPUTInputLayoutCache.h"

CPUTSprite::CPUTSprite() :
mpMaterial(NULL),
mpMesh(NULL)
{
}

CPUTSprite::~CPUTSprite()
{
    SAFE_RELEASE(mpMaterial);
    SAFE_RELEASE(mpMesh);
}

//-----------------------------------------------
CPUTSprite* CPUTSprite::Create(
    float          spriteX,
    float          spriteY,
    float          spriteWidth,
    float          spriteHeight,
    CPUTMaterial*  pMaterial
    )
{
    CPUTSprite* pCPUTSprite = new CPUTSprite();

    pCPUTSprite->mpMaterial = pMaterial;
    pMaterial->AddRef();

    CPUTBufferElementInfo pVertexInfo[3] = {
            { "POSITION", 0, 0, CPUT_F32, 3, 3 * sizeof(float), 0 },
            { "TEXCOORD", 0, 1, CPUT_F32, 2, 2 * sizeof(float), 3 * sizeof(float) }
    };
    const float top = -spriteY;
    const float bottom = -spriteY - spriteHeight;
    const float left = spriteX;
    const float right = spriteX + spriteWidth;
    SpriteVertex pVertices[] = {
            { left,     top,    1.0f, 0.0f, 0.0f },
            { right,    top,    1.0f, 1.0f, 0.0f },
            { left,     bottom, 1.0f, 0.0f, 1.0f },

            { right,    top,    1.0f, 1.0f, 0.0f },
            { right,    bottom, 1.0f, 1.0f, 1.0f },
            { left,     bottom, 1.0f, 0.0f, 1.0f }
    };

    pCPUTSprite->mpMesh = CPUTMesh::Create();
    pCPUTSprite->mpMesh->CreateNativeResources(NULL, 0, 2, pVertexInfo, 6, pVertices, NULL, 0, NULL);
    pCPUTSprite->mpMesh->SetMeshTopology(eCPUT_MESH_TOPOLOGY::CPUT_TOPOLOGY_INDEXED_TRIANGLE_LIST);
    return pCPUTSprite;
} 

void CPUTSprite::DrawSprite(CPUTRenderParameters &renderParams) 
{ 
    DrawSprite(renderParams, *mpMaterial); 
}

//-----------------------------------------
void CPUTSprite::DrawSprite(
    CPUTRenderParameters &renderParams,
    CPUTMaterial         &material
    )
{
    material.SetRenderStates();
    CPUTInputLayoutCache::GetInputLayoutCache()->Apply(mpMesh, &material);
    mpMesh->Draw();

}

    
