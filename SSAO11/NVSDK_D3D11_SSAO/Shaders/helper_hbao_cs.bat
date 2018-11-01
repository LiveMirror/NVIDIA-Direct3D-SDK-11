del "Bin\HBAOX_CS_%1.h"
fxc /T cs_5_0 /E HBAOX_CS11_%1 "Source/HBAO_CS.hlsl" /Fh "Bin/HBAOX_CS_%1.h" /DSTEP_SIZE=%1

del "Bin\HBAOY_CS_%1.h"
fxc /T cs_5_0 /E HBAOY_CS11_%1 "Source/HBAO_CS.hlsl" /Fh "Bin/HBAOY_CS_%1.h" /DSTEP_SIZE=%1
