# Waits until no process holds a ruin kit material asset open (every editor that has them loaded keeps the files
# open, and the headless build cannot replace them meanwhile). Prints the holders while it waits.
# usage: powershell -ExecutionPolicy Bypass -File ruin_wait_unlocked.ps1 [-TimeoutSeconds 900] [-Once]
#   -Once: only list the holders (exit 0 = free, 1 = held)
param([int]$TimeoutSeconds = 900, [switch]$Once,
      [string]$Folder = 'D:\UE5Projects\RTS_Survival\Content\RTS_Survival\Blueprints\Environment\Berlin\RuinKit\Materials')
$src = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class RuinRmLock {
  [StructLayout(LayoutKind.Sequential)] struct RM_UNIQUE_PROCESS { public int dwProcessId; public System.Runtime.InteropServices.ComTypes.FILETIME ProcessStartTime; }
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] struct RM_PROCESS_INFO {
    public RM_UNIQUE_PROCESS Process;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string strAppName;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string strServiceShortName;
    public int ApplicationType; public uint AppStatus; public uint TSSessionId; [MarshalAs(UnmanagedType.Bool)] public bool bRestartable; }
  [DllImport("rstrtmgr.dll", CharSet = CharSet.Unicode)] static extern int RmRegisterResources(uint h, uint nFiles, string[] files, uint nApps, [In] RM_UNIQUE_PROCESS[] apps, uint nServices, string[] services);
  [DllImport("rstrtmgr.dll", CharSet = CharSet.Auto)] static extern int RmStartSession(out uint h, int flags, string key);
  [DllImport("rstrtmgr.dll")] static extern int RmEndSession(uint h);
  [DllImport("rstrtmgr.dll")] static extern int RmGetList(uint h, out uint needed, ref uint count, [In, Out] RM_PROCESS_INFO[] info, ref uint reasons);
  public static List<int> Who(string[] paths) {
    var result = new List<int>(); uint h; string key = Guid.NewGuid().ToString();
    if (RmStartSession(out h, 0, key) != 0) return result;
    try {
      if (RmRegisterResources(h, (uint)paths.Length, paths, 0, null, 0, null) != 0) return result;
      uint needed = 0, count = 0, reasons = 0;
      int r = RmGetList(h, out needed, ref count, null, ref reasons);
      if (r == 234) { var info = new RM_PROCESS_INFO[needed]; count = needed;
        if (RmGetList(h, out needed, ref count, info, ref reasons) == 0) for (int i = 0; i < count; i++) result.Add(info[i].Process.dwProcessId); }
    } finally { RmEndSession(h); }
    return result; }
}
'@
Add-Type -TypeDefinition $src
$files = @(Get-ChildItem $Folder -Filter *.uasset | ForEach-Object { $_.FullName })
$start = Get-Date
$last = ''
while ($true) {
  $who = @([RuinRmLock]::Who($files) | Sort-Object -Unique)
  if ($who.Count -eq 0) { 'ruin kit materials: free'; exit 0 }
  $text = ($who | ForEach-Object { $p = Get-Process -Id $_ -ErrorAction SilentlyContinue; '{0}({1})' -f $_, $p.ProcessName }) -join ' '
  if ($text -ne $last) { 'held by ' + $text; $last = $text }
  if ($Once) { exit 1 }
  if (((Get-Date) - $start).TotalSeconds -gt $TimeoutSeconds) { 'timeout'; exit 2 }
  Start-Sleep -Seconds 3
}
