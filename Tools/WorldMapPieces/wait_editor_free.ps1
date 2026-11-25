# Waits until the piece assets can be saved again. The shared editor keeps them open, and saves fail while it plays.
# Exit 0: the files are unlocked (editor closed): rebuild headless with run_build.sh.
# Exit 3: the editor is still open but no longer in Play In Editor: apply the change through the editor instead.
# Exit 2: still busy after the deadline.
param([int]$Minutes = 180, [switch]$Once)
$probe = "D:\UE5Projects\RTS_Survival\Content\RTS_Survival\WorldCampaign\MapObjects\Missions\Story\Berlin\BP_Mission_Berlin.uasset"

function Get-PieRunning {
    # Read-only get_pie_status over the editor's MCP endpoint. The server's chunked replies are malformed, so this
    # talks plain TCP (see Source/AGENTS.md). Returns $true, $false, or $null when the editor does not answer.
    try {
        $token = (Get-Content "$env:LOCALAPPDATA\UECP\session_token.txt" -Raw).Trim()
        $body = '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"call_action","arguments":{"action":"get_pie_status","params":{}}}}'
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($body)
        $head = "POST /uecp/RTS_Survival/mcp HTTP/1.1`r`nHost: 127.0.0.1:9855`r`nAuthorization: Bearer $token`r`n" +
            "Content-Type: application/json`r`nAccept: application/json, text/event-stream`r`n" +
            "Content-Length: $($bytes.Length)`r`nConnection: close`r`n`r`n"
        $client = New-Object System.Net.Sockets.TcpClient
        $client.ReceiveTimeout = 15000
        $client.Connect("127.0.0.1", 9855)
        $stream = $client.GetStream()
        $headBytes = [System.Text.Encoding]::ASCII.GetBytes($head)
        $stream.Write($headBytes, 0, $headBytes.Length)
        $stream.Write($bytes, 0, $bytes.Length)
        $reader = New-Object System.IO.StreamReader($stream)
        $reply = $reader.ReadToEnd()
        $client.Close()
        if ($reply -match 'pie_running\\?"\s*:\s*true') { return $true }
        if ($reply -match 'pie_running\\?"\s*:\s*false') { return $false }
        return $null
    } catch { return $null }
}

$deadline = (Get-Date).AddMinutes($Minutes)
$idlePolls = 0
while ($true) {
    $locked = $false
    try { $stream = [System.IO.File]::Open($probe, 'Open', 'ReadWrite', 'None'); $stream.Close() } catch { $locked = $true }
    if (-not $locked) { "files unlocked at $(Get-Date -Format HH:mm:ss)"; exit 0 }
    $pie = Get-PieRunning
    if ($Once) { "locked=$locked pie_running=$pie"; exit 0 }
    if ($pie -eq $false) { $idlePolls++ } else { $idlePolls = 0 }
    if ($idlePolls -ge 3) { "editor open, not playing, at $(Get-Date -Format HH:mm:ss)"; exit 3 }
    if ((Get-Date) -gt $deadline) { "still busy after $Minutes minutes (locked $locked, playing $pie)"; exit 2 }
    Start-Sleep -Seconds 30
}
