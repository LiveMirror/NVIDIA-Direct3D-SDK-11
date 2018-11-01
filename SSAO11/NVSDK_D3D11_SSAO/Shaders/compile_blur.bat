del "Bin\Blur_Composite.h"
fxc /T ps_5_0 /E Blur_Composite_PS11 "Source/Blur_Composite_PS.hlsl" /Fh "Bin/Blur_Composite.h"

FOR /L %%A IN (0,2,16) DO helper_blur_cs.bat %%A

pause
