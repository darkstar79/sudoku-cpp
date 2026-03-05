@echo off
REM Run all tests on Windows

echo ==================================================
echo Running All Tests
echo ==================================================

echo Running Unit Tests...
build\Release\bin\tests\unit_tests.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Unit tests failed
    exit /b %ERRORLEVEL%
)

echo.
echo Running Integration Tests...
build\Release\bin\tests\integration_tests.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Integration tests failed
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo All tests passed!
echo ==================================================
