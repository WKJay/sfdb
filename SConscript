from building import *

cwd     = GetCurrentDir()
CPPPATH = [cwd]
src     = Glob('*.c')

if GetDepend('SFDB_USING_EXAMPLE'):
    src = src + ['examples/rtthread/example.c']

group = DefineGroup('sfdb', src, depend = [''], CPPPATH = CPPPATH)

Return('group')
