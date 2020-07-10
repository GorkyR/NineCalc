@echo off
set ignoredWarnings=-wd4100 -wd4189 -wd4505 -wd4201
set compileFlags=-nologo -W4 -WX %ignoredWarnings% -GR- -Gm- -EHsc -EHa- -MT -Oi -Od -Zi
set defineFlags=-DSTB_TRUETYPE_IMPLEMENTATION -DVERSION=v0.4.1-alpha -DDEBUG
set linkFlags=/link -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST build (mkdir build)
pushd build
	del *.pdb > NUL 2> NUL
	cl %compileFlags% %defineFlags% ..\win_ninecalc.cpp -Fe:ninecalc.exe %linkFlags%
popd