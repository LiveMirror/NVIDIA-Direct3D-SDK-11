del "Bin\BlurX_CS_%1.h
fxc /T cs_5_0 /E BlurX_CS11_%1 "Source/BlurX_CS.hlsl" /Fh Bin/BlurX_CS_%1.h /DKERNEL_RADIUS=%1 /DHALF_RES_AO=0

del "Bin\UpsampleAndBlurX_CS_%1.h
fxc /T cs_5_0 /E UpsampleAndBlurX_CS11_%1 "Source/BlurX_CS.hlsl" /Fh Bin/UpsampleAndBlurX_CS_%1.h /DKERNEL_RADIUS=%1 /DHALF_RES_AO=1

del "Bin\BlurY_CS_%1.h"
fxc /T cs_5_0 /E BlurY_CS11_%1 "Source/BlurY_CS.hlsl" /Fh Bin/BlurY_CS_%1.h /DKERNEL_RADIUS=%1
