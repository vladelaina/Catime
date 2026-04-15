param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ProjectPath)) {
    Write-Output "::warning::VCXPROJ file not found: $ProjectPath"
    exit 0
}

function Write-DebugLine {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,

        [switch]$Annotate
    )

    $line = "VCXPROJ_DEBUG $Message"
    Write-Output "COPYABLE $line"
    Write-Output $line
    if ($Annotate) {
        $escaped = $line.Replace("%", "%25").Replace("`r", "%0D").Replace("`n", "%0A")
        Write-Output "::notice::$escaped"
    }
}

try {
    [xml]$xml = Get-Content -Path $ProjectPath -Raw
} catch {
    Write-Output "::warning::Failed to parse VCXPROJ XML: $($_.Exception.Message)"
    Get-Content -Path $ProjectPath | Select-Object -First 80 | ForEach-Object { Write-DebugLine $_ }
    exit 0
}

$ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
$ns.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

Write-Output "::notice::Inspecting ResourceCompile settings from $(Split-Path $ProjectPath -Leaf)"
Write-DebugLine "Project=$ProjectPath" -Annotate

$globals = $xml.SelectSingleNode("//msb:PropertyGroup[@Label='Globals']", $ns)
if ($globals) {
    foreach ($name in @("Platform", "ProjectGuid", "Keyword", "WindowsTargetPlatformVersion")) {
        $node = $globals.SelectSingleNode("msb:$name", $ns)
        if ($node -and $node.InnerText) {
            Write-DebugLine "Global.$name=$($node.InnerText.Trim())" -Annotate
        }
    }
}

$resourceItems = @($xml.SelectNodes("//msb:ItemGroup/msb:ResourceCompile", $ns))
if ($resourceItems.Count -eq 0) {
    Write-DebugLine "No ResourceCompile items found"
} else {
    foreach ($item in $resourceItems) {
        $include = $item.GetAttribute("Include")
        if ($include) {
            Write-DebugLine "ResourceInclude=$include" -Annotate
        }
    }
}

$definitionGroups = @($xml.SelectNodes("//msb:ItemDefinitionGroup[msb:ResourceCompile]", $ns))
if ($definitionGroups.Count -eq 0) {
    Write-DebugLine "No ItemDefinitionGroup/ResourceCompile blocks found"
} else {
    foreach ($group in $definitionGroups) {
        $condition = $group.GetAttribute("Condition")
        if (-not $condition) {
            $condition = "<none>"
        }
        Write-DebugLine "ResourceDefinition.Condition=$condition" -Annotate
        $resourceCompile = $group.SelectSingleNode("msb:ResourceCompile", $ns)
        foreach ($child in $resourceCompile.ChildNodes) {
            if ($child.NodeType -eq [System.Xml.XmlNodeType]::Element) {
                $value = $child.InnerText.Trim()
                if ($value) {
                    $annotate = $child.LocalName -ne "AdditionalOptions"
                    Write-DebugLine "ResourceDefinition.$($child.LocalName)=$value" -Annotate:$annotate
                }
            }
        }
    }
}

$interestingRaw = Select-String -Path $ProjectPath -Pattern "<ResourceCompile|<AdditionalIncludeDirectories>|<PreprocessorDefinitions>|<AdditionalOptions>|<Culture>|<NullTerminateStrings>|<ResourceOutputFileName>" -Context 0,2
foreach ($match in $interestingRaw | Select-Object -First 40) {
    Write-DebugLine "Raw:$($match.Line.Trim())"
    foreach ($contextLine in $match.Context.PostContext) {
        if ($contextLine.Trim()) {
            Write-DebugLine "Raw:$($contextLine.Trim())"
        }
    }
}
