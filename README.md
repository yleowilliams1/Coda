# Coda

## Gild

Gild is my utils library. It's just included in file form here, it has some special compilation steps which I'll quickly list below.

Gild requires the following:
- C11 

#### Windows
- VS 2019 16.8+ for windows
- /utf-8 
- fopen/sscanf/strerror are deprecated so we compile with '_CRT_SECURE_NO_WARNINGS'
- For none MSVC/MinGW we do "_USE_MINGW_ANSI_STDIO=1" for printf(%zu)

