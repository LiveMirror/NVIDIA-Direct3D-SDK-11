start compile_blur.bat
start compile_hbao.bat

del "Bin\FullScreenTriangle_VS.h"
fxc /O3 /T vs_5_0 Source/FullScreenTriangle_VS.hlsl /E FullScreenTriangle_VS11 /Fh "Bin/FullScreenTriangle_VS.h"

del "Bin\DownsampleDepth_PS.h"
fxc /O3 /T ps_5_0 Source/DownsampleDepth_PS.hlsl /E DownsampleDepth_PS11 /Fh "Bin/DownsampleDepth_PS.h"

del "Bin\LinearizeDepthNoMSAA_PS.h"
fxc /O3 /T ps_5_0 Source/LinearizeDepthNoMSAA_PS.hlsl /E LinearizeDepthNoMSAA_PS11 /Fh "Bin/LinearizeDepthNoMSAA_PS.h"

del "Bin\ResolveAndLinearizeDepthMSAA_PS.h"
fxc /O3 /T ps_5_0 Source/ResolveAndLinearizeDepthMSAA_PS.hlsl /E ResolveAndLinearizeDepthMSAA_PS11 /Fh "Bin/ResolveAndLinearizeDepthMSAA_PS.h"

pause
