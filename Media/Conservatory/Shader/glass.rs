
[DepthStencilStateDX11]
DepthEnable = true
DepthFunc = D3D11_COMPARISON_GREATER_EQUAL
DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO

[RasterizerStateDX11]
CullMode = D3D11_CULL_NONE

[SamplerDX11_1]

[SamplerDX11_2]
ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL
Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT

[RenderTargetBlendStateDX11_1] 
BlendEnable = true 
SrcBlend = D3D11_BLEND_SRC_ALPHA 
DestBlend = D3D11_BLEND_INV_SRC_ALPHA 
BlendOp = D3D11_BLEND_OP_ADD

[DepthStencilStateDX11]
depthwritemask = 0
Depthenable = false

[DepthStencilStateOGL]
DepthEnable = GL_FALSE


[RasterizerStateOGL]
CullingEnabled = GL_FALSE

[RenderTargetBlendStateOGL]
blendenable = true
srcblend = GL_ONE
destblend = GL_ONE_MINUS_SRC_ALPHA
srcblendalpha = GL_ONE
destblendalpha = GL_ONE
blendop = gl_func_add
blendopalpha = gl_func_add