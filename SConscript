# RT-Thread building script for bridge

import os
from building import *
Import('env')

cwd = GetCurrentDir()
objs = []
list = os.listdir(cwd)

if GetDepend('PKG_USING_STATE_MACHINE'):
    for d in list:
        path = os.path.join(cwd, d)
        if os.path.isfile(os.path.join(path, 'SConscript')):
            objs = objs + SConscript(os.path.join(d, 'SConscript'))

# 只编译 RT-Thread 相关代码，只包含 RT 线程安全实现

Import('env')

src = [
    'src/state_machine_rt.c',
    'examples/rtthread_app.c',
    # 如有其他 RT-Thread 相关源文件可在此添加
]

env.Program(target='rtthread_app', source=src)

Return('objs')
