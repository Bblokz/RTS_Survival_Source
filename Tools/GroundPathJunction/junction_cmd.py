"""Client for junction_editor_server.py: send python to the throwaway editor and print its output.

    python junction_cmd.py -c "print(unreal.SystemLibrary.get_engine_version())"
    python junction_cmd.py path/to/script.py [--timeout 900]
"""
import os
import sys
import time

CMD_DIR = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/junction/cmd"


def main():
    args = sys.argv[1:]
    timeout = 600.0
    if "--timeout" in args:
        k = args.index("--timeout")
        timeout = float(args[k + 1])
        del args[k:k + 2]
    if args[0] == "-c":
        source = args[1]
    else:
        with open(args[0], "r", encoding="utf-8") as handle:
            source = handle.read()
    req_id = "%d_%d" % (int(time.time() * 1000), os.getpid())
    tmp = os.path.join(CMD_DIR, "tmp_%s" % req_id)
    with open(tmp, "w", encoding="utf-8") as handle:
        handle.write(source)
    os.replace(tmp, os.path.join(CMD_DIR, "req_%s.py" % req_id))
    result = os.path.join(CMD_DIR, "res_%s.txt" % req_id)
    start = time.time()
    while not os.path.exists(result):
        if time.time() - start > timeout:
            print("TIMEOUT after %.0f s (request %s still running or editor gone)" % (timeout, req_id))
            return 2
        time.sleep(0.5)
    with open(result, "r", encoding="utf-8") as handle:
        text = handle.read()
    os.remove(result)
    print(text)
    return 0 if text.rstrip().endswith("OK") else 1


if __name__ == "__main__":
    sys.exit(main())
