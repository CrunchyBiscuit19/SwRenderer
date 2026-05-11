cd ..
Get-ChildItem src -Recurse -Include *.cpp,*.h | ForEach-Object { clang-format -i $_.FullName }