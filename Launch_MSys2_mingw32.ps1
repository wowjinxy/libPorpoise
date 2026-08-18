param (
	[string]$command
)

set MSYSTEM=MINGW
$msys2pwd=.\msys2\usr\bin\bash.exe -c "pwd"
if ($command) {
	.\msys2\usr\bin\bash.exe --rcfile "$msys2pwd/standalone/scripts/env_vars_mingw32.sh" -ci "$command"
} else {
	.\msys2\usr\bin\bash.exe --rcfile "$msys2pwd/standalone/scripts/env_vars_mingw32.sh"
}
