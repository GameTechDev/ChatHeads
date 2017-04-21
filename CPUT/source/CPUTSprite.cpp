/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

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

    
