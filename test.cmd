@echo off
set entryFile=test_calculator.cpp
set outputFile=test_calculator.exe

set ignoredWarnings=-wd4100 -wd4189 -wd4505 -wd4201
set compileFlags=-nologo -W4 -WX %ignoredWarnings% -GR- -Gm- -EHa -MT -Oi -Od -Zi
set defineFlags=-DDEBUG
set linkFlags=/link -incremental:no -opt:ref

IF NOT EXIST build (mkdir build)
pushd build
	del *.pdb > NUL 2> NUL
	del %outputFile% > NUL 2> NUL
	cl %compileFlags% %defineFlags% ..\%entryFile% -Fe:%outputFile% %linkFlags% && .\%outputFile%
popd