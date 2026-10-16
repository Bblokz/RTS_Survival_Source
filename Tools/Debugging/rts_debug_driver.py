# Runs a program under lldb, logs every stop with a symbolized stack, steps over hard-coded breakpoints
# (Unreal's ensure calls __debugbreak when a debugger is attached) and dumps all threads on a crash.
# Settings come from environment variables:
#   RTS_DBG_EXE, RTS_DBG_ARGS (one string, split on '|'), RTS_DBG_CWD, RTS_DBG_LOG,
#   RTS_DBG_MAX_SECONDS, RTS_DBG_DUMP (minidump path written on a crash).
import os
import time
import lldb

BREAKPOINT_CODE = "0x80000003"
THREAD_NAME_CODE = "0x406d1388"
CPP_EXCEPTION_CODE = "0xe06d7363"
FRAMES_ON_STOP = 40
FRAMES_PER_THREAD_ON_CRASH = 25


class Log:
    def __init__(self, path):
        self.file = open(path, "w", encoding="utf-8")

    def write(self, text):
        line = "[%s] %s" % (time.strftime("%H:%M:%S"), text)
        self.file.write(line + "\n")
        self.file.flush()


def describe_frame(frame):
    module = frame.GetModule().GetFileSpec().GetFilename() or "?"
    function = frame.GetFunctionName() or frame.GetSymbol().GetName() or "0x%x" % frame.GetPC()
    line_entry = frame.GetLineEntry()
    location = ""
    if line_entry.IsValid() and line_entry.GetFileSpec().GetFilename():
        location = " [%s:%d]" % (line_entry.GetFileSpec().GetFilename(), line_entry.GetLine())
    return "%s!%s%s" % (module, function, location)


def log_thread(log, thread, frame_limit):
    log.write("  thread #%d tid=%d name=%s" % (thread.GetIndexID(), thread.GetThreadID(), thread.GetName()))
    for index in range(min(thread.GetNumFrames(), frame_limit)):
        log.write("    #%d %s" % (index, describe_frame(thread.GetFrameAtIndex(index))))


def get_stop_description(thread):
    return thread.GetStopDescription(256) or ""


def step_over_hardcoded_break(process, thread):
    frame = thread.GetFrameAtIndex(0)
    program_counter = frame.GetPC()
    error = lldb.SBError()
    instruction = process.ReadMemory(program_counter, 1, error)
    if error.Success() and instruction == b"\xcc":
        frame.SetPC(program_counter + 1)
        return True
    return False


def run():
    log = Log(os.environ["RTS_DBG_LOG"])
    executable = os.environ["RTS_DBG_EXE"]
    arguments = [Argument for Argument in os.environ.get("RTS_DBG_ARGS", "").split("|") if Argument]
    working_directory = os.environ.get("RTS_DBG_CWD", os.path.dirname(executable))
    max_seconds = float(os.environ.get("RTS_DBG_MAX_SECONDS", "3600"))
    dump_path = os.environ.get("RTS_DBG_DUMP", "")

    debugger = lldb.SBDebugger.Create()
    debugger.SetAsync(True)
    target = debugger.CreateTarget(executable)
    if not target.IsValid():
        log.write("RTS_DBG target_invalid %s" % executable)
        return
    listener = debugger.GetListener()
    launch_info = lldb.SBLaunchInfo(arguments)
    launch_info.SetWorkingDirectory(working_directory)
    error = lldb.SBError()
    process = target.Launch(launch_info, error)
    if not error.Success():
        log.write("RTS_DBG launch_failed %s" % error.GetCString())
        return
    log.write("RTS_DBG launched pid=%d exe=%s args=%s" % (process.GetProcessID(), executable, " ".join(arguments)))

    start_seconds = time.time()
    breaks = 0
    last_exception_key = None
    event = lldb.SBEvent()
    while True:
        if time.time() - start_seconds > max_seconds:
            log.write("RTS_DBG timeout after %.0fs; killing the process" % max_seconds)
            process.Kill()
            break
        if not listener.WaitForEvent(1, event):
            continue
        if not lldb.SBProcess.EventIsProcessEvent(event):
            continue
        state = lldb.SBProcess.GetStateFromEvent(event)
        if state == lldb.eStateExited:
            log.write("RTS_DBG exited code=%d after %.0fs breaks=%d" % (process.GetExitStatus(),
                                                                        time.time() - start_seconds, breaks))
            break
        if state == lldb.eStateCrashed or state == lldb.eStateDetached:
            log.write("RTS_DBG state=%s" % debugger.StateAsCString(state))
            break
        if state != lldb.eStateStopped:
            continue
        if lldb.SBProcess.GetRestartedFromEvent(event):
            continue
        stopped_thread = None
        for thread in process:
            if thread.GetStopReason() not in (lldb.eStopReasonNone, lldb.eStopReasonInvalid):
                stopped_thread = thread
                break
        if stopped_thread is None:
            process.Continue()
            continue
        description = get_stop_description(stopped_thread)
        lowered = description.lower()
        if THREAD_NAME_CODE in lowered:
            process.Continue()
            continue
        if BREAKPOINT_CODE in lowered or stopped_thread.GetStopReason() == lldb.eStopReasonBreakpoint:
            breaks += 1
            log.write("RTS_DBG BREAK #%d %s" % (breaks, description))
            log_thread(log, stopped_thread, FRAMES_ON_STOP)
            stepped = step_over_hardcoded_break(process, stopped_thread)
            log.write("RTS_DBG continue stepped_over_int3=%s" % stepped)
            process.Continue()
            continue
        exception_key = (stopped_thread.GetThreadID(), stopped_thread.GetFrameAtIndex(0).GetPC(), lowered)
        if exception_key == last_exception_key or CPP_EXCEPTION_CODE not in lowered:
            is_repeat = exception_key == last_exception_key
            log.write("RTS_DBG EXCEPTION %s %s" % ("second_chance" if is_repeat else "first_chance", description))
            log_thread(log, stopped_thread, FRAMES_ON_STOP)
            if is_repeat:
                log.write("RTS_DBG CRASH all threads:")
                for thread in process:
                    if thread.GetThreadID() != stopped_thread.GetThreadID():
                        log_thread(log, thread, FRAMES_PER_THREAD_ON_CRASH)
                if dump_path:
                    result = lldb.SBCommandReturnObject()
                    debugger.GetCommandInterpreter().HandleCommand("process save-core \"%s\"" % dump_path, result)
                    log.write("RTS_DBG save-core ok=%s %s" % (result.Succeeded(), result.GetOutput() or result.GetError()))
                process.Kill()
                break
        else:
            log.write("RTS_DBG cpp_exception %s at %s" % (description, describe_frame(stopped_thread.GetFrameAtIndex(0))))
        last_exception_key = exception_key
        process.Continue()
    log.write("RTS_DBG done")


def __lldb_init_module(debugger, internal_dict):
    run()
