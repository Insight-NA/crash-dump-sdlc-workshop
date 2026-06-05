if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
cmake --preset windows-msvc
cmake --build build --target ea-sandbox
