# Function to get next available filename by finding the highest existing number
function Get-NextAvailableFilename {
    param (
        [string]$baseFilename = "read_all"
    )
    
    # Get all existing files matching the pattern
    $existingFiles = Get-ChildItem -Path "." -Filter "${baseFilename}_*.md"
    
    # Find the highest number
    $highestNum = 0
    foreach ($file in $existingFiles) {
        if ($file.Name -match "${baseFilename}_(\d+)\.md") {
            $num = [int]$matches[1]
            if ($num -gt $highestNum) {
                $highestNum = $num
            }
        }
    }
    
    # Use next number after the highest found
    $nextNum = $highestNum + 1
    return "${baseFilename}_${nextNum}.md"
}

# Get unique filename
$outputFile = Get-NextAvailableFilename

# Define excluded extensions
$excludedExtensions = @(
    ".json",
    ".jpg", ".jpeg", ".png", ".gif", ".bmp", 
    ".tiff", ".ico", ".svg", ".webp",
    ".md"
)

# Initialize empty string for content
$markdownContent = ""

# Get all files from specified directories
$files = Get-ChildItem -Path ".\" -File -Recurse | 
    Where-Object { $excludedExtensions -notcontains $_.Extension }

foreach ($file in $files) {
    # Create a visible separator with filename
    $separator = "_______________ filename: $($file.Name) _______________"
    $markdownContent += [char]10+$separator
    $markdownContent += [char]10+(Get-Content $file.FullName -Raw)
}

# Write content to file
$markdownContent | Out-File -FilePath $outputFile -Encoding UTF8

Write-Host "Files have been combined into $outputFile"
Write-Host "Excluded file types: $($excludedExtensions -join ', ')"
Write-Host "Total files processed: $($files.Count)"