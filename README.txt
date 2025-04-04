Assignment 4 – Multiplayer Asteroids Game
Developed by:
Irwen Yap Zi Yang
Thang Weng Khong
Tan Yong Chin
Wilmer Lee Jun Rong

How to Start the Game
Hosting (Server):
Launch the application.
Enter a desired port number (e.g., 5000) in the text field.
Click "Host" to start the server and begin listening for client connections.

Joining (Client):
On client machines, launch the same application.
Input the IP address of the host (e.g., 192.168.1.2) and the same port number used by the host.
Click "Connect" to join the server.

Starting the Game:
Once all players are connected, the host should click "Start Game" to begin the multiplayer session.
All players will then be spawned and the game will synchronize across all screens

Gameplay Controls:
W / A / S / D – Move your spaceship (Up / Left / Down / Right)
Spacebar – Fire bullets

Game Features:
Real-time movement and bullet synchronization
Host-spawned asteroid events propagated using event IDs and ACKs
Deterministic collision detection and score updates
Reconnection and heartbeat detection system for network durability
End-of-game scoreboard showing synchronized player scores