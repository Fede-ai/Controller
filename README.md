<h1 align="center">🎮 Fede-ai - Controller 🎮</h1>

<h3>Project overview 🗺️ :</h3>

- This C++ project allows you to control another Windows computer wirelessly
- It works using TCP sockets and a server is needed to establish the connections
- The project consists of three applications:
  - A `victim` app (Windows only) that has to be opened on a computer for it to be controlled
  - A `controller` app (Windows only) that lets you see the list of victims connected to the server and choose one of them to control
  - The `server` (cross-platform) that makes the connection possible
- The victim application is almost invisible to the user and automatically runs on startup

 <h3>Libraries needed 📖 :</h3>

 - This project relies on two different libraries:
   - Mlib version `1.2.0` (my personal library - https://github.com/Fede-ai/Mlib/releases/tag/v1.2.0)
   - SFML version `2.6.1` (https://www.sfml-dev.org/download/sfml/2.6.1/)
 - For convenience, the default libraries path is set to `C:/` (it can be changed in the project's settings)

<h3>Getting started 🎬 :</h3>

 - Clone this project on your computer
 - Download the necessary libraries 
	- The default library and include path is set to `C:/` so if you install them in any other directory, 
you will need to adjust the paths from the projects' settings
 	- Remember to copy SFML's dlls to the release folder since it is linked dynamically
 - In order to compile the project, you need to add to the root folder a file named `secret.h`. In this file you **MUST** define the following macros:
	- `SERVER_IP` => a string containing the ip address of the server
	- `SERVER_PORT` => an integer which indicates the port to which the server will listen
	- `SERVER_FILES_PATH` => a string that tells the server where to create the files containing the logs/names database 
(make sure this directory already exists when the program is run)
	- `PASS` => a string containing the password that controllers can use to access admin commands
	- `CONTROLLER_PASS` & `VICTIM_PASS` => two strings (shouldn't be the same) that allow client applications to verify themselves to the server.
They are primarly used during development (so that older versions of the client aren't allowed access to the server)
 - Here is an example of what a `secret.h` file might look like:
```C++
#define SERVER_IP "123.123.123.123"
#define SERVER_PORT 8000
#define SERVER_FILES_PATH "./files"
#define PASS "SecretPassword"
#define CONTROLLER_PASS "c0.2.6"
#define VICTIM_PASS "v0.3.1"
```
 - You are all set and ready to build the project!

<h3>Main features 📝 :</h3>

<h3>Controller's console commands 💻 :</h3>

<h3>Controller's window commands 🎮 :</h3>
