@echo off
set module_dir="C:\Program Files\SDL_Pango Development\lib\pango\1.4.0\modules"
set module=%1%

if not exist %module_dir% goto ERROR
echo ���W���[�����C���X�g�[���f�B���N�g���ɃR�s�[���Ă��܂�
copy %module% %module_dir%
goto END
:ERROR
echo �C���X�g�[���f�B���N�g����������܂���
:END
