@echo off
set prereq_dir="..\..\..\Prereq\lib"
set bin_dir="..\bin\"%1%

if not exist %bin_dir% mkdir %bin_dir%
if not exist %prereq_dir% goto ERROR
echo �K�v�ȋ��L���C�u�������R�s�[���Ă��܂�
copy %prereq_dir%\*.dll %bin_dir%
goto END
:ERROR
echo �K�v�ȋ��L���C�u������������܂���ł���
:END
