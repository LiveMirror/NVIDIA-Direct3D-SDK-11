del "Bin\HBAO_PS.h"
fxc /O3 /T ps_5_0 Source/HBAO_PS.hlsl /E HBAO_PS11 /Fh "Bin/HBAO_PS.h"

FOR /L %%A IN (1,1,8) DO helper_hbao_cs.bat %%A

pause
