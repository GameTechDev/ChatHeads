set shadergenerator=..\..\..\..\Bin\ShaderGenerator.exe
set templatedir=..\..\..\..\ShaderGenerator
%shadergenerator% HLSL ..\Shader\ground.shd 				%templatedir%\shaderTemplate.hlsl ..\Shader\ground.fx				  
%shadergenerator% HLSL ..\Shader\base.shd 					%templatedir%\shaderTemplate.hlsl ..\Shader\base.fx
%shadergenerator% HLSL ..\Shader\conservatoryTile_r05.shd 	%templatedir%\shaderTemplate.hlsl ..\Shader\conservatoryTile_r05.fx
%shadergenerator% HLSL ..\Shader\conservatoryTile_r10.shd 	%templatedir%\shaderTemplate.hlsl ..\Shader\conservatoryTile_r10.fx
%shadergenerator% HLSL ..\Shader\cornerPosts.shd			%templatedir%\shaderTemplate.hlsl ..\Shader\cornerPosts.fx
%shadergenerator% HLSL ..\Shader\fence.shd 					%templatedir%\shaderTemplate.hlsl ..\Shader\fence.fx
%shadergenerator% HLSL ..\Shader\fenceTubes.shd 			%templatedir%\shaderTemplate.hlsl ..\Shader\fenceTubes.fx
%shadergenerator% HLSL ..\Shader\glass.shd 					%templatedir%\shaderTemplate.hlsl ..\Shader\glass.fx
%shadergenerator% HLSL ..\Shader\glassTubes.shd				%templatedir%\shaderTemplate.hlsl ..\Shader\glassTubes.fx
%shadergenerator% HLSL ..\Shader\platform.shd 				%templatedir%\shaderTemplate.hlsl ..\Shader\platform.fx				  
%shadergenerator% HLSL ..\Shader\barkTreeA.shd 				%templatedir%\shaderTemplate.hlsl ..\Shader\barkTreeA.fx
%shadergenerator% HLSL ..\Shader\leaves.shd 				%templatedir%\shaderTemplate.hlsl ..\Shader\leaves.fx				  
%shadergenerator% HLSL ..\Shader\barrier.shd				%templatedir%\shaderTemplate.hlsl ..\Shader\barrier.fx

%shadergenerator% GLSL ..\Shader\ground.shd 				%templatedir%\shaderTemplate.glsl ..\Shader\ground.glsl
%shadergenerator% GLSL ..\Shader\base.shd 					%templatedir%\shaderTemplate.glsl ..\Shader\base.glsl
%shadergenerator% GLSL ..\Shader\conservatoryTile_r05.shd 	%templatedir%\shaderTemplate.glsl ..\Shader\conservatoryTile_r05.glsl
%shadergenerator% GLSL ..\Shader\conservatoryTile_r10.shd 	%templatedir%\shaderTemplate.glsl ..\Shader\conservatoryTile_r10.glsl
%shadergenerator% GLSL ..\Shader\cornerPosts.shd			%templatedir%\shaderTemplate.glsl ..\Shader\cornerPosts.glsl
%shadergenerator% GLSL ..\Shader\fence.shd 					%templatedir%\shaderTemplate.glsl ..\Shader\fence.glsl
%shadergenerator% GLSL ..\Shader\fenceTubes.shd 			%templatedir%\shaderTemplate.glsl ..\Shader\fenceTubes.glsl
%shadergenerator% GLSL ..\Shader\glass.shd 					%templatedir%\shaderTemplate.glsl ..\Shader\glass.glsl
%shadergenerator% GLSL ..\Shader\glassTubes.shd				%templatedir%\shaderTemplate.glsl ..\Shader\glassTubes.glsl
%shadergenerator% GLSL ..\Shader\platform.shd 				%templatedir%\shaderTemplate.glsl ..\Shader\platform.glsl
%shadergenerator% GLSL ..\Shader\barkTreeA.shd 				%templatedir%\shaderTemplate.glsl ..\Shader\barkTreeA.glsl
%shadergenerator% GLSL ..\Shader\leaves.shd 				%templatedir%\shaderTemplate.glsl ..\Shader\leaves.glsl
%shadergenerator% GLSL ..\Shader\barrier.shd				%templatedir%\shaderTemplate.glsl ..\Shader\barrier.glsl
