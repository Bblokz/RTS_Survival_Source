"""Directory locks shared by the building agents (editor.lock, headless.lock).
A lock whose owner process is gone (killed by a tool timeout, so its finally never ran) is broken automatically."""
import ctypes
import os
import sys
import time

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/NomadicDeploy/'
STILL_ACTIVE = 259


def pid_alive(pid):
    kernel = ctypes.windll.kernel32
    handle = kernel.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not handle:
        return False
    code = ctypes.c_ulong()
    ok = kernel.GetExitCodeProcess(handle, ctypes.byref(code))
    kernel.CloseHandle(handle)
    return bool(ok) and code.value == STILL_ACTIVE


def owner(path):
    try:
        return open(path + '/owner.txt').read()
    except OSError:
        return ''


def acquire(name, label, poll=1):
    path = ROOT + name
    waited = 0
    while True:
        try:
            os.mkdir(path)
            open(path + '/owner.txt', 'w').write(f'{os.getpid()} {time.ctime()} {label}')
            return path
        except FileExistsError:
            text = owner(path)
            pid = int(text.split()[0]) if text.split() and text.split()[0].isdigit() else None
            age = time.time() - os.path.getmtime(path)
            if (pid is not None and not pid_alive(pid)) or (pid is None and age > 120):
                print(f'breaking stale {name} (owner: {text or "none"})', flush=True)
                release(name)
                continue
            if waited % 60 == 0:
                print(f'waiting for {name} held by: {text or "?"}', file=sys.stderr, flush=True)
            time.sleep(poll)
            waited += poll


def release(name):
    path = ROOT + name
    try:
        os.remove(path + '/owner.txt')
    except OSError:
        pass
    try:
        os.rmdir(path)
    except OSError:
        pass
