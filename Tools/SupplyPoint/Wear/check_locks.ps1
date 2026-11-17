# Prints the processes holding any supply point material file open (Restart Manager), or FREE.
$code = @'
using System; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class RMSP {
 [StructLayout(LayoutKind.Sequential)] struct RM_UNIQUE_PROCESS { public int dwProcessId; public System.Runtime.InteropServices.ComTypes.FILETIME ProcessStartTime; }
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct RM_PROCESS_INFO { public RM_UNIQUE_PROCESS Process; [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string strAppName; [MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)] public string strServiceShortName; public int ApplicationType; public uint AppStatus; public uint TSSessionId; [MarshalAs(UnmanagedType.Bool)] public bool bRestartable; }
 [DllImport("rstrtmgr.dll", CharSet=CharSet.Unicode)] static extern int RmStartSession(out uint h, int f, string k);
 [DllImport("rstrtmgr.dll")] static extern int RmEndSession(uint h);
 [DllImport("rstrtmgr.dll", CharSet=CharSet.Unicode)] static extern int RmRegisterResources(uint h, uint n, string[] files, uint a, IntPtr b, uint c, string[] d);
 [DllImport("rstrtmgr.dll")] static extern int RmGetList(uint h, out uint needed, ref uint n, [In, Out] RM_PROCESS_INFO[] info, ref uint reasons);
 public static List<string> Who(string[] paths) {
  var res = new List<string>(); uint h; RmStartSession(out h, 0, Guid.NewGuid().ToString());
  try { RmRegisterResources(h, (uint)paths.Length, paths, 0, IntPtr.Zero, 0, null); uint needed=0, n=0, r=0;
   RmGetList(h, out needed, ref n, null, ref r); var info = new RM_PROCESS_INFO[needed]; n = needed;
   if (needed > 0 && RmGetList(h, out needed, ref n, info, ref r) == 0) foreach (var i in info) res.Add(i.Process.dwProcessId + " " + i.strAppName);
  } finally { RmEndSession(h); } return res; } }
'@
Add-Type -TypeDefinition $code
$d = "D:\UE5Projects\RTS_Survival\Content\RTS_Survival\Blueprints\Environment\SupplyPoint\Materials\"
$who = [RMSP]::Who(@(($d + "M_SupplyPoint_WornSteel.uasset"), ($d + "MI_SupplyPoint_CraneSteel.uasset")))
if ($who.Count -eq 0) { "FREE" } else { "HELD " + ($who -join '; ') }
