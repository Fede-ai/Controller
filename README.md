<h1 align="center">🎮 Fede-ai - Controller 🎮</h1>

<h3>Project overview 🗺️ :</h3>

- This C++ project allows you to control another Windows computer wirelessly
- It works using TCP sockets and a running server is needed to establish the connection
- The project consists of three applications:
  - A "Victim" app that has to be opened on a computer for it to be controlled
  - A "Controller" app that lets you see the list of victims connected to the server and choose one of them to control
  - The server that makes the connection possible
- The victim application is almost invisible to the user and automatically runs on startup

 <h3>Libraries needed 📖 :</h3>

 - This project relies on two different libraries:
   - Mlib version 1.0.0 (my personal library - https://github.com/Fede-ai/My-library)
   - SFML version 2.6.1 (https://www.sfml-dev.org/download/sfml/2.6.0/)
 - For convenience, the default libraries path is set to C:/ (it can be changed in the project's settings)

<h3>Getting started 🎬 :</h3>

 - Download this project on your computer
 - Downloaded the necessary libraries 
	- The default library and include path is set to C:/ so if you install them in any other directory, you will need to adjust the paths
 	- Remember to copy SFML's dlls to the release folder since it is linked dynamically
 - For the project to run, you need to add to the root folder a file named "secret.h". In this file you must define:
	- A macro called "SERVER_IP" => a string containing the ip address of the server
	- A macro called "SERVER_PORT" => the port to which the server will listen, in a number format
 - You are all set and ready to run the project!

<h3>Main features 📝 :</h3>

<h3>Controller's console commands 💻 :</h3>

<h3>Controller's window commands 🎮 :</h3>
