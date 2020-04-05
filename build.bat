@echo off
set ignoredWarnings=-wd4100 -wd4189 -wd4505 -wd4201
set compileFlags=-nologo -W4 -WX %ignoredWarnings% -GR- -Gm- -EHsc -EHa- -MT -Oi -Od -Zi
set defineFlags=-DDEBUG
set linkFlags=/link -incremental:no -opt:ref user32.lib gdi32.lib
set dllExports=/EXPORT:update_and_render

IF NOT EXIST build (mkdir build)
pushd build
	del *.pdb > NUL 2> NUL
	cl %compileFlags% %defineFlags% ..\ninecalc.cpp %linkFlags%
popd