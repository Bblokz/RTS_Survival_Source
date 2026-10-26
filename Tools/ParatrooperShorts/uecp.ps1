param([string]$Method = 'tools/list', [string]$ToolName, [string]$ArgumentsJson = '{}', [string]$ArgumentsFile, [string]$PythonFile, [string]$OutputFile, [int]$Port = 30001)
$ErrorActionPreference = 'Stop'
if ($ArgumentsFile) { $ArgumentsJson = Get-Content -Raw -LiteralPath $ArgumentsFile }
if ($PythonFile) { $Method = 'tools/call'; $ToolName = 'execute_python'; $ArgumentsJson = @{code=(Get-Content -Raw -LiteralPath $PythonFile); description='Paratrooper meadow authoring and verification'} | ConvertTo-Json -Compress }
$RequestBody = @{jsonrpc='2.0'; id=1; method=$Method}
if ($Method -eq 'tools/call') { $RequestBody.params = @{name=$ToolName; arguments=($ArgumentsJson | ConvertFrom-Json -AsHashtable)} }
$BodyBytes = [Text.Encoding]::UTF8.GetBytes(($RequestBody | ConvertTo-Json -Depth 80 -Compress))
$SessionToken = (Get-Content -Raw -LiteralPath 'C:/Users/basbl/AppData/Local/UECP/session_token.txt').Trim()
$Client = [Net.Sockets.TcpClient]::new()
$Client.Connect('127.0.0.1',$Port)
$Stream = $Client.GetStream()
$Stream.ReadTimeout = 120000
$Header = "POST /mcp HTTP/1.1`r`nHost: 127.0.0.1:$Port`r`nAuthorization: Bearer $SessionToken`r`nContent-Type: application/json`r`nContent-Length: $($BodyBytes.Length)`r`nConnection: close`r`n`r`n"
$HeaderBytes = [Text.Encoding]::UTF8.GetBytes($Header)
$Stream.Write($HeaderBytes,0,$HeaderBytes.Length)
$Stream.Write($BodyBytes,0,$BodyBytes.Length)
$Reader = [IO.StreamReader]::new($Stream,[Text.Encoding]::UTF8)
try { $Reply = $Reader.ReadToEnd() } finally { $Client.Dispose() }
$ReplyBody = ($Reply -split "`r`n`r`n",2)[1]
if ($ReplyBody.StartsWith('data:')) { $ReplyBody = (($ReplyBody -split "`n" | Where-Object { $_.StartsWith('data:') }) -replace '^data:\s*','') -join "`n" }
$Response = $ReplyBody | ConvertFrom-Json
if ($Response.result.content) {
    $ResultText = ($Response.result.content | Where-Object type -eq 'text' | ForEach-Object { $_.text }) -join "`n"
    if ($OutputFile) { Set-Content -LiteralPath $OutputFile -Value $ResultText -Encoding utf8 }
    try { $ToolResult = $ResultText | ConvertFrom-Json; if ($ToolResult.ok -eq $false) { $ResultText } elseif ($ToolResult.output_text) { $ToolResult.output_text } else { $ResultText } } catch { $ResultText }
}
elseif ($Response.result.tools) { $Response.result.tools | Select-Object name,description | ConvertTo-Json -Depth 5 }
else { $ReplyBody }

